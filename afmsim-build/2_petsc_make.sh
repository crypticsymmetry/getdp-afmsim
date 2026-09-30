#!/usr/bin/bash
export PATH=/mingw64/bin:/usr/bin
export MSYSTEM=MINGW64
cd /e/petsc-real/petsc-3.25.5
export PETSC_ARCH=${PETSC_ARCH:-arch-mingw-real}
make PETSC_DIR=/e/petsc-real/petsc-3.25.5 PETSC_ARCH=$PETSC_ARCH MAKE_NP=8 all > /e/petsc-real/make-$PETSC_ARCH.out 2>&1
rc=$?
echo "petsc make exit $rc"
tail -15 /e/petsc-real/make-$PETSC_ARCH.out
ls -la /e/petsc-real/petsc-3.25.5/$PETSC_ARCH/lib/libpetsc* 2>&1
if [ $rc -eq 0 ]; then
  bash "$(dirname "$0")/3_getdp_build.sh"
fi
