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

namespace facebook {
namespace cachelib {

/* Container Interface Implementation */
template <typename T, MMSieve2::Hook<T> T::*HookPtr>
MMSieve2::Container<T, HookPtr>::Container(serialization::MMSieve2Object object,
                                           PtrCompressor compressor)
    : compressor_(std::move(compressor)),
      sieve2_(*object.sieve2(), compressor_),
      insertionPoint_(compressor_.unCompress(
          CompressedPtr{*object.compressedInsertionPoint()})),
      tailSize_(*object.tailSize()),
      config_(*object.config()) {
  sieve2RefreshTime_ = config_.sieve2RefreshTime;
  nextReconfigureTime_ = config_.mmReconfigureIntervalSecs.count() == 0
                             ? std::numeric_limits<Time>::max()
                             : static_cast<Time>(util::getCurrentTimeSec()) +
                                   config_.mmReconfigureIntervalSecs.count();
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
bool MMSieve2::Container<T, HookPtr>::recordAccess(T& node,
                                                   AccessMode mode) noexcept {
  if ((mode == AccessMode::kWrite && !config_.updateOnWrite) ||
      (mode == AccessMode::kRead && !config_.updateOnRead)) {
    return false;
  }

  const auto curr = static_cast<Time>(util::getCurrentTimeSec());
  // check if the node is still being memory managed
  if (node.isInMMContainer() &&
      ((curr >= getUpdateTime(node) +
                    sieve2RefreshTime_.load(std::memory_order_relaxed)) ||
       !isAccessed(node))) {
    if (!isAccessed(node)) {
      markAccessed(node);
    }

    auto func = [this, &node, curr]() {
      reconfigureLocked(curr);
      ensureNotInsertionPoint(node);
      if (node.isInMMContainer()) {
        sieve2_.moveToHead(node);
        setUpdateTime(node, curr);
      }
      if (isTail(node)) {
        unmarkTail(node);
        tailSize_--;
        XDCHECK_LE(0u, tailSize_);
        updatesieve2InsertionPoint();
      }
    };

    // if the tryLockUpdate optimization is on, and we were able to grab the
    // lock, execute the critical section and return true, else return false
    //
    // if the tryLockUpdate optimization is off, we always execute the
    // critical section and return true
    if (config_.tryLockUpdate) {
      if (auto lck = LockHolder{*sieve2Mutex_, std::try_to_lock}) {
        func();
        return true;
      }

      return false;
    }

    sieve2Mutex_->lock_combine(func);
    return true;
  }
  return false;
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
cachelib::EvictionAgeStat MMSieve2::Container<T, HookPtr>::getEvictionAgeStat(
    uint64_t projectedLength) const noexcept {
  return sieve2Mutex_->lock_combine([this, projectedLength]() {
    return getEvictionAgeStatLocked(projectedLength);
  });
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
cachelib::EvictionAgeStat
MMSieve2::Container<T, HookPtr>::getEvictionAgeStatLocked(
    uint64_t projectedLength) const noexcept {
  EvictionAgeStat stat{};
  const auto currTime = static_cast<Time>(util::getCurrentTimeSec());

  const T* node = sieve2_.getTail();
  stat.warmQueueStat.oldestElementAge =
      node ? currTime - getUpdateTime(*node) : 0;
  for (size_t numSeen = 0; numSeen < projectedLength && node != nullptr;
       numSeen++, node = sieve2_.getPrev(*node)) {
  }
  stat.warmQueueStat.projectedAge = node ? currTime - getUpdateTime(*node)
                                         : stat.warmQueueStat.oldestElementAge;
  XDCHECK(detail::areBytesSame(stat.hotQueueStat, EvictionStatPerType{}));
  XDCHECK(detail::areBytesSame(stat.coldQueueStat, EvictionStatPerType{}));
  return stat;
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
void MMSieve2::Container<T, HookPtr>::setConfig(const Config& newConfig) {
  sieve2Mutex_->lock_combine([this, newConfig]() {
    config_ = newConfig;
    if (config_.sieve2InsertionPointSpec == 0 && insertionPoint_ != nullptr) {
      auto curr = insertionPoint_;
      while (tailSize_ != 0) {
        XDCHECK(curr != nullptr);
        unmarkTail(*curr);
        tailSize_--;
        curr = sieve2_.getNext(*curr);
      }
      insertionPoint_ = nullptr;
    }
    sieve2RefreshTime_.store(config_.sieve2RefreshTime,
                             std::memory_order_relaxed);
    nextReconfigureTime_ = config_.mmReconfigureIntervalSecs.count() == 0
                               ? std::numeric_limits<Time>::max()
                               : static_cast<Time>(util::getCurrentTimeSec()) +
                                     config_.mmReconfigureIntervalSecs.count();
  });
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
typename MMSieve2::Config MMSieve2::Container<T, HookPtr>::getConfig() const {
  return sieve2Mutex_->lock_combine([this]() { return config_; });
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
void MMSieve2::Container<T, HookPtr>::updatesieve2InsertionPoint() noexcept {
  if (config_.sieve2InsertionPointSpec == 0) {
    return;
  }

  // If insertionPoint_ is nullptr initialize it to tail first
  if (insertionPoint_ == nullptr) {
    insertionPoint_ = sieve2_.getTail();
    tailSize_ = 0;
    if (insertionPoint_ != nullptr) {
      markTail(*insertionPoint_);
      tailSize_++;
    }
  }

  if (sieve2_.size() <= 1) {
    // we are done;
    return;
  }

  XDCHECK_NE(reinterpret_cast<uintptr_t>(nullptr),
             reinterpret_cast<uintptr_t>(insertionPoint_));

  const auto expectedSize = sieve2_.size() >> config_.sieve2InsertionPointSpec;
  auto curr = insertionPoint_;

  while (tailSize_ < expectedSize && curr != sieve2_.getHead()) {
    curr = sieve2_.getPrev(*curr);
    markTail(*curr);
    tailSize_++;
  }

  while (tailSize_ > expectedSize && curr != sieve2_.getTail()) {
    unmarkTail(*curr);
    tailSize_--;
    curr = sieve2_.getNext(*curr);
  }

  insertionPoint_ = curr;
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
bool MMSieve2::Container<T, HookPtr>::add(T& node) noexcept {
  const auto currTime = static_cast<Time>(util::getCurrentTimeSec());

  return sieve2Mutex_->lock_combine([this, &node, currTime]() {
    if (node.isInMMContainer()) {
      return false;
    }
    if (config_.sieve2InsertionPointSpec == 0 || insertionPoint_ == nullptr) {
      sieve2_.linkAtHead(node);
    } else {
      sieve2_.insertBefore(*insertionPoint_, node);
    }
    node.markInMMContainer();
    setUpdateTime(node, currTime);
    unmarkAccessed(node);
    updatesieve2InsertionPoint();
    return true;
  });
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
typename MMSieve2::Container<T, HookPtr>::LockedIterator
MMSieve2::Container<T, HookPtr>::getEvictionIterator() const noexcept {
  LockHolder l(*sieve2Mutex_);
  return LockedIterator{std::move(l), sieve2_.rbegin()};
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
template <typename F>
void MMSieve2::Container<T, HookPtr>::withEvictionIterator(F&& fun) {
  if (config_.useCombinedLockForIterators) {
    sieve2Mutex_->lock_combine(
        [this, &fun]() { fun(Iterator{sieve2_.rbegin()}); });
  } else {
    LockHolder lck{*sieve2Mutex_};
    fun(Iterator{sieve2_.rbegin()});
  }
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
void MMSieve2::Container<T, HookPtr>::ensureNotInsertionPoint(
    T& node) noexcept {
  // If we are removing the insertion point node, grow tail before we remove
  // so that insertionPoint_ is valid (or nullptr) after removal
  if (&node == insertionPoint_) {
    insertionPoint_ = sieve2_.getPrev(*insertionPoint_);
    if (insertionPoint_ != nullptr) {
      tailSize_++;
      markTail(*insertionPoint_);
    } else {
      XDCHECK_EQ(sieve2_.size(), 1u);
    }
  }
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
void MMSieve2::Container<T, HookPtr>::removeLocked(T& node) {
  ensureNotInsertionPoint(node);
  sieve2_.remove(node);
  unmarkAccessed(node);
  if (isTail(node)) {
    unmarkTail(node);
    tailSize_--;
  }
  node.unmarkInMMContainer();
  updatesieve2InsertionPoint();
  return;
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
bool MMSieve2::Container<T, HookPtr>::remove(T& node) noexcept {
  return sieve2Mutex_->lock_combine([this, &node]() {
    if (!node.isInMMContainer()) {
      return false;
    }
    removeLocked(node);
    return true;
  });
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
void MMSieve2::Container<T, HookPtr>::remove(Iterator& it) noexcept {
  T& node = *it;
  XDCHECK(node.isInMMContainer());
  ++it;
  removeLocked(node);
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
bool MMSieve2::Container<T, HookPtr>::replace(T& oldNode, T& newNode) noexcept {
  return sieve2Mutex_->lock_combine([this, &oldNode, &newNode]() {
    if (!oldNode.isInMMContainer() || newNode.isInMMContainer()) {
      return false;
    }
    const auto updateTime = getUpdateTime(oldNode);
    sieve2_.replace(oldNode, newNode);
    oldNode.unmarkInMMContainer();
    newNode.markInMMContainer();
    setUpdateTime(newNode, updateTime);
    if (isAccessed(oldNode)) {
      markAccessed(newNode);
    } else {
      unmarkAccessed(newNode);
    }
    XDCHECK(!isTail(newNode));
    if (isTail(oldNode)) {
      markTail(newNode);
      unmarkTail(oldNode);
    } else {
      unmarkTail(newNode);
    }
    if (insertionPoint_ == &oldNode) {
      insertionPoint_ = &newNode;
    }
    return true;
  });
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
serialization::MMSieve2Object MMSieve2::Container<T, HookPtr>::saveState()
    const noexcept {
  serialization::MMSieve2Config configObject;
  *configObject.sieve2RefreshTime() =
      sieve2RefreshTime_.load(std::memory_order_relaxed);
  *configObject.sieve2RefreshRatio() = config_.sieve2RefreshRatio;
  *configObject.updateOnWrite() = config_.updateOnWrite;
  *configObject.updateOnRead() = config_.updateOnRead;
  *configObject.tryLockUpdate() = config_.tryLockUpdate;
  *configObject.sieve2InsertionPointSpec() = config_.sieve2InsertionPointSpec;

  serialization::MMSieve2Object object;
  *object.config() = configObject;
  *object.compressedInsertionPoint() =
      compressor_.compress(insertionPoint_).saveState();
  *object.tailSize() = tailSize_;
  *object.sieve2() = sieve2_.saveState();
  return object;
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
MMContainerStat MMSieve2::Container<T, HookPtr>::getStats() const noexcept {
  auto stat = sieve2Mutex_->lock_combine([this]() {
    auto* tail = sieve2_.getTail();

    // we return by array here because DistributedMutex is fastest when the
    // output data fits within 48 bytes.  And the array is exactly 48 bytes, so
    // it can get optimized by the implementation.
    //
    // the rest of the parameters are 0, so we don't need the critical section
    // to return them
    return folly::make_array(
        sieve2_.size(), tail == nullptr ? 0 : getUpdateTime(*tail),
        sieve2RefreshTime_.load(std::memory_order_relaxed));
  });
  return {stat[0] /* sieve2 size */,
          stat[1] /* tail time */,
          stat[2] /* refresh time */,
          0,
          0,
          0,
          0};
}

template <typename T, MMSieve2::Hook<T> T::*HookPtr>
void MMSieve2::Container<T, HookPtr>::reconfigureLocked(const Time& currTime) {
  if (currTime < nextReconfigureTime_) {
    return;
  }
  nextReconfigureTime_ = currTime + config_.mmReconfigureIntervalSecs.count();

  // update sieve2 refresh time
  auto stat = getEvictionAgeStatLocked(0);
  auto sieve2RefreshTime = std::min(
      std::max(config_.defaultsieve2RefreshTime,
               static_cast<uint32_t>(stat.warmQueueStat.oldestElementAge *
                                     config_.sieve2RefreshRatio)),
      ksieve2RefreshTimeCap);
  sieve2RefreshTime_.store(sieve2RefreshTime, std::memory_order_relaxed);
}

// Iterator Context Implementation
template <typename T, MMSieve2::Hook<T> T::*HookPtr>
MMSieve2::Container<T, HookPtr>::LockedIterator::LockedIterator(
    LockHolder l, const Iterator& iter) noexcept
    : Iterator(iter), l_(std::move(l)) {}

}  // namespace cachelib
}  // namespace facebook
