#!/usr/bin/bash
# PETSc for GetDP: real scalars, sequential, MUMPS + METIS, OpenBLAS.
set -e
export PATH=/mingw64/bin:/usr/bin
export MSYSTEM=MINGW64
cd /e/petsc-real/petsc-3.25.5
export PETSC_DIR=/e/petsc-real/petsc-3.25.5
# PETSC_ARCH=arch-mingw-real-omp PETSC_EXTRA=--with-openmp for B1b.
export PETSC_ARCH=${PETSC_ARCH:-arch-mingw-real}
/usr/bin/python3 ./configure \
  --with-scalar-type=real --with-precision=double \
  --with-mpi=0 --with-debugging=0 \
  --with-shared-libraries=0 \
  --with-cc=gcc --with-cxx=g++ --with-fc=gfortran \
  --with-openblas=1 --with-openblas-dir=/mingw64 \
  --download-mumps --download-metis ${PETSC_EXTRA:-} \
  --with-x=0 --with-windows-graphics=0 --with-ssl=0 \
  COPTFLAGS="-O3" CXXOPTFLAGS="-O3" FOPTFLAGS="-O3" \
  > /e/petsc-real/configure-$PETSC_ARCH.out 2>&1
echo "configure exit $?"
