#!/usr/bin/bash
# Minimal static Gmsh library for GetDP: the options the official GetDP builds
# report ("ANN Eigen Parser Plugins Post") less the plugins, which GetDP never calls and whose 4.15 BoundaryLayer needs the mesh module; plus the private API, which installs
# the internal headers (gmsh/PView.h, GmshGlobal.h, BasisFactory.h) GetDP uses.
set -e
export PATH=/mingw64/bin:/usr/bin
export MSYSTEM=MINGW64
SRC=/e/petsc-real/gmsh-4.15.2-source
BUILD=/e/petsc-real/gmsh-build
PREFIX=/e/petsc-real/gmsh-install
rm -rf "$BUILD"; mkdir -p "$BUILD"; cd "$BUILD"
cmake "$SRC" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DDEFAULT=0 -DENABLE_BUILD_LIB=1 -DENABLE_PRIVATE_API=1 \
  -DENABLE_POST=1 -DENABLE_PARSER=1 -DENABLE_PLUGINS=0 \
  -DENABLE_ANN=1 -DENABLE_EIGEN=1 \
  -DCMAKE_INSTALL_PREFIX="E:/petsc-real/gmsh-install" \
  > cmake.out 2>&1 || { echo "gmsh cmake failed"; tail -30 cmake.out; exit 1; }
grep -E "Gmsh .* has been configured|Build options" cmake.out || true
ninja -j8 lib > ninja.out 2>&1 || { echo "gmsh build failed"; tail -40 ninja.out; exit 1; }
ninja install > install.out 2>&1 || { echo "gmsh install failed"; tail -30 install.out; exit 1; }
ls -la "$PREFIX"/lib/libgmsh* ; ls "$PREFIX"/include/gmsh | head -5; ls "$PREFIX"/include/gmsh | wc -l
