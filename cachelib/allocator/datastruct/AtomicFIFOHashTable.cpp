
#include "cachelib/allocator/datastruct/AtomicFIFOHashTable.h"


namespace facebook {
namespace cachelib {


bool AtomicFIFOHashTable::contains(uint32_t key) noexcept {
  // LockHolder l(*mtx_);
  uint32_t bucketIdx = getBucketIdx(key);
  int64_t currTime = numInserts_.load();
  uint64_t zero = 0;

  for (uint32_t i = 0; i < nItemPerBucket_; i++) {
    // uint64_t valInTable = hashTable_[bucketIdx + i];
    uint64_t valInTable = __atomic_load_n(&hashTable_[bucketIdx + i], __ATOMIC_RELAXED);  
    int64_t age = currTime - getInsertionTime(valInTable);
    if (valInTable == 0) {
      continue;
    }
    if (age > fifoSize_) {
      __atomic_compare_exchange_n(&hashTable_[bucketIdx + i], &valInTable, 0,
                                  true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
      continue;
    }
    if (matchKey(valInTable, key)) {
      __atomic_compare_exchange_n(&hashTable_[bucketIdx + i], &valInTable, 0,
                                  true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
      return true;
    }
  }
  return false;
}

void AtomicFIFOHashTable::insert(uint32_t key) noexcept {
  // LockHolder l(*mtx_);
  int64_t currTime = numInserts_++;
  if (currTime > UINT32_MAX) {
    numInserts_ = 0;
    currTime = 0;
  }
  size_t bucketIdx = getBucketIdx(key);
  uint64_t hashTableVal = genHashtableVal(key, currTime);

  for (size_t i = 0; i < nItemPerBucket_; i++) {
    uint64_t valInTable = __atomic_load_n(&hashTable_[bucketIdx + i], __ATOMIC_RELAXED);
    if (valInTable == 0) {
      if (__atomic_compare_exchange_n(&hashTable_[bucketIdx + i], &valInTable,
                                      hashTableVal, true, __ATOMIC_RELAXED,
                                      __ATOMIC_RELAXED)) {
        return;
      }
    }
  }
  // we do not find an empty slots, random choose and overwrite one
  numEvicts_++;
  __atomic_store_n(&hashTable_[key % numElem_], hashTableVal, __ATOMIC_RELAXED);
  // hashTable_[key % numElem_] = hashTableVal;
}

} // namespace cachelib
} // namespace facebook
