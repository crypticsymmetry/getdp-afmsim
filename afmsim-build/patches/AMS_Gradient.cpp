// AMS_Gradient.cpp -- hand hypre's AMS the discrete gradient of a GetDP
// edge-element system (B2 of the Axial Flux sim's GETDP_PERFORMANCE_ROADMAP).
//
// Not part of upstream GetDP. tools/getdp_real_build/3_getdp_build.sh copies
// it into src/kernel and applies the small hook in LinAlg_PETSC.cpp that
// calls GetDP_AMS_Attach when the preconditioner is "-pc_type hypre
// -pc_hypre_type ams".
//
// AMS needs, beyond the matrix:
//
//  * G, the discrete gradient: one row per equation, and for a lowest-order
//    edge (Whitney, BF_Edge) unknown from node a to node b, -1 at a and +1
//    at b, so that G phi is the edge-element representation of grad phi.
//    GetDP stores a global edge as {+a, -b} in EdgesXNodes: the positive
//    entry is the tail, the negative one the head (Den_Line = {1, -2} is
//    "nodes 1 -> 2", expanded {-1, +1}).
//  * the edge constant vectors G x, G y, G z: each edge's own head minus
//    tail coordinates.
//
// Periodic (Link) edges are not equations, but an unknown edge that touches
// a slave face node still has that node in its G row. The nodal space AMS
// builds on must be periodic too, so a slave node is merged into its master
// with the link coefficient (-1 for an anti-periodic sector). The node map is
// read off the edge links: a slave node's master is the node common to the
// master edges of the slave edges that meet there, ties broken by (r, z),
// which a rotation about z preserves.
//
// With -getdp_ams_check, prints |A G phi| / |A u| for phi random on the
// interior nodes (zero on nodes of Dirichlet edges) and u random: ~1e-15 if
// G spans the matrix's gradient kernel, O(1) if a sign or a link is wrong.

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

#include "GetDPConfig.h"
#include "ProData.h"
#include "DofData.h"
#include "GeoData.h"
#include "ListUtils.h"
#include "TreeUtils.h"
#include "Message.h"
#include "BF.h"

#if defined(HAVE_PETSC)
#include <petsc.h>
#endif

#if defined(HAVE_PETSC) && defined(PETSC_HAVE_HYPRE)

extern struct Problem Problem_S;
extern struct CurrentData Current;

namespace {

struct AMSData {
  Mat G = nullptr;
  Vec ex = nullptr, ey = nullptr, ez = nullptr;
  std::vector<int> fixedCols; // columns of nodes on Dirichlet edges
  bool ok = false;
};

std::map<const void *, AMSData> gCache;
// The matrix (object id, state) each PC was last set up for. The id, since
// GetDP assembles each Jacobian into a new Mat whose state count comes out
// the same every time.
std::map<const void *, std::pair<PetscInt64, PetscObjectState> > gAttached;

struct NodeMap {
  int master;
  double sign;
};

bool edgeBasis(struct BasisFunction *bf)
{
  return bf->Function == (void (*)())BF_Edge;
}

AMSData build(struct DofData *dd, struct GeoData *gd)
{
  AMSData out;
  // 1. Which basis-function codes are lowest-order edge functions.
  std::set<int> edgeCodes;
  for(int i = 0; i < List_Nbr(dd->FunctionSpaceIndex); i++) {
    int fsi = *(int *)List_Pointer(dd->FunctionSpaceIndex, i);
    struct FunctionSpace *fs =
      (struct FunctionSpace *)List_Pointer(Problem_S.FunctionSpace, fsi);
    for(int j = 0; j < List_Nbr(fs->BasisFunction); j++) {
      struct BasisFunction *bf =
        (struct BasisFunction *)List_Pointer(fs->BasisFunction, j);
      if(edgeBasis(bf)) edgeCodes.insert(bf->Num);
    }
  }
  if(edgeCodes.empty()) {
    Message::Error("AMS: the system has no BF_Edge basis function");
    return out;
  }

  // 2. Edge number -> (tail, head).
  std::map<int, std::pair<int, int> > edges;
  List_T *el = Tree2List(gd->EdgesXNodes);
  for(int i = 0; i < List_Nbr(el); i++) {
    struct Entity2XEntity1 *e = (struct Entity2XEntity1 *)List_Pointer(el, i);
    if(e->NbrEntities != 2) continue;
    edges[e->Num] =
      std::make_pair(std::abs(e->NumEntities[0]), std::abs(e->NumEntities[1]));
  }
  List_Delete(el);

  // 3. Periodic node map from the edge links.
  std::map<int, std::set<int> > cand;
  std::map<int, double> linkSign;
  int nLinks = 0;
  for(int i = 0; i < List_Nbr(dd->DofList); i++) {
    struct Dof *d = (struct Dof *)List_Pointer(dd->DofList, i);
    if(d->Type != DOF_LINK || !edgeCodes.count(d->NumType)) continue;
    auto s = edges.find(d->Entity);
    auto m = edges.find(d->Case.Link.EntityRef);
    if(s == edges.end() || m == edges.end()) continue;
    nLinks++;
    std::set<int> mn = {m->second.first, m->second.second};
    for(int sn : {s->second.first, s->second.second}) {
      auto it = cand.find(sn);
      if(it == cand.end())
        cand[sn] = mn;
      else {
        std::set<int> keep;
        for(int x : it->second)
          if(mn.count(x)) keep.insert(x);
        if(!keep.empty()) it->second = keep;
      }
      linkSign[sn] = d->Case.Link.Coef;
    }
  }
  std::map<int, NodeMap> slave;
  int ties = 0;
  for(auto &c : cand) {
    int pick = *c.second.begin();
    if(c.second.size() > 1) {
      ties++;
      struct Geo_Node *s = Geo_GetGeoNodeOfNum(c.first);
      double rs = std::hypot(s->x, s->y), best = 1e300;
      for(int m : c.second) {
        struct Geo_Node *g = Geo_GetGeoNodeOfNum(m);
        double dist = std::fabs(std::hypot(g->x, g->y) - rs) + std::fabs(g->z - s->z);
        if(dist < best) { best = dist; pick = m; }
      }
    }
    if(pick != c.first) slave[c.first] = {pick, linkSign[c.first]};
  }
  auto resolve = [&](int n, double &sgn) {
    sgn = 1.0;
    for(int hop = 0; hop < 8; hop++) {
      auto it = slave.find(n);
      if(it == slave.end()) break;
      sgn *= it->second.sign;
      n = it->second.master;
    }
    return n;
  };

  // 4. Rows, columns, and the Dirichlet nodes for the check.
  int n = dd->NbrDof;
  std::map<int, int> col;
  std::vector<PetscInt> ri, ci;
  std::vector<PetscScalar> vv;
  std::vector<double> ex(n, 0.), ey(n, 0.), ez(n, 0.);
  int nonEdge = 0, rows = 0;
  std::set<int> fixedNodes;
  for(int i = 0; i < List_Nbr(dd->DofList); i++) {
    struct Dof *d = (struct Dof *)List_Pointer(dd->DofList, i);
    bool isEdge = edgeCodes.count(d->NumType) > 0;
    if(d->Type == DOF_FIXED && isEdge) {
      auto e = edges.find(d->Entity);
      if(e != edges.end()) {
        fixedNodes.insert(e->second.first);
        fixedNodes.insert(e->second.second);
      }
      continue;
    }
    if(d->Type != DOF_UNKNOWN) continue;
    if(!isEdge) { nonEdge++; continue; }
    auto e = edges.find(d->Entity);
    if(e == edges.end()) { nonEdge++; continue; }
    int row = d->Case.Unknown.NumDof - 1;
    int ends[2] = {e->second.first, e->second.second};
    double coef[2] = {-1.0, 1.0};
    for(int k = 0; k < 2; k++) {
      double sgn;
      int m = resolve(ends[k], sgn);
      auto c = col.find(m);
      int cidx = (c == col.end()) ? (col[m] = (int)col.size()) : c->second;
      ri.push_back(row);
      ci.push_back(cidx);
      vv.push_back(coef[k] * sgn);
    }
    struct Geo_Node *a = Geo_GetGeoNodeOfNum(ends[0]);
    struct Geo_Node *b = Geo_GetGeoNodeOfNum(ends[1]);
    ex[row] = b->x - a->x;
    ey[row] = b->y - a->y;
    ez[row] = b->z - a->z;
    rows++;
  }
  for(int fn : fixedNodes) {
    double sgn;
    auto c = col.find(resolve(fn, sgn));
    if(c != col.end()) out.fixedCols.push_back(c->second);
  }
  Message::Info("AMS: discrete gradient %d x %d (%d edge rows, %d other "
                "unknowns, %d periodic edge links, %d slave nodes, %d ties)",
                n, (int)col.size(), rows, nonEdge, nLinks, (int)slave.size(),
                ties);
  if(nonEdge)
    Message::Warning("AMS: %d unknowns are not lowest-order edge DOFs; their "
                     "gradient rows are empty and AMS will converge poorly",
                     nonEdge);

  MatCreateSeqAIJ(PETSC_COMM_SELF, n, (PetscInt)col.size(), 2, NULL, &out.G);
  for(size_t k = 0; k < vv.size(); k++)
    MatSetValue(out.G, ri[k], ci[k], vv[k], ADD_VALUES);
  MatAssemblyBegin(out.G, MAT_FINAL_ASSEMBLY);
  MatAssemblyEnd(out.G, MAT_FINAL_ASSEMBLY);

  Vec *vs[3] = {&out.ex, &out.ey, &out.ez};
  std::vector<double> *src[3] = {&ex, &ey, &ez};
  for(int k = 0; k < 3; k++) {
    VecCreateSeq(PETSC_COMM_SELF, n, vs[k]);
    for(int r = 0; r < n; r++) VecSetValue(*vs[k], r, (*src[k])[r], INSERT_VALUES);
    VecAssemblyBegin(*vs[k]);
    VecAssemblyEnd(*vs[k]);
  }
  out.ok = true;
  return out;
}

void check(const AMSData &d, Mat A)
{
  PetscInt m, nc;
  MatGetSize(d.G, &m, &nc);
  Vec phi, gphi, agphi, u, au;
  VecCreateSeq(PETSC_COMM_SELF, nc, &phi);
  PetscRandom rnd;
  PetscRandomCreate(PETSC_COMM_SELF, &rnd);
  PetscRandomSetFromOptions(rnd);
  VecSetRandom(phi, rnd);
  for(int c : d.fixedCols) VecSetValue(phi, c, 0.0, INSERT_VALUES);
  VecAssemblyBegin(phi);
  VecAssemblyEnd(phi);
  MatCreateVecs(A, &u, &au);
  VecDuplicate(u, &gphi);
  VecDuplicate(u, &agphi);
  MatMult(d.G, phi, gphi);
  MatMult(A, gphi, agphi);
  VecSetRandom(u, rnd);
  PetscReal ng, nag, nu, nau;
  VecNorm(gphi, NORM_2, &ng);
  MatMult(A, u, au);
  VecNorm(agphi, NORM_2, &nag);
  VecNorm(u, NORM_2, &nu);
  VecNorm(au, NORM_2, &nau);
  Message::Info("AMS check: |A G phi|/|G phi| = %.3e against |A u|/|u| = %.3e "
                "(ratio %.3e; ~1e-14 means G spans the gradient kernel)",
                (double)(nag / ng), (double)(nau / nu),
                (double)((nag / ng) / (nau / nu)));
  VecDestroy(&phi);
  VecDestroy(&gphi);
  VecDestroy(&agphi);
  VecDestroy(&u);
  VecDestroy(&au);
  PetscRandomDestroy(&rnd);
}

} // namespace

// Called before every KSPSolve. hypre_AMSSetup allocates its auxiliary
// matrices and BoomerAMG solvers afresh on every call and frees none of the
// previous ones, and PETSc calls it again on the same hypre object whenever
// the matrix changes: ~20 MB lost per Newton step on a 239k-edge sector, 1.2 GB
// over an 8-angle sweep. So on a changed matrix the PC is rebuilt, which
// destroys the old AMS object (PCDestroy_HYPRE -> HYPRE_AMSDestroy), and G and
// the edge vectors are attached again. A setup is redone either way; a
// preconditioner kept on purpose (KSPSetReusePreconditioner) is left alone.
void GetDP_AMS_Attach(PC pc, Mat A)
{
  PetscObjectState st = 0;
  PetscObjectStateGet((PetscObject)A, &st);
  PetscInt64 id = 0;
  PetscObjectGetId((PetscObject)A, &id);
  const std::pair<PetscInt64, PetscObjectState> state(id, st);
  auto seen = gAttached.find(pc);
  const bool first = seen == gAttached.end();
  if(!first) {
    PetscBool reuse = PETSC_FALSE;
    PCGetReusePreconditioner(pc, &reuse);
    if(reuse || seen->second == state) return;
    PCSetType(pc, PCNONE);
    PCSetType(pc, PCHYPRE);
    PCHYPRESetType(pc, "ams");
    PCSetFromOptions(pc);
  }
  struct DofData *dd = Current.DofData;
  struct GeoData *gd = Current.GeoData;
  if(!dd || !gd) {
    Message::Error("AMS: no current system");
    return;
  }
  auto it = gCache.find(dd);
  if(it == gCache.end()) it = gCache.emplace(dd, build(dd, gd)).first;
  AMSData &d = it->second;
  if(!d.ok) return;
  PetscBool chk = PETSC_FALSE, set = PETSC_FALSE;
  PetscOptionsGetBool(NULL, NULL, "-getdp_ams_check", &chk, &set);
  if(chk && first) check(d, A);
  PCHYPRESetDiscreteGradient(pc, d.G);
  PCHYPRESetEdgeConstantVectors(pc, d.ex, d.ey, d.ez);
  // Magnetostatics has no mass term: the curl-curl operator is singular, and
  // AMS is told so rather than handed a zero Poisson matrix.
  PetscBool beta0 = PETSC_TRUE;
  PetscOptionsGetBool(NULL, NULL, "-getdp_ams_singular", &beta0, &set);
  if(beta0) PCHYPRESetBetaPoissonMatrix(pc, NULL);
  gAttached[pc] = state;
  if(first)
    Message::Info("AMS: discrete gradient attached%s",
                  beta0 ? " (singular curl-curl, beta = 0)" : "");
}

#endif
