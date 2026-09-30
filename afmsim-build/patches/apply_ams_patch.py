"""
Apply the Axial Flux sim's GetDP patches to a source tree, idempotently.

    python apply_ams_patch.py "E:/Program Files/getdp-git-source"

B2, hypre AMS:

* copies AMS_Gradient.cpp into src/kernel and adds it to CMakeLists.txt;
* inserts, in src/kernel/LinAlg_PETSC.cpp, a call to GetDP_AMS_Attach once
  the KSP's options are read and the preconditioner is hypre AMS.

5.13, the term cache:

* copies TermCache.cpp into src/kernel and adds it to CMakeLists.txt;
* in Cal_GalerkinTermOfFemEquation.cpp, replays a cached element matrix
  before integrating a term and stores a freshly integrated one;
* in SolvingOperations.cpp, opens the cache on entry to an IterativeLoop
  and empties it on exit.

Every edited file is backed up once as <name>.orig. Running it again changes
nothing. The AMS hook and file compile to nothing on a PETSc without hypre, so
the patched tree builds every arch.
"""

from __future__ import annotations

import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MARK = "GetDP_AMS_Attach"

DECL_ANCHOR = "extern struct CurrentData Current;\n\n#if defined(HAVE_PETSC)\n"
DECL = ("extern struct CurrentData Current;\n\n#if defined(HAVE_PETSC)\n\n"
        "#if defined(PETSC_HAVE_HYPRE)\n"
        "// B2 (tools/getdp_real_build/patches): hypre AMS needs the discrete\n"
        "// gradient, which only the DOF and edge data can provide.\n"
        "void GetDP_AMS_Attach(PC pc, Mat A);\n"
        "#endif\n")

HOOK_ANCHOR = ("  Message::Info(\"N: %ld - %s %s %s\", long(i), ksptype, pctype, stype);\n")
HOOK = ("#if defined(PETSC_HAVE_HYPRE)\n"
        "  if(pctype && !strcmp(pctype, PCHYPRE)) {\n"
        "    const char *htype = NULL;\n"
        "    _try(PCHYPREGetType(pc, &htype));\n"
        "    if(htype && !strcmp(htype, \"ams\")) GetDP_AMS_Attach(pc, A->M);\n"
        "  }\n"
        "#endif\n") + HOOK_ANCHOR

CACHE_MARK = "GetDP_TermCache_"

CACHE_DECL_ANCHOR = "extern struct Problem Problem_S;\nextern struct CurrentData Current;\n"
GALERKIN_DECL = CACHE_DECL_ANCHOR + (
    "\n// Axial Flux sim patch (TermCache.cpp): terms constant within a Newton\n"
    "// loop replay their element matrices.\n"
    "bool GetDP_TermCache_Get(struct EquationTerm *term, int element, int nEqu,\n"
    "                         int nDof, int nHar, double *Ek);\n"
    "void GetDP_TermCache_Put(struct EquationTerm *term, int element, int nEqu,\n"
    "                         int nDof, int nHar, const double *Ek);\n")
LOOP_DECL = CACHE_DECL_ANCHOR + (
    "\n// Axial Flux sim patch (TermCache.cpp): the cache lives for one\n"
    "// IterativeLoop.\n"
    "void GetDP_TermCache_Begin();\n"
    "void GetDP_TermCache_End();\n")

GET_ANCHOR = "    case NODOF: Nbr_Dof = 1; break;\n    }\n  }\n\n"
GET_HOOK = GET_ANCHOR + (
    "  // Axial Flux sim patch: a term the deck marks constant within this\n"
    "  // IterativeLoop replays its element matrix (TermCache.cpp).\n"
    "  const bool termCache = FI->Type_DefineQuantityDof != INTEGRALQUANTITY &&\n"
    "                         Current.TypeAssembly != ASSEMBLY_SPARSITY_PATTERN;\n"
    "  if(termCache && GetDP_TermCache_Get(EquationTerm_P, Element->Num, Nbr_Equ,\n"
    "                                      Nbr_Dof, Current.NbrHar, &Ek[0][0][0])) {\n"
    "    for(i = 0; i < Nbr_Equ; i++)\n"
    "      for(j = 0; j < Nbr_Dof; j++)\n"
    "        ((void (*)(struct Dof *, struct Dof *,\n"
    "                   double *))FI->Function_AssembleTerm)(\n"
    "          QuantityStorageEqu_P->BasisFunction[i].Dof,\n"
    "          QuantityStorageDof_P->BasisFunction[j].Dof, Ek[i][j]);\n"
    "    Current.flagAssDiag = 0;\n"
    "    return;\n"
    "  }\n\n")
PUT_ANCHOR = "    if(Message::GetVerbosity() == 10) {\n      printf(\"Galerkin = \");\n"
PUT_HOOK = (
    "    if(termCache && !Current.flagAssDiag)\n"
    "      GetDP_TermCache_Put(EquationTerm_P, Element->Num, Nbr_Equ, Nbr_Dof,\n"
    "                          Current.NbrHar, &Ek[0][0][0]);\n\n") + PUT_ANCHOR
BEGIN_ANCHOR = ("      Message::Info(\"IterativeLoop ...\");\n\n"
                "      Save_Iteration = Current.Iteration;\n")
BEGIN_HOOK = BEGIN_ANCHOR + "      GetDP_TermCache_Begin();\n"
END_ANCHOR = ("      Current.Iteration = Save_Iteration;\n      break;\n\n"
              "    case OPERATION_ITERATIVELINEARSOLVER:\n")
END_HOOK = ("      Current.Iteration = Save_Iteration;\n      GetDP_TermCache_End();\n"
            "      break;\n\n    case OPERATION_ITERATIVELINEARSOLVER:\n")


def backup(p: Path) -> None:
    orig = p.with_name(p.name + ".orig")
    if not orig.exists():
        shutil.copy2(p, orig)


def edit(p: Path, mark: str, pairs) -> None:
    """Replace each anchor once, keeping the file's line endings."""
    raw = p.read_bytes().decode("utf-8")
    if mark in raw:
        return
    backup(p)
    crlf = "\r\n" in raw
    t = raw.replace("\r\n", "\n")
    for a, b in pairs:
        assert t.count(a) == 1, f"{p.name}: anchor not found: {a[:60]!r}"
        t = t.replace(a, b)
    if crlf:
        t = t.replace("\n", "\r\n")
    p.write_bytes(t.encode("utf-8"))


def main(src_root: str) -> None:
    kernel = Path(src_root) / "src" / "kernel"
    shutil.copy2(HERE / "AMS_Gradient.cpp", kernel / "AMS_Gradient.cpp")
    shutil.copy2(HERE / "TermCache.cpp", kernel / "TermCache.cpp")

    cm = kernel / "CMakeLists.txt"
    t = cm.read_text(encoding="utf-8")
    a = "      LinAlg.cpp LinAlg_PETSC.cpp LinAlg_SPARSKIT.cpp\n"
    changed = False
    for new in ("AMS_Gradient.cpp", "TermCache.cpp"):
        if new not in t:
            backup(cm)
            assert t.count(a) == 1, "CMakeLists anchor not found"
            t = t.replace(a, a + f"      {new}\n")
            changed = True
    if changed:
        cm.write_text(t, encoding="utf-8", newline="\n")

    edit(kernel / "LinAlg_PETSC.cpp", MARK,
         [(DECL_ANCHOR, DECL), (HOOK_ANCHOR, HOOK)])
    edit(kernel / "Cal_GalerkinTermOfFemEquation.cpp", CACHE_MARK,
         [(CACHE_DECL_ANCHOR, GALERKIN_DECL), (GET_ANCHOR, GET_HOOK),
          (PUT_ANCHOR, PUT_HOOK)])
    edit(kernel / "SolvingOperations.cpp", CACHE_MARK,
         [(CACHE_DECL_ANCHOR, LOOP_DECL), (BEGIN_ANCHOR, BEGIN_HOOK),
          (END_ANCHOR, END_HOOK)])
    print("AMS and term-cache patches applied to", kernel)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else r"E:\Program Files\getdp-git-source")
