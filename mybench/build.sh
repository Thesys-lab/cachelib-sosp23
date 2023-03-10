#!/bin/bash 


set -euo pipefail

BUILD_TYPE=RelWithDebInfo

# sudo apt purge libgoogle-glog-dev libgoogle-glog0v5 || true
pushd ../; 
# ./contrib/build.sh -j -v;
# cd build-cachelib; make -j > /dev/null && make install > /dev/null;
cd build-cachelib; ninja && ninja install >/dev/null;
popd;

# cp -r ../cachelib/cmake ../opt/cachelib/ || true

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

export GLOG_logtostderr=1
# ninja && ./mybench /disk/data/fb_assoc_altoona_leader.oracleGeneral 16000 86400 1
