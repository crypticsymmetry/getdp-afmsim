// TermCache.cpp -- replay the element matrices of terms that do not change
// inside a Newton loop (5.12/5.13 of the Axial Flux sim's
// GETDP_PERFORMANCE_ROADMAP).
//
// Not part of upstream GetDP. tools/getdp_real_build/patches/apply_ams_patch.py
// copies it into src/kernel and adds three small hooks:
//
//  * Cal_GalerkinTermOfFemEquation asks GetDP_TermCache_Get for a term's
//    element matrix before integrating it, and hands a freshly integrated one
//    to GetDP_TermCache_Put;
//  * the IterativeLoop operation calls GetDP_TermCache_Begin on entry and
//    GetDP_TermCache_End on exit.
//
// A Newton loop re-assembles the whole system every iteration, although
// terms such as a linear reluctivity, an eps a.a' regularisation or a
// fixed source are the same every time. Which terms those are is the deck's
// statement, not GetDP's inference: a term takes part only when its
// integration method's name starts with "Cached" (an ordinary integration
// method to an unpatched GetDP, so such a deck runs anywhere). The cache
// lives only between the Begin and End of an IterativeLoop and is emptied at
// both, so a term need only be constant *within one loop* -- a rotor angle,
// a current or a continuation factor set between loops is fine; anything
// that reads the solution, the iteration or a runtime variable set inside the
// loop is not.
//
// The replayed matrix is the one integrated in the loop's first pass, so the
// assembly is the same to the bit. -getdp_no_term_cache turns it off.

#include <string.h>
#include <unordered_map>
#include <vector>

#include "GetDPConfig.h"
#include "ProData.h"
#include "ListUtils.h"
#include "Message.h"

#if defined(HAVE_PETSC)
#include <petsc.h>
#endif

extern struct Problem Problem_S;

namespace {

struct Entry {
  size_t offset;
  int nEqu, nDof, nHar;
};

struct TermStore {
  bool wanted;
  std::unordered_map<int, Entry> at; // element number -> stored matrix
  std::vector<double> val;
};

std::unordered_map<const void *, TermStore> gStore;
int gDepth = 0;
int gEnabled = -1;
size_t gHits = 0, gStored = 0;

bool enabled()
{
  if(gEnabled < 0) {
    gEnabled = 1;
#if defined(HAVE_PETSC)
    PetscBool off = PETSC_FALSE, set = PETSC_FALSE;
    PetscOptionsGetBool(NULL, NULL, "-getdp_no_term_cache", &off, &set);
    if(set && off) gEnabled = 0;
#endif
  }
  return gEnabled == 1;
}

TermStore *store(struct EquationTerm *term)
{
  if(gDepth <= 0 || !enabled()) return nullptr;
  auto it = gStore.find(term);
  if(it == gStore.end()) {
    TermStore s;
    struct IntegrationMethod *im =
      (struct IntegrationMethod *)List_Pointer(
        Problem_S.IntegrationMethod,
        term->Case.LocalTerm.IntegrationMethodIndex);
    s.wanted = im && im->Name && !strncmp(im->Name, "Cached", 6);
    it = gStore.emplace(term, std::move(s)).first;
  }
  return it->second.wanted ? &it->second : nullptr;
}

inline size_t idx(int i, int j, int k)
{
  return ((size_t)i * NBR_MAX_BASISFUNCTIONS + j) * NBR_MAX_HARMONIC + k;
}

} // namespace

void GetDP_TermCache_Begin()
{
  gDepth++;
  gStore.clear();
  gHits = gStored = 0;
}

void GetDP_TermCache_End()
{
  if(gHits)
    Message::Info("Term cache: %lu element terms integrated, %lu replayed",
                  (unsigned long)gStored, (unsigned long)gHits);
  if(gDepth > 0) gDepth--;
  gStore.clear();
  gHits = gStored = 0;
}

// Whether a term's element matrix was stored in this loop; if so, copy it
// into Ek (laid out as double[NBR_MAX_BASISFUNCTIONS][..][NBR_MAX_HARMONIC]).
bool GetDP_TermCache_Get(struct EquationTerm *term, int element, int nEqu,
                         int nDof, int nHar, double *Ek)
{
  TermStore *s = store(term);
  if(!s) return false;
  auto it = s->at.find(element);
  if(it == s->at.end()) return false;
  const Entry &e = it->second;
  if(e.nEqu != nEqu || e.nDof != nDof || e.nHar != nHar) return false;
  const double *v = &s->val[e.offset];
  for(int i = 0; i < nEqu; i++)
    for(int j = 0; j < nDof; j++)
      for(int k = 0; k < nHar; k++) Ek[idx(i, j, k)] = *v++;
  gHits++;
  return true;
}

void GetDP_TermCache_Put(struct EquationTerm *term, int element, int nEqu,
                         int nDof, int nHar, const double *Ek)
{
  TermStore *s = store(term);
  if(!s) return;
  Entry e = {s->val.size(), nEqu, nDof, nHar};
  for(int i = 0; i < nEqu; i++)
    for(int j = 0; j < nDof; j++)
      for(int k = 0; k < nHar; k++) s->val.push_back(Ek[idx(i, j, k)]);
  s->at[element] = e;
  gStored++;
}
