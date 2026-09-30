# GetDP with real-arithmetic PETSc (B1)

How `E:\Program Files\getdp_real\getdp.exe` was built on 2026-09-23/24, and its OpenMP variant
`E:\Program Files\getdp_real_omp\getdp.exe` (B1b), which is now the default GetDP
(`config/paths.py`). The results are in `Docs/GETDP_PERFORMANCE_ROADMAP.md` §5.5 and §5.7.

## Prerequisites (MSYS2, `C:\msys64`)

```
pacman -S --needed mingw-w64-x86_64-gcc-fortran mingw-w64-x86_64-python mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-openblas mingw-w64-x86_64-metis mingw-w64-x86_64-ninja \
  python diffutils patch unzip tar
```

PETSc's configure step needs the MSYS (POSIX) `python` and `diff`; the MinGW Python is refused
("Windows python detected").

## Steps

The PETSc tree must be on a path without spaces, so it is `E:\petsc-real`. Get
`petsc-3.25.5.tar.gz` from petsc.org and `gmsh-4.15.2-source.tgz` from gmsh.info and unpack both
there. Then run, from an MSYS2 shell:

0. `0_gmsh_lib.sh` — a minimal static Gmsh library (parser, post-processing, ANN, Eigen) with the
   private API, whose headers (`gmsh/PView.h`, `GmshGlobal.h`, `BasisFactory.h`) GetDP includes.
   Plugins are off: GetDP never calls them, and in 4.15 the BoundaryLayer plugin needs the mesh
   module. Installs to `E:\petsc-real\gmsh-install`.
1. `1_petsc_configure.sh` — real double, no MPI, static, `--download-mumps --download-metis`,
   OpenBLAS. It fetches MUMPS 5.8.2 and METIS 5.1.0.
2. `2_petsc_make.sh` — builds `libpetsc.a` (about 20 minutes), then runs step 3.
3. `3_getdp_build.sh` — GetDP from `E:\Program Files\getdp-git-source`, Ninja, static. Two
   workarounds:
   - PETSc's `petscvariables` records MSYS paths (`/e/...`), so the include and library
     directories are passed again in Windows form in the compiler and linker flags.
   - MSYS2's static OpenBLAS needs an OpenMP runtime; `-fopenmp -lgomp` supplies GCC's.
   - The static Gmsh library needs `-lws2_32 -lwsock32 -lwinmm -lshlwapi -lole32 -lpsapi`.

Copy `E:\petsc-real\getdp-build\getdp.exe` to `E:\Program Files\getdp_real\`.

4. `4_provenance.py --exe <getdp.exe> --out <provenance.json> [--src ...] [--arch ...]` records what
   the build is made of. The GetDP snapshot has no git metadata, so it is identified by a content
   hash of the pristine tree; that hash is the same before and after the patches are applied. The
   record also holds the patches and scripts by hash, the library versions, PETSc's configure line,
   the toolchain and the binary's SHA-256.

**The default build (2026-09-25)** lives in the project, as `third_party/getdp/getdp.exe` (not in
git), next to its `provenance.json`. It was made by copying the source to
`E:\petsc-real\getdp-src-clean`, restoring the `.orig` files there, and running step 3 on that copy:
`PETSC_ARCH=arch-mingw-real-omp-hypre GETDP_SRC=/e/petsc-real/getdp-src-clean
GETDP_BUILD=/e/petsc-real/getdp-build-clean`. It gives the same 108 KPIs as the incrementally built
binary it replaced. `E:\Program Files\getdp_real_omp_hypre\getdp.exe` is the same file, kept as the
fallback.

Steps 1–3 read `PETSC_ARCH` (default `arch-mingw-real`) and step 1 appends `PETSC_EXTRA` to the
configure line, so a variant builds beside the default without touching it: B1b is
`PETSC_ARCH=arch-mingw-real-omp PETSC_EXTRA=--with-openmp`, which builds GetDP in
`E:\petsc-real\getdp-build-omp`. Seed the new arch's `externalpackages\` with the default's
`MUMPS_5.8.2` and `git.metis` to skip the downloads. Start MSYS2's bash from PowerShell or cmd;
started from Git Bash it runs nothing and exits 0.

B2 (hypre AMS) is `PETSC_ARCH=arch-mingw-real-omp-hypre PETSC_EXTRA="--with-openmp --download-hypre"`
(hypre 3.1.0, with no MPI, on MPIUNI), installed as `E:\Program Files\getdp_real_omp_hypre`. Step 3
always applies `patches/apply_ams_patch.py` to the GetDP source first. The patch is idempotent and
leaves `.orig` copies. It adds `patches/AMS_Gradient.cpp` to `src/kernel`, and adds a hook in
`LinAlg_PETSC.cpp` that hands a `hypre`/`ams` PC the discrete gradient and the edge vectors. On a PETSc
without hypre, the file compiles to nothing. Two options: `-getdp_ams_check` prints `|A·G·φ|` (1.6e-16
on the production sector), and `-getdp_ams_singular false` keeps AMS out of its β = 0 mode, which is what
the backend passes, since its ungauged deck is regularised (roadmap §5.11). The backend recognises the
build by `AMS: discrete gradient attached` in the binary. The hook also rebuilds the PC whenever
GetDP hands it a new Jacobian `Mat`. Without that, hypre's AMS setup leaks its old hierarchy on
every Newton step (about 0.16 GB per angle of a sweep). To iterate on the patch, run
`apply_ams_patch.py` and then `ninja` in the existing build directory rather than the whole of step 3.

The same script also applies the **term cache** (`patches/TermCache.cpp`, roadmap §5.13), on every
arch. A term whose integration method's name starts with `Cached` is integrated once per
`IterativeLoop`, and its element matrices are replayed on the loop's later passes. The cache is
emptied on entry to and exit from each loop. The deck writer marks the terms that are constant within
a Newton loop: the linear reluctivity, the ε a·a' regularisation, and the magnet and coil sources.
To an unpatched GetDP, `CachedInt` is just `Int`. The replay is the same element matrix, so results
are the same to the bit. `-getdp_no_term_cache` (in `.petscrc` or `PETSC_OPTIONS`) turns it off, and
each loop logs `Term cache: N element terms integrated, M replayed`.

## Known limits of this build

- **The default build runs single-threaded.** Its MUMPS has no OpenMP, and its OpenBLAS threads slow
  the factorisation (11.4 s on 1 thread, 20.3 s on 16). The backend does this itself.
- **The OpenMP build (B1b, `E:\Program Files\getdp_real_omp`, now preferred) threads MUMPS itself**:
  12.0, 10.3, 7.8, 9.6, 13.4 s on 1, 2, 4, 8, 16 threads. The backend gives a lone solve four and
  each concurrent sweep block one. It recognises the build by the string `and #OMP` in the binary
  (MUMPS prints `without OMP` instead when built without it); `--info` does not say.
- `to_msh22.py` is left from the first, Gmsh-less build, which read MSH ≤ 3.1 only.
- **Out-of-core MUMPS (`-mat_mumps_icntl_22 1`) hangs** after writing its factor file. It no longer
  returns NaN, but it is not usable.
