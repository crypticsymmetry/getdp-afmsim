#!/usr/bin/bash
# GetDP 4.0-git against the real-arithmetic PETSc and the minimal static Gmsh
# library of 0_gmsh_lib.sh (so it reads MSH 4 meshes the way the official
# builds do).
set -e
export PATH=/mingw64/bin:/usr/bin
export MSYSTEM=MINGW64
export PETSC_DIR=/e/petsc-real/petsc-3.25.5
# PETSC_ARCH=arch-mingw-real-omp PETSC_EXTRA=--with-openmp for B1b.
export PETSC_ARCH=${PETSC_ARCH:-arch-mingw-real}
# GETDP_SRC and GETDP_BUILD override the source tree and build directory
# (the clean rebuild of 2026-09-25 built a pristine copy elsewhere).
SRC="${GETDP_SRC:-/e/Program Files/getdp-git-source}"
BUILD="${GETDP_BUILD:-/e/petsc-real/getdp-build${PETSC_ARCH#arch-mingw-real}}"
GMSH=E:/petsc-real/gmsh-install
INC="-IE:/petsc-real/petsc-3.25.5/include -IE:/petsc-real/petsc-3.25.5/$PETSC_ARCH/include -IC:/msys64/mingw64/include"
# B2: the hypre AMS hook and discrete gradient (patches/). Idempotent, and
# compiles to nothing on a PETSc without hypre.
/mingw64/bin/python3 "$(dirname "$0")/patches/apply_ams_patch.py" "$SRC"
rm -rf "$BUILD"; mkdir -p "$BUILD"
cd "$BUILD"
# PETSc's petscvariables records MSYS paths (/e/...), which the native
# compiler cannot open, so the same directories are passed again in Windows
# form. MSYS2's static OpenBLAS needs an OpenMP runtime: -fopenmp -lgomp.
# The static Gmsh library needs the Windows socket, timer and shell libraries.
cmake "$SRC" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_PETSC=ON -DENABLE_SLEPC=OFF -DENABLE_ARPACK=OFF \
  -DENABLE_GMSH=ON -DCMAKE_PREFIX_PATH="$GMSH" \
  -DENABLE_PYTHON=OFF -DENABLE_OCTAVE=OFF \
  -DENABLE_SPARSKIT=OFF -DENABLE_ZITSOL=OFF -DENABLE_PEWE=OFF \
  -DENABLE_GSL=OFF -DENABLE_NR=ON -DENABLE_MPI=OFF \
  -DENABLE_BLAS_LAPACK=ON -DENABLE_FORTRAN=ON \
  -DPETSC_DIR="$PETSC_DIR" -DPETSC_ARCH="$PETSC_ARCH" \
  -DCMAKE_CXX_FLAGS="$INC" -DCMAKE_C_FLAGS="$INC" \
  -DCMAKE_EXE_LINKER_FLAGS="-static -fopenmp -LE:/petsc-real/petsc-3.25.5/$PETSC_ARCH/lib -LC:/msys64/mingw64/lib" \
  -DCMAKE_CXX_STANDARD_LIBRARIES="-lgomp -lgfortran -lquadmath -lpthread -lws2_32 -lwsock32 -lwinmm -lshlwapi -lole32 -lpsapi" \
  > "$BUILD/cmake.out" 2>&1 || { echo "cmake failed"; tail -40 "$BUILD/cmake.out"; exit 1; }
grep -E "Build options|Using PETSc" "$BUILD/cmake.out" || true
ninja -j8 > "$BUILD/ninja.out" 2>&1 || { echo "build failed"; tail -60 "$BUILD/ninja.out"; exit 1; }
ls -la "$BUILD"/getdp.exe
"$BUILD"/getdp.exe --info
