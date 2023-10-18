#!/bin/bash 
set -euo pipefail

BUILD_TYPE=RelWithDebInfo

sudo apt install ninja-build >/dev/null; 

clear
pushd ../; 
./contrib/build.sh -j -v;
# cd build-cachelib; make -j > /dev/null && make install > /dev/null;
# cp cachelib/allocator/*.h opt/cachelib/include/cachelib/allocator/
cd build-cachelib; ninja && ninja install;
popd;

# Root directory for the CacheLib project
CLBASE="$PWD/../"

# Additional "FindXXX.cmake" files are here (e.g. FindSodium.cmake)
CLCMAKE="$CLBASE/cachelib/cmake"

# After ensuring we are in the correct directory, set the installation prefix"
PREFIX="$CLBASE/opt/cachelib/"

CMAKE_PARAMS="-DCMAKE_INSTALL_PREFIX='${PREFIX}' -DCMAKE_MODULE_PATH='${CLCMAKE}' -DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
# echo "CMAKE_PARAMS $CMAKE_PARAMS"

export CMAKE_PREFIX_PATH="$PREFIX/lib/cmake:$PREFIX/lib64/cmake:$PREFIX/lib:$PREFIX/lib64:$PREFIX:${CMAKE_PREFIX_PATH:-}"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"
LD_LIBRARY_PATH="$PREFIX/lib:$PREFIX/lib64:${LD_LIBRARY_PATH:-}"
export LD_LIBRARY_PATH

rm -r _build 2>/dev/null || true
mkdir -p _build 2>/dev/null || true
cd _build && cmake $CMAKE_PARAMS -G Ninja .. >/dev/null

# export GLOG_logtostderr=1
ninja
cd ..;
