# GetDP, as built for the Axial Flux simulator

This is [GetDP](https://getdp.info) 4.0.0-git (GPL-2.0-or-later, by Patrick Dular and Christophe
Geuzaine) with two patches. It's the solver the Axial Flux simulator (afmsim) runs as a separate
program, and it's published here as the complete corresponding source of the `getdp.exe` it uses.

## What is here

- **The upstream source, unmodified**: the first commit. It's a snapshot without git metadata. Its
  content hash, as `afmsim-build/4_provenance.py` computes it, is
  `5620700af9d4c2c8012d68dfc56330828540d706f66b0d7efe5ee1078b9a0499` (432 files).
- **The modifications**: the second commit. They're made by `afmsim-build/patches/apply_ams_patch.py`,
  and each changed or added file says so in its header comment:
  - `src/kernel/AMS_Gradient.cpp` (new), plus a hook in `LinAlg_PETSC.cpp`. It gives hypre's AMS
    preconditioner the discrete gradient and edge vectors, so 3-D magnetostatic solves run as
    CG + AMS. It compiles to nothing on a PETSc without hypre.
    - `-getdp_ams_check` prints `|A·G·φ|`.
    - `-getdp_ams_singular false` keeps AMS out of its β = 0 mode.
  - `src/kernel/TermCache.cpp` (new), plus hooks in `Cal_GalerkinTermOfFemEquation.cpp` and
    `SolvingOperations.cpp`. A term whose integration method's name starts with `Cached` is
    integrated once per `IterativeLoop`, and its element matrices are replayed afterwards; the
    results are bit-identical. `-getdp_no_term_cache` turns it off. To an unpatched GetDP,
    `CachedInt` is just `Int`.
  - `src/kernel/CMakeLists.txt` compiles the two new files.
- **How the binary is built**: `afmsim-build/`, with MSYS2 MinGW-w64 on Windows.
  - `BUILD.md` is the procedure. `0_…` to `3_…` are the scripts. It builds a static Gmsh library,
    then a real-arithmetic PETSc with MUMPS, METIS and hypre, then GetDP.
  - `provenance.json` records the exact versions and configure line, the toolchain, and the
    SHA-256 of every script, patch and the binary. The scripts use the author's paths
    (`E:\petsc-real`); adjust them to yours.

## The binary

`getdp.exe` (Windows x64, statically linked) is attached to the
[releases](../../releases) with its source. Its SHA-256 is
`77f6b20ad6075939152798c300560cde4c94542036bab336f6b5d6a1dac5305a`. Everything linked into it is
listed below.

| Component | Version | Licence | Source |
|---|---|---|---|
| GetDP (this repository) | 4.0.0-git + patches | GPL-2.0-or-later | here |
| Gmsh library | 4.15.2 | GPL-2.0-or-later, with linking exception | `gmsh-4.15.2-source.tgz` in the release (from gmsh.info) |
| PETSc | 3.25.5 | BSD-2-Clause | petsc.org |
| hypre | 3.1.0 | Apache-2.0 or MIT | github.com/hypre-space/hypre |
| MUMPS | 5.8.2 | CeCILL-C | mumps-solver.org |
| METIS | 5.1.0 | Apache-2.0 | github.com/KarypisLab/METIS |
| OpenBLAS | 0.3.30 | BSD-3-Clause | github.com/OpenMathLib/OpenBLAS |
| GCC runtime (libgfortran, libgomp, libstdc++, libquadmath) | 15.2.0 | GPL-3.0 with the GCC Runtime Library Exception | gcc.gnu.org |
| MinGW-w64 runtime and winpthreads | MSYS2 | ZPL / MIT-style | mingw-w64.org |

Their licence texts are in `afmsim-build/third-party-licenses/`.

## Licence

GetDP is free software under the GNU General Public License, version 2 or (at your option) any
later version; see `LICENSE.txt`. The modifications here are released under the same terms.
There is no warranty.

Upstream GetDP is at https://getdp.info and https://gitlab.onelab.info/getdp/getdp. Report problems
with these patches here, not upstream.
