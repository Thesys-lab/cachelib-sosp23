#!/bin/bash 

set -euo pipefail


algo=s3fifo
for nThread in 1 2 4 8 16 24 32; do 
    sz=$(echo "1000 * ${nThread}" | bc)
    hp=$(echo "21 + l(${nThread})/l(2)" | bc -l | cut -d'.' -f1)
    echo "nThread ${nThread} sz $sz hp $hp"
   ./strictlru /disk/data/zipf1.0.oracleGeneral.bin_1_20 $sz $hp ${nThread} | tee -a result/strictlru
done


for sz in 100 200 500 1000 2000 4000; do 
   ./s3fifo /disk/data/zipf1.0.oracleGeneral.bin_10_100 $sz 24 1 &
done


i=0
for size in 500 1000 2000 4000; do 
   i=$((i+1))
   for algo in strictlru lru tinylfu twoq s3fifo; do 
      for nThread in 1 2 4 8 16; do 
         sz=$(echo "$size * ${nThread}" | bc)
         hp=$(echo "18 + ${i} + l(${nThread})/l(2)" | bc -l | cut -d'.' -f1)
         echo "nThread ${nThread} sz $sz hp $hp"
         numactl --membind =0 ./_build/${algo} /disk/data/zipf1.0.oracleGeneral.bin_1_20 $sz $hp ${nThread} | tee -a res
      done
   done
done

i=0
for size in 200 500 1000 2000 4000; do 
   i=$((i+1))
   for algo in strictlru lru tinylfu twoq s3fifo; do 
      for nThread in 1 2 4 8 16; do 
         sz=$(echo "$size * ${nThread}" | bc)
         hp=$(echo "21 + ${i} + l(${nThread})/l(2)" | bc -l | cut -d'.' -f1)
         echo "nThread ${nThread} sz $sz hp $hp"
         numactl --membind =0 ./_build/${algo} /disk/data/cluster52.oracleGeneral.sample10.bin $sz $hp ${nThread} | tee -a cluster52
      done
   done
done



