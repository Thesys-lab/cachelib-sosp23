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

# export GLOG_logtostderr=1
ninja
cd ..;

DATA=/disk/data/wiki_2019t.oracleGeneral.bin
SIZE=16000
HP=24
NTHREAD=1
# gdb -ex r --args ./s3fifo /disk/data/cluster52.oracleGeneral.sample10 400 24 2
# ./s3fifo /disk/data/cluster52.oracleGeneral.sample10 200 24 1
# gdb -ex r --args ./_build/sieve /disk/data/w96.oracleGeneral.bin 1000 24 1
numactl --membind=0 ./_build/sieve /disk/data/zipf_1_1_20.oracleGeneral.bin 16000 27 16
numactl --membind=0 ./_build/s3fifo /disk/data/zipf_1_1_20.oracleGeneral.bin 16000 27 16
numactl --membind=0 ./_build/sieve /disk/data/zipf_1_1_20.oracleGeneral.bin 64000 27 16
numactl --membind=0 ./_build/s3fifo /disk/data/zipf_1_1_20.oracleGeneral.bin 64000 27 16

# ./_build/s3fifo $DATA ${SIZE} ${HP} $NTHREAD
# echo '########################################################'
# ./_build/clock $DATA ${SIZE} ${HP} $NTHREAD
# echo '########################################################'
# ./_build/lru $DATA ${SIZE} ${HP} $NTHREAD
# echo '########################################################'
# ./_build/sieve $DATA ${SIZE} ${HP} $NTHREAD
# echo '########################################################'
# ./_build/sievebuffered $DATA ${SIZE} ${HP} $NTHREAD
exit








for algo in lru s3fifo clock sieve tinylfu twoq; do
    for sz in 1000 2000 4000 8000 12000 16000 20000 24000 28000 32000; do
        numactl --membind=0 ./_build/$algo $DATA $sz ${HP} $NTHREAD | tee -a res
    done
done

# cluster52
numactl --membind=0 ./_build/$algo $DATA 100 20 1 | tee -a res2
numactl --membind=0 ./_build/$algo $DATA 200 21 1 | tee -a res2
numactl --membind=0 ./_build/$algo $DATA 400 22 1 | tee -a res2
numactl --membind=0 ./_build/$algo $DATA 800 23 1 | tee -a res2
numactl --membind=0 ./_build/$algo $DATA 1600 24 1 | tee -a res2
numactl --membind=0 ./_build/$algo $DATA 2400 24 1 | tee -a res2
numactl --membind=0 ./_build/$algo $DATA 3200 25 1 | tee -a res2
numactl --membind=0 ./_build/$algo $DATA 4800 25 1 | tee -a res2
numactl --membind=0 ./_build/$algo $DATA 6400 26 1 | tee -a res2
numactl --membind=0 ./_build/$algo $DATA 8000 26 1 | tee -a res2


numactl --membind=0 ./_build/$algo $DATA 4000 25 1 | tee -a res_mul2
numactl --membind=0 ./_build/$algo $DATA 8000 26 2 | tee -a res_mul2
numactl --membind=0 ./_build/$algo $DATA 16000 27 4 | tee -a res_mul2
numactl --membind=0 ./_build/$algo $DATA 32000 28 8 | tee -a res_mul2
numactl --membind=0 ./_build/$algo $DATA 64000 29 16 | tee -a res_mul2



# wiki 
numactl --membind=0 ./_build/sieve $DATA 8000 24 1 | tee -a res_mul
numactl --membind=0 ./_build/sieve $DATA 16000 25 2 | tee -a res_mul
numactl --membind=0 ./_build/sieve $DATA 32000 26 4 | tee -a res_mul
numactl --membind=0 ./_build/sieve $DATA 64000 27 8 | tee -a res_mul
numactl --membind=0 ./_build/sieve $DATA 128000 28 16 | tee -a res_mul

# meta
numactl --membind=0 ./_build/${algo} $DATA 4000 24 1 | tee -a res_mul4
numactl --membind=0 ./_build/${algo} $DATA 8000 25 2 | tee -a res_mul4
numactl --membind=0 ./_build/${algo} $DATA 16000 26 4 | tee -a res_mul4
numactl --membind=0 ./_build/${algo} $DATA 32000 27 8 | tee -a res_mul4
numactl --membind=0 ./_build/${algo} $DATA 64000 28 16 | tee -a res_mul4

# zipf
numactl --membind=0 ./_build/${algo} $DATA 4000 22 1 | tee -a res_mul6
numactl --membind=0 ./_build/${algo} $DATA 8000 23 2 | tee -a res_mul6
numactl --membind=0 ./_build/${algo} $DATA 16000 24 4 | tee -a res_mul6
numactl --membind=0 ./_build/${algo} $DATA 32000 25 8 | tee -a res_mul6
numactl --membind=0 ./_build/${algo} $DATA 64000 26 16 | tee -a res_mul6

# ./_build/twoq /disk/data/w96.oracleGeneral.bin 1000 24 1 &
# ./_build/tinylfu /disk/data/w96.oracleGeneral.bin 1000 24 1 &
# numactl --membind=0 gdb -ex r --args ./_build/s3fifo /disk/data/zipf1.0.oracleGeneral.bin 16000 28 16
# numactl --membind=0 ./_build/s3fifo /disk/zipf_1_1_20.oracleGeneral.bin 8000 28 16
# numactl --membind=0 ./_build/clock /disk/zipf_1_1_20.oracleGeneral.bin 8000 28 16
# numactl --membind=0 ./_build/sieve /disk/zipf_1_1_20.oracleGeneral.bin 8000 28 16
# numactl --membind=0 ./mybench /disk/data/de/cluster52.oracleGeneral.sample10 8000 26 32

