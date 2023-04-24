/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <stdexcept>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <folly/Format.h>
#include <folly/Range.h>
#pragma GCC diagnostic pop

namespace facebook {
namespace cachelib {

template <typename T, typename BucketHashTable::Hook<T> T::*HookPtr>
BucketHashTable::Impl<T, HookPtr>::Impl(size_t numSlots,
                                        const PtrCompressor& compressor,
                                        const Hasher& hasher)
    : numSlots_((numSlots >> 3) << 3),
      slotMask_(numSlots_ - 1),
      compressor_(compressor),
      hasher_(hasher) {
  if (numSlots_ == 0) {
    throw std::invalid_argument("Can not have 0 buckets");
  }
  if (numSlots_ & (numSlots_ - 1)) {
    throw std::invalid_argument("Number of slots must be a power of two");
  }
  if (sizeof(CompressedPtr) != sizeof(uint32_t)) {
    throw std::invalid_argument(
        folly::sformat("CompressedPtr size {} != uint32_t size {}",
                       sizeof(CompressedPtr), sizeof(uint32_t)));
  }
  hashTable_ = std::make_unique<uint32_t[]>(numSlots_);
  uint32_t* memStart = hashTable_.get();
  std::fill(memStart, memStart + numSlots_, 0);
}

template <typename T, typename BucketHashTable::Hook<T> T::*HookPtr>
BucketHashTable::Impl<T, HookPtr>::Impl(size_t numSlots,
                                        void* memStart,
                                        const PtrCompressor& compressor,
                                        const Hasher& hasher,
                                        bool resetMem)
    : numSlots_((numSlots >> 3) << 3),
      slotMask_(numSlots_ - 1),
      hashTable_(static_cast<uint32_t*>(memStart)),
      restorable_(true),
      compressor_(compressor),
      hasher_(hasher) {
  if (numSlots_ == 0) {
    throw std::invalid_argument("Can not have 0 buckets");
  }
  if (numSlots_ & (numSlots_ - 1)) {
    throw std::invalid_argument("Number of buckets must be a power of two");
  }
  if (resetMem) {
    uint32_t* memStartBucket = static_cast<uint32_t*>(memStart);
    std::fill(memStartBucket, memStartBucket + numSlots_, 0);
  }
}

template <typename T, typename BucketHashTable::Hook<T> T::*HookPtr>
BucketHashTable::Impl<T, HookPtr>::Impl::~Impl() {
  if (restorable_) {
    hashTable_.release();
  }
}

template <typename T, typename BucketHashTable::Hook<T> T::*HookPtr>
bool BucketHashTable::Impl<T, HookPtr>::insertInBucket(T& node) noexcept {
  XDCHECK_LT(bucket, numSlots_);

  /* calculate hash table value */
  uint64_t hashVal = getHash(*node);
  uint64_t tag = getTagFromHash(hashVal);
  uint64_t ptr = static_cast<uint64_t>(
      reinterpret_cast<uint32_t>(compressor_.compress(&node)));
  uint64_t newValInTable = (ptr << 32u) | tag;

  int writePos = -1;
  size_t bucket = hashVal & slotMask_;
  uint64_t* head = &hashTable_[bucket];
  char* headChar = reinterpret_cast<char*>(head);
  uint64_t* currHead;

  int nExtraBuckets = static_cast<int>(*(headChar + nExtraBucketPos));
  bool firstBucket = true;

  while (nExtraBuckets >= 0) {
    int startIdx = 0;
    if (firstBucket) {
      startIdx = 1;
      firstBucket = false;
      currHead = head;
    } else {
      currHead = reinterpret_cast<std::unique_ptr<uint64_t[]>>(*(currHead + nItemsPerBucket - 1));
    }
    int endIdx = nExtraBuckets == 0 ? nItemsPerBuckek : nItemsPerBucket - 1;

    // use the first slot for metadata
    for (int i = startIdx; i < endIdx; i++) {
      uint32_t valInTable = __atomic_load_n(&currHead[i], __ATOMIC_RELAXED);
      if (val == 0) {
        if (__atomic_compare_exchange_n(&currHead[i], valInTable, newValInTable,
                                        false, __ATOMIC_RELAXED,
                                        __ATOMIC_RELAXED)) {
          writePos = i;
        } else {
          continue;
        }
      } else {
        uint64_t tag2 = getTagFromHashTableVal(valInTable);
        if (tag == tag2 &&
            compressor_.unCompress(ptr)->getKey() == node.getKey()) {
          // key already exists
          uint64_t zero = 0;
          bool reverted = __atomic_compare_exchange_n(
              &currHead[writePos], valInTable, zero, false, __ATOMIC_RELAXED,
              __ATOMIC_RELAXED);
          XDCHECK(reverted);
          return false;
        }
      }
    }
  }


  if (writePos == -1) {
    // no empty slot found, need to expand
      std::unique_ptr<uint64_t> newBucket = std::make_unique<uint64_t[]>(nItemsPerBucket);
      if (newBucket == nullptr) {
        return false;
      }
      memset(newBucket.get(), 0, nItemsPerBucket * sizeof(uint64_t));
      lock(headChar);



      unlock(headChar);

      
  //     uint32_t* oldBucket = head;
  //     uint32_t* newBucketPtr =
  //         static_cast<uint32_t*>(compressor_.compress(newBucket));
  //     uint64_t newBucketVal = (static_cast<uint64_t>(newBucketPtr) << 32u) |
  //                             static_cast<uint64_t>(tag);
  //     if (__atomic_compare_exchange_n(head, oldBucket, newBucketVal, false,
  //                                     __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
  //       // successfully allocated a new bucket
  //       head = newBucket;
  //       nExtraBuckets = 1;
  //     } else {
  //       // failed to allocate a new bucket, free the memory
  //       free(newBucket);
  //       return false;
  //     }
  //   }

  //   // write to the extra bucket
  //   head = reinterpret_cast<uint32_t*>(compressor_.unCompress(
  //       getPtrFromHashTableVal(__atomic_load_n(head, __ATOMIC_RELAXED))));
  //   for (int i = 0; i < nItemsPerBucket; i++) {
  //     uint32_t valInTable = __atomic_load_n(&head[i], __ATOMIC_RELAXED);
  //     if (val == 0) {
  //       if (__atomic_compare_exchange_n(&head[i], valInTable, newValInTable,
  //                                       false, __ATOMIC_RELAXED,
  //                                       __ATOMIC_RELAXED)) {
  //         writePos = i;
  //         break;
  //       }
  //     }
  //   }
  //   XDCHECK(writePos != -1);
  //   nExtraBuckets--;
  // }

  const auto key = node.getKey();
  do {
    const auto existing = findInBucket(key, bucket);
    if (existing != nullptr) {
      return false;
    }

    // insert at the head of the bucket
    setHashNext(node, *head);
  } while (!__atomic_compare_exchange_n(&hashTable_[bucket], head, newHead,
                                        false, __ATOMIC_RELAXED,
                                        __ATOMIC_RELAXED));
  return true;
}

template <typename T, typename BucketHashTable::Hook<T> T::*HookPtr>
T* BucketHashTable::Impl<T, HookPtr>::insertOrReplaceInBucket(
    T& node, size_t bucket) noexcept {
  XDCHECK_LT(bucket, numBuckets_);

  const auto key = node.getKey();
  // See if we can find the key and the previous node
  T* curr = compressor_.unCompress(hashTable_[bucket]);
  T* prev = nullptr;

  while (curr != nullptr && key != curr->getKey()) {
    prev = curr;
    curr = getHashNext(*curr);
  }

  // insert if the key doesn't exist
  if (!curr) {
    const auto head = hashTable_[bucket];
    hashTable_[bucket] = compressor_.compress(&node);
    setHashNext(node, head);
    return nullptr;
  }

  // replace
  if (prev) {
    setHashNext(*prev, &node);
  } else {
    hashTable_[bucket] = compressor_.compress(&node);
  }
  setHashNext(node, getHashNext(*curr));

  return curr;
}

template <typename T, typename BucketHashTable::Hook<T> T::*HookPtr>
void BucketHashTable::Impl<T, HookPtr>::removeFromBucket(
    T& node, size_t bucket) noexcept {
  // node must be present in hashtable.
  XDCHECK_EQ(reinterpret_cast<uintptr_t>(findInBucket(node.getKey(), bucket)),
             reinterpret_cast<uintptr_t>(&node))
      << node.toString();

  T* const prev = findPrevInBucket(node, bucket);
  if (prev != nullptr) {
    setHashNext(*prev, getHashNext(node));
  } else {
    XDCHECK_EQ(reinterpret_cast<uintptr_t>(&node),
               reinterpret_cast<uintptr_t>(
                   compressor_.unCompress(hashTable_[bucket])));
    hashTable_[bucket] = getHashNextCompressed(node);
  }
}

template <typename T, typename BucketHashTable::Hook<T> T::*HookPtr>
T* BucketHashTable::Impl<T, HookPtr>::findInBucket(
    Key key, size_t bucket) const noexcept {
  XDCHECK_LT(bucket, numBuckets_);
  T* curr = compressor_.unCompress(hashTable_[bucket]);
  while (curr != nullptr && reinterpret_cast<int64_t>(curr) != INVALID_PTR_ &&
         curr->getKey() != key) {
    curr = getHashNext(*curr);
  }
  return curr;
}

template <typename T, typename BucketHashTable::Hook<T> T::*HookPtr>
T* BucketHashTable::Impl<T, HookPtr>::findPrevInBucket(
    const T& node, size_t bucket) const noexcept {
  XDCHECK_LT(bucket, numBuckets_);
  T* curr = compressor_.unCompress(hashTable_[bucket]);
  T* prev = nullptr;

  const auto key = node.getKey();
  while (curr != nullptr && key != curr->getKey()) {
    prev = curr;
    curr = getHashNext(*curr);
  }
  // node must be in the hashtable
  XDCHECK(curr != nullptr);
  return prev;
}

template <typename T, typename BucketHashTable::Hook<T> T::*HookPtr>
template <typename F>
void BucketHashTable::Impl<T, HookPtr>::forEachBucketElem(size_t bucket,
                                                          F&& func) const {
  XDCHECK_LT(bucket, numBuckets_);
  T* curr = compressor_.unCompress(hashTable_[bucket]);

  while (curr != nullptr) {
    func(curr);
    curr = getHashNext(*curr);
  }
}

template <typename T, typename BucketHashTable::Hook<T> T::*HookPtr>
unsigned int BucketHashTable::Impl<T, HookPtr>::getBucketNumElems(
    size_t bucket) const {
  XDCHECK_LT(bucket, numBuckets_);

  T* curr = compressor_.unCompress(hashTable_[bucket]);

  unsigned int numElems = 0;
  while (curr != nullptr) {
    ++numElems;
    curr = getHashNext(*curr);
  }
  return numElems;
}

// AccessContainer interface
template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
BucketHashTable::Container<T, HookPtr, LockT>::Container(
    const serialization::BucketHashTableObject& object,
    const Config& config,
    ShmAddr memSegment,
    const PtrCompressor& compressor,
    HandleMaker hm)
    : Container(object,
                config,
                memSegment.addr,
                memSegment.size,
                compressor,
                std::move(hm)) {}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
BucketHashTable::Container<T, HookPtr, LockT>::Container(
    const serialization::BucketHashTableObject& object,
    const Config& config,
    void* memStart,
    size_t nBytes,
    const PtrCompressor& compressor,
    HandleMaker hm)
    : config_{config},
      handleMaker_(std::move(hm)),
      ht_{config_.getNumBuckets(), memStart, compressor, config_.getHasher(),
          false /* resetMem */},
      locks_{config_.getLocksPower(), config_.getHasher()},
      numKeys_(*object.numKeys()) {
  if (config_.getBucketsPower() !=
      static_cast<uint32_t>(*object.bucketsPower())) {
    throw std::invalid_argument(folly::sformat(
        "Hashtable bucket power not compatible. old = {}, new = {}",
        *object.bucketsPower(),
        config.getBucketsPower()));
  }

  if (nBytes != ht_.size()) {
    throw std::invalid_argument(
        folly::sformat("Hashtable size not compatible. old = {}, new = {}",
                       ht_.size(),
                       nBytes));
  }

  // checking hasher magic id not equal to 0 is to ensure it'll be
  // a warm roll going from a cachelib without hasher magic id to
  // one with a magic id
  if (*object.hasherMagicId() != 0 &&
      *object.hasherMagicId() != config_.getHasher()->getMagicId()) {
    throw std::invalid_argument(folly::sformat(
        "Hash object's ID mismatch. expected = {}, actual = {}",
        *object.hasherMagicId(), config_.getHasher()->getMagicId()));
  }
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
typename BucketHashTable::Container<T, HookPtr, LockT>::DistributionStats
BucketHashTable::Container<T, HookPtr, LockT>::getDistributionStats() const {
  const auto now = util::getCurrentTimeSec();
  const uint64_t numKeys = numKeys_;

  std::unique_lock<std::mutex> statsLockGuard(cachedStatsLock_);
  const auto numKeysDifference = numKeys > cachedStats_.numKeys
                                     ? numKeys - cachedStats_.numKeys
                                     : cachedStats_.numKeys - numKeys;

  const bool needToRecompute =
      (now - cachedStatsUpdateTime_ > 10 * 60 /* seconds */) ||
      (cachedStats_.numKeys > 0 &&
       (static_cast<double>(numKeysDifference) /
            static_cast<double>(cachedStats_.numKeys) >
        0.05));

  // return the cached value or if someone else is already computing.
  if (!needToRecompute || !canRecomputeDistributionStats_) {
    return cachedStats_;
  }

  // record that we are iterating so that we dont cause everyone who
  // observes this to recompute
  canRecomputeDistributionStats_ = false;

  // release the lock.
  statsLockGuard.unlock();

  // compute the distribution
  std::map<unsigned int, uint64_t> distribution;
  const auto numBuckets = ht_.getNumBuckets();
  for (size_t currBucket = 0; currBucket < numBuckets; ++currBucket) {
    auto l = locks_.lockShared(currBucket);
    ++distribution[ht_.getBucketNumElems(currBucket)];
  }

  // acquire lock
  statsLockGuard.lock();
  cachedStats_.numKeys = numKeys;
  cachedStats_.itemDistribution = std::move(distribution);
  cachedStats_.numBuckets = ht_.getNumBuckets();
  cachedStatsUpdateTime_ = now;
  canRecomputeDistributionStats_ = true;
  return cachedStats_;
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
bool BucketHashTable::Container<T, HookPtr, LockT>::insert(T& node) noexcept {
  if (node.isAccessible()) {
    // already in hash table.
    return false;
  }

  const auto bucket = ht_.getBucket(node.getKey());
  // auto l = locks_.lockExclusive(bucket);
  const bool res = ht_.insertInBucket(node, bucket);

  if (res) {
    node.markAccessible();
    numKeys_.fetch_add(1, std::memory_order_relaxed);
  }

  return res;
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
typename T::Handle
BucketHashTable::Container<T, HookPtr, LockT>::insertOrReplace(T& node) {
  if (node.isAccessible()) {
    return handleMaker_(nullptr);
  }

  const auto bucket = ht_.getBucket(node.getKey());
  // auto l = locks_.lockExclusive(bucket);
  T* oldNode = ht_.insertOrReplaceInBucket(node, bucket);
  XDCHECK_NE(reinterpret_cast<uintptr_t>(&node),
             reinterpret_cast<uintptr_t>(oldNode));

  // grab a handle to the old node before we mark it as not being in the hash
  // table.
  typename T::Handle handle;
  try {
    handle = handleMaker_(oldNode);
  } catch (const std::exception&) {
    // put the element back since we failed to grab handle.
    ht_.insertOrReplaceInBucket(*oldNode, bucket);
    XDCHECK_EQ(
        reinterpret_cast<uintptr_t>(ht_.findInBucket(node.getKey(), bucket)),
        reinterpret_cast<uintptr_t>(oldNode))
        << oldNode->toString();
    throw;
  }

  node.markAccessible();

  if (oldNode) {
    oldNode->unmarkAccessible();
  } else {
    numKeys_.fetch_add(1, std::memory_order_relaxed);
  }

  return handle;
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
bool BucketHashTable::Container<T, HookPtr, LockT>::replaceIfAccessible(
    T& oldNode, T& newNode) noexcept {
  return replaceIf(oldNode, newNode, [](T&) { return true; });
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
template <typename F>
bool BucketHashTable::Container<T, HookPtr, LockT>::replaceIf(T& oldNode,
                                                              T& newNode,
                                                              F&& predicate) {
  const auto key = newNode.getKey();
  const auto bucket = ht_.getBucket(key);
  // auto l = locks_.lockExclusive(bucket);

  if (oldNode.isAccessible() && predicate(oldNode)) {
    ht_.insertOrReplaceInBucket(newNode, bucket);
    oldNode.unmarkAccessible();
    newNode.markAccessible();
    return true;
  }
  return false;
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
bool BucketHashTable::Container<T, HookPtr, LockT>::remove(T& node) noexcept {
  const auto bucket = ht_.getBucket(node.getKey());
  // auto l = locks_.lockExclusive(bucket);

  // check inside the lock to prevent from racing removes
  if (!node.isAccessible()) {
    return false;
  }

  ht_.removeFromBucket(node, bucket);
  node.unmarkAccessible();

  numKeys_.fetch_sub(1, std::memory_order_relaxed);
  return true;
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
typename T::Handle BucketHashTable::Container<T, HookPtr, LockT>::removeIf(
    T& node, const std::function<bool(const T& node)>& predicate) {
  const auto bucket = ht_.getBucket(node.getKey());
  auto l = locks_.lockExclusive(bucket);

  // check inside the lock to prevent from racing removes
  if (node.isAccessible() && predicate(node)) {
    // grab the handle before we do any other state change. this ensures that
    // if handle maker throws an exception, we leave the item in a consistent
    // state.
    auto handle = handleMaker_(&node);
    ht_.removeFromBucket(node, bucket);
    node.unmarkAccessible();
    numKeys_.fetch_sub(1, std::memory_order_relaxed);
    return handle;
  } else {
    return handleMaker_(nullptr);
  }
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
typename T::Handle BucketHashTable::Container<T, HookPtr, LockT>::find(
    Key key) const {
  const auto bucket = ht_.getBucket(key);
  auto l = locks_.lockShared(bucket);
  return handleMaker_(ht_.findInBucket(key, bucket));
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
serialization::BucketHashTableObject
BucketHashTable::Container<T, HookPtr, LockT>::saveState() const {
  if (!ht_.isRestorable()) {
    throw std::logic_error(
        "hashtable is not restorable since the memory is not managed by user");
  }

  if (numIterators_ != 0) {
    throw std::logic_error(
        folly::sformat("There are {} pending iterators", numIterators_.load()));
  }

  serialization::BucketHashTableObject object;
  *object.bucketsPower() = config_.getBucketsPower();
  *object.locksPower() = config_.getLocksPower();
  *object.numKeys() = numKeys_;
  *object.hasherMagicId() = config_.getHasher()->getMagicId();
  return object;
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
void BucketHashTable::Container<T, HookPtr, LockT>::getBucketElems(
    size_t bucket, std::vector<Handle>& handles) const {
  handles.clear();
  auto l = locks_.lockShared(bucket);

  ht_.forEachBucketElem(bucket, [this, &handles](T* e) {
    try {
      XDCHECK(e);
      handles.emplace_back(handleMaker_(e));
    } catch (const std::exception&) {
      // if we are not able to acquire a handle, skip over them.
    }
  });
}

// Container's Iterator
// with/without throtter to iterate
template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
typename BucketHashTable::Container<T, HookPtr, LockT>::Iterator&
BucketHashTable::Container<T, HookPtr, LockT>::Iterator::operator++() {
  if (throttler_) {
    throttler_->throttle();
  }

  ++curSor_;
  if (curSor_ < bucketElems_.size()) {
    return *this;
  }

  ++currBucket_;
  for (; currBucket_ < container_->config_.getNumBuckets(); ++currBucket_) {
    container_->getBucketElems(currBucket_, bucketElems_);
    if (!bucketElems_.empty()) {
      curSor_ = 0;
      return *this;
    } else if (throttler_) {
      throttler_->throttle();
    }
  }

  // reach the end
  bucketElems_.clear();
  curSor_ = 0;
  return *this;
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
T& BucketHashTable::Container<T, HookPtr, LockT>::Iterator::operator*() {
  return *curr();
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
BucketHashTable::Container<T, HookPtr, LockT>::Iterator::Iterator(
    Container<T, HookPtr, LockT>& container,
    folly::Optional<util::Throttler::Config> throttlerConfig)
    : container_(&container) {
  if (throttlerConfig) {
    throttler_.assign(util::Throttler(*throttlerConfig));
  }

  ++container_->numIterators_;

  reset();
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
BucketHashTable::Container<T, HookPtr, LockT>::Iterator::Iterator(
    Iterator&& other) noexcept
    : container_{other.container_},
      currBucket_{other.currBucket_},
      curSor_{other.curSor_},
      bucketElems_(std::move(other.bucketElems_)) {
  // increment the iterator count when we move.
  ++container_->numIterators_;
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
typename BucketHashTable::Container<T, HookPtr, LockT>::Iterator&
BucketHashTable::Container<T, HookPtr, LockT>::Iterator::operator=(
    Iterator&& other) noexcept {
  if (this != &other) {
    this->~Iterator();
    new (this) Iterator(std::move(other));
  }
  return *this;
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
BucketHashTable::Container<T, HookPtr, LockT>::Iterator::Iterator(
    Container<T, HookPtr, LockT>& container, EndIterT)
    : container_(&container), currBucket_{container_->config_.getNumBuckets()} {
  // increment the iterator for both the end and begin() types so that the
  // destructor can just blindly decrement.
  ++container_->numIterators_;
  XDCHECK_EQ(0u, curSor_);
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
typename BucketHashTable::Container<T, HookPtr, LockT>::Iterator
BucketHashTable::Container<T, HookPtr, LockT>::begin(
    folly::Optional<util::Throttler::Config> throttlerConfig) {
  return Iterator(*this, throttlerConfig);
}

template <typename T,
          typename BucketHashTable::Hook<T> T::*HookPtr,
          typename LockT>
void BucketHashTable::Container<T, HookPtr, LockT>::Iterator::reset() {
  curSor_ = 0;
  currBucket_ = 0;
  container_->getBucketElems(currBucket_, bucketElems_);
  while (bucketElems_.empty() &&
         ++currBucket_ < container_->config_.getNumBuckets()) {
    if (throttler_) {
      throttler_->throttle();
    }
    container_->getBucketElems(currBucket_, bucketElems_);
  }
  XDCHECK_EQ(0u, curSor_);
}
} // namespace cachelib
} // namespace facebook
