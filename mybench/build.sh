#!/bin/bash 


set -euo pipefail

BUILD_TYPE=RelWithDebInfo

clear
pushd ../; 
# ./contrib/build.sh -j -v;
# cd build-cachelib; make -j > /dev/null && make install > /dev/null;
# cp cachelib/allocator/*.h opt/cachelib/include/cachelib/allocator/
cd build-cachelib; ninja && ninja install;
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

# rm -r _build || true
mkdir -p _build 2>/dev/null || true
cd _build && cmake $CMAKE_PARAMS -G Ninja .. >/dev/null
# ninja && ./mybench /disk/data/w80.oracleGeneral.bin 100 86400 4
# exit

export GLOG_logtostderr=1
ninja 
# gdb -ex r --args ./s3fifo /disk/data/cluster52.oracleGeneral.sample10 400 24 2
./s3fifo /disk/data/cluster52.oracleGeneral.sample10 200 24 1
# ./s3fifo /disk/data/de/w80.oracleGeneral.bin 1000 24 1
./_build/clock /disk/data/meta/meta_kvcache_traces_1.oracleGeneral.bin 1000 24 1 &
./_build/lru /disk/data/meta/meta_kvcache_traces_1.oracleGeneral.bin 1000 24 1 &
./_build/twoq /disk/data/meta/meta_kvcache_traces_1.oracleGeneral.bin 1000 24 1 &
./_build/tinylfu /disk/data/meta/meta_kvcache_traces_1.oracleGeneral.bin 1000 24 1 &
# numactl --membind=0 gdb -ex r --args ./s3fifo /disk/data/zipf1.0.oracleGeneral.bin 16000 28 16
# numactl --membind=0 ./mybench /disk/data/de/cluster52.oracleGeneral.sample10 8000 26 32


