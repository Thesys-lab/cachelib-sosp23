#!/bin/bash 
set -euo pipefail

# data is generated using libCacheSim/scripts/data_gen.py
# python3 libCacheSim/scripts/data_gen.py -m 1000000 -n 20000000 --alpha 1.0 --bin-output zipf1.0_1_100.oracleGeneral.bin

usage() {
   echo "$0 <algo> <cache size in MB> <hashpower>"
   exit
}

algo=${1:-"s3fifo"}
sz_base=${2:-"4000"}
hp_base=${3:-"21"}


for nThread in 1 2 4 8 16; do 
    sz=$(echo "${sz_base} * ${nThread}" | bc)
    hp=$(echo "${hp_base} + l(${nThread})/l(2)" | bc -l | cut -d'.' -f1)
    echo "############## ${algo} ${nThread} threads, cache size $sz MB, hashpower $hp"
    numactl --membind=0 ./_build/${algo} zipf1.0_1_100.oracleGeneral.bin $sz $hp ${nThread} | tail -n 1
done

