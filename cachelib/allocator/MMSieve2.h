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

#include <atomic>
#include <cstring>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <folly/Format.h>
#pragma GCC diagnostic pop
#include <folly/container/Array.h>
#include <folly/lang/Aligned.h>
#include <folly/synchronization/DistributedMutex.h>

#include "cachelib/allocator/Cache.h"
#include "cachelib/allocator/CacheStats.h"
#include "cachelib/allocator/Util.h"
#include "cachelib/allocator/datastruct/DList2.h"
#include "cachelib/allocator/memory/serialize/gen-cpp2/objects_types.h"
#include "cachelib/common/CompilerUtils.h"
#include "cachelib/common/Mutex.h"

namespace facebook {
namespace cachelib {

class MMSieve2 {
 public:
  // unique identifier per MMType
  static const int kId;

  // forward declaration;
  template <typename T>
  using Hook = DList2Hook<T>;
  using SerializationType = serialization::MMSieve2Object;
  using SerializationConfigType = serialization::MMSieve2Config;
  using SerializationTypeContainer = serialization::MMSieve2Collection;

  // This is not applicable for MMSieve2, just for compile of cache allocator
  enum LruType { NumTypes };

  // Config class for MMSieve2
  struct Config {
    // create from serialized config
    explicit Config(SerializationConfigType configState)
        : Config(
              *configState.sieve2RefreshTime(),
              *configState.sieve2RefreshRatio(), *configState.updateOnWrite(),
              *configState.updateOnRead(), *configState.tryLockUpdate(),
              static_cast<uint8_t>(*configState.sieve2InsertionPointSpec())) {}

    // @param time        the sieve2 refresh time in seconds.
    //                    An item will be promoted only once in each sieve2
    //                    refresh time depite the number of accesses it gets.
    // @param udpateOnW   whether to promote the item on write
    // @param updateOnR   whether to promote the item on read
    Config(uint32_t time, bool updateOnW, bool updateOnR)
        : Config(time, updateOnW, updateOnR, false, 0) {}

    // @param time        the sieve2 refresh time in seconds.
    //                    An item will be promoted only once in each sieve2
    //                    refresh time depite the number of accesses it gets.
    // @param udpateOnW   whether to promote the item on write
    // @param updateOnR   whether to promote the item on read
    // @param tryLockU    whether to use a try lock when doing update.
    // @param ipSpec      insertion point spec, which is the inverse power of
    //                    length from the end of the queue. For example, value 1
    //                    means the insertion point is 1/2 from the end of
    //                    sieve2; value 2 means 1/4 from the end of sieve2.
    Config(uint32_t time, bool updateOnW, bool updateOnR, uint8_t ipSpec)
        : Config(time, updateOnW, updateOnR, false, ipSpec) {}

    Config(uint32_t time, bool updateOnW, bool updateOnR, bool tryLockU,
           uint8_t ipSpec)
        : Config(time, 0., updateOnW, updateOnR, tryLockU, ipSpec) {}

    // @param time        the sieve2 refresh time in seconds.
    //                    An item will be promoted only once in each sieve2
    //                    refresh time depite the number of accesses it gets.
    // @param ratio       the sieve2 refresh ratio. The ratio times the
    //                    oldest element's lifetime in warm queue
    //                    would be the minimum value of sieve2 refresh time.
    // @param udpateOnW   whether to promote the item on write
    // @param updateOnR   whether to promote the item on read
    // @param tryLockU    whether to use a try lock when doing update.
    // @param ipSpec      insertion point spec, which is the inverse power of
    //                    length from the end of the queue. For example, value 1
    //                    means the insertion point is 1/2 from the end of
    //                    sieve2; value 2 means 1/4 from the end of sieve2.
    Config(uint32_t time, double ratio, bool updateOnW, bool updateOnR,
           bool tryLockU, uint8_t ipSpec)
        : Config(time, ratio, updateOnW, updateOnR, tryLockU, ipSpec, 0) {}

    // @param time        the sieve2 refresh time in seconds.
    //                    An item will be promoted only once in each sieve2
    //                    refresh time depite the number of accesses it gets.
    // @param ratio       the sieve2 refresh ratio. The ratio times the
    //                    oldest element's lifetime in warm queue
    //                    would be the minimum value of sieve2 refresh time.
    // @param udpateOnW   whether to promote the item on write
    // @param updateOnR   whether to promote the item on read
    // @param tryLockU    whether to use a try lock when doing update.
    // @param ipSpec      insertion point spec, which is the inverse power of
    //                    length from the end of the queue. For example, value 1
    //                    means the insertion point is 1/2 from the end of
    //                    sieve2; value 2 means 1/4 from the end of sieve2.
    // @param mmReconfigureInterval   Time interval for recalculating sieve2
    //                                refresh time according to the ratio.
    Config(uint32_t time, double ratio, bool updateOnW, bool updateOnR,
           bool tryLockU, uint8_t ipSpec, uint32_t mmReconfigureInterval)
        : Config(time, ratio, updateOnW, updateOnR, tryLockU, ipSpec,
                 mmReconfigureInterval, false) {}

    // @param time        the sieve2 refresh time in seconds.
    //                    An item will be promoted only once in each sieve2
    //                    refresh time depite the number of accesses it gets.
    // @param ratio       the sieve2 refresh ratio. The ratio times the
    //                    oldest element's lifetime in warm queue
    //                    would be the minimum value of sieve2 refresh time.
    // @param udpateOnW   whether to promote the item on write
    // @param updateOnR   whether to promote the item on read
    // @param tryLockU    whether to use a try lock when doing update.
    // @param ipSpec      insertion point spec, which is the inverse power of
    //                    length from the end of the queue. For example, value 1
    //                    means the insertion point is 1/2 from the end of
    //                    sieve2; value 2 means 1/4 from the end of sieve2.
    // @param mmReconfigureInterval   Time interval for recalculating sieve2
    //                                refresh time according to the ratio.
    // useCombinedLockForIterators    Whether to use combined locking for
    //                                withEvictionIterator
    Config(uint32_t time, double ratio, bool updateOnW, bool updateOnR,
           bool tryLockU, uint8_t ipSpec, uint32_t mmReconfigureInterval,
           bool useCombinedLockForIterators)
        : defaultsieve2RefreshTime(time),
          sieve2RefreshRatio(ratio),
          updateOnWrite(updateOnW),
          updateOnRead(updateOnR),
          tryLockUpdate(tryLockU),
          sieve2InsertionPointSpec(ipSpec),
          mmReconfigureIntervalSecs(
              std::chrono::seconds(mmReconfigureInterval)),
          useCombinedLockForIterators(useCombinedLockForIterators) {}

    Config() = default;
    Config(const Config& rhs) = default;
    Config(Config&& rhs) = default;

    Config& operator=(const Config& rhs) = default;
    Config& operator=(Config&& rhs) = default;

    template <typename... Args>
    void addExtraConfig(Args...) {}

    // threshold value in seconds to compare with a node's update time to
    // determine if we need to update the position of the node in the linked
    // list. By default this is 60s to reduce the contention on the sieve2 lock.
    uint32_t defaultsieve2RefreshTime{60};
    uint32_t sieve2RefreshTime{defaultsieve2RefreshTime};

    // ratio of sieve2 refresh time to the tail age. If a refresh time computed
    // according to this ratio is larger than sieve2Refreshtime, we will adopt
    // this one instead of the sieve2RefreshTime set.
    double sieve2RefreshRatio{0.};

    // whether the sieve2 needs to be updated on writes for recordAccess. If
    // false, accessing the cache for writes does not promote the cached item
    // to the head of the sieve2.
    bool updateOnWrite{false};

    // whether the sieve2 needs to be updated on reads for recordAccess. If
    // false, accessing the cache for reads does not promote the cached item
    // to the head of the sieve2.
    bool updateOnRead{true};

    // whether to tryLock or lock the sieve2 lock when attempting promotion on
    // access. If set, and tryLock fails, access will not result in promotion.
    bool tryLockUpdate{false};

    // By default insertions happen at the head of the sieve2. If we need
    // insertions at the middle of sieve2 we can adjust this to be a non-zero.
    // Ex: sieve2InsertionPointSpec = 1, we insert at the middle (1/2 from end)
    //     sieve2InsertionPointSpec = 2, we insert at a point 1/4th from tail
    uint8_t sieve2InsertionPointSpec{0};

    // Minimum interval between reconfigurations. If 0, reconfigure is never
    // called.
    std::chrono::seconds mmReconfigureIntervalSecs{};

    // Whether to use combined locking for withEvictionIterator.
    bool useCombinedLockForIterators{false};
  };

  // The container object which can be used to keep track of objects of type
  // T. T must have a public member of type Hook. This object is wrapper
  // around DList2, is thread safe and can be accessed from multiple threads.
  // The current implementation models an sieve2 using the above DList2
  // implementation.
  template <typename T, Hook<T> T::*HookPtr>
  struct Container {
   private:
    using sieve2List = DList2<T, HookPtr>;
    using Mutex = folly::DistributedMutex;
    using LockHolder = std::unique_lock<Mutex>;
    using PtrCompressor = typename T::PtrCompressor;
    using Time = typename Hook<T>::Time;
    using CompressedPtr = typename T::CompressedPtr;
    using RefFlags = typename T::Flags;

   public:
    Container() = default;
    Container(Config c, PtrCompressor compressor)
        : compressor_(std::move(compressor)),
          sieve2_(compressor_),
          config_(std::move(c)) {
      sieve2RefreshTime_ = config_.sieve2RefreshTime;
      nextReconfigureTime_ =
          config_.mmReconfigureIntervalSecs.count() == 0
              ? std::numeric_limits<Time>::max()
              : static_cast<Time>(util::getCurrentTimeSec()) +
                    config_.mmReconfigureIntervalSecs.count();
    }
    Container(serialization::MMSieve2Object object, PtrCompressor compressor);

    Container(const Container&) = delete;
    Container& operator=(const Container&) = delete;

    using Iterator = typename sieve2List::Iterator;

    // context for iterating the MM container. At any given point of time,
    // there can be only one iterator active since we need to lock the sieve2
    // for iteration. we can support multiple iterators at same time, by using a
    // shared ptr in the context for the lock holder in the future.
    class LockedIterator : public Iterator {
     public:
      // noncopyable but movable.
      LockedIterator(const LockedIterator&) = delete;
      LockedIterator& operator=(const LockedIterator&) = delete;

      LockedIterator(LockedIterator&&) noexcept = default;

      // 1. Invalidate this iterator
      // 2. Unlock
      void destroy() {
        Iterator::reset();
        if (l_.owns_lock()) {
          l_.unlock();
        }
      }

      // Reset this iterator to the beginning
      void resetToBegin() {
        if (!l_.owns_lock()) {
          l_.lock();
        }
        Iterator::resetToBegin();
      }

     private:
      // private because it's easy to misuse and cause deadlock for MMSieve2
      LockedIterator& operator=(LockedIterator&&) noexcept = default;

      // create an sieve2 iterator with the lock being held.
      LockedIterator(LockHolder l, const Iterator& iter) noexcept;

      // only the container can create iterators
      friend Container<T, HookPtr>;

      // lock protecting the validity of the iterator
      LockHolder l_;
    };

    // records the information that the node was accessed. This could bump up
    // the node to the head of the sieve2 depending on the time when the node
    // was last updated in sieve2 and the ksieve2RefreshTime. If the node was
    // moved to the head in the sieve2, the node's updateTime will be updated
    // accordingly.
    //
    // @param node  node that we want to mark as relevant/accessed
    // @param mode  the mode for the access operation.
    //
    // @return      True if the information is recorded and bumped the node
    //              to the head of the sieve2, returns false otherwise
    bool recordAccess(T& node, AccessMode mode) noexcept;

    // adds the given node into the container and marks it as being present in
    // the container. The node is added to the head of the sieve2.
    //
    // @param node  The node to be added to the container.
    // @return  True if the node was successfully added to the container. False
    //          if the node was already in the contianer. On error state of node
    //          is unchanged.
    bool add(T& node) noexcept;

    // removes the node from the sieve2 and sets it previous and next to
    // nullptr.
    //
    // @param node  The node to be removed from the container.
    // @return  True if the node was successfully removed from the container.
    //          False if the node was not part of the container. On error, the
    //          state of node is unchanged.
    bool remove(T& node) noexcept;

    // same as the above but uses an iterator context. The iterator is updated
    // on removal of the corresponding node to point to the next node. The
    // iterator context is responsible for locking.
    //
    // iterator will be advanced to the next node after removing the node
    //
    // @param it    Iterator that will be removed
    void remove(Iterator& it) noexcept;

    // replaces one node with another, at the same position
    //
    // @param oldNode   node being replaced
    // @param newNode   node to replace oldNode with
    //
    // @return true  If the replace was successful. Returns false if the
    //               destination node did not exist in the container, or if the
    //               source node already existed.
    bool replace(T& oldNode, T& newNode) noexcept;

    // Obtain an iterator that start from the tail and can be used
    // to search for evictions. This iterator holds a lock to this
    // container and only one such iterator can exist at a time
    LockedIterator getEvictionIterator() const noexcept;

    // Execute provided function under container lock. Function gets
    // iterator passed as parameter.
    template <typename F>
    void withEvictionIterator(F&& f);

    // get copy of current config
    Config getConfig() const;

    // override the existing config with the new one.
    void setConfig(const Config& newConfig);

    bool isEmpty() const noexcept { return size() == 0; }

    // reconfigure the MMContainer: update refresh time according to current
    // tail age
    void reconfigureLocked(const Time& currTime);

    // returns the number of elements in the container
    size_t size() const noexcept {
      return sieve2Mutex_->lock_combine([this]() { return sieve2_.size(); });
    }

    // Returns the eviction age stats. See CacheStats.h for details
    EvictionAgeStat getEvictionAgeStat(uint64_t projectedLength) const noexcept;

    // for saving the state of the sieve2
    //
    // precondition:  serialization must happen without any reader or writer
    // present. Any modification of this object afterwards will result in an
    // invalid, inconsistent state for the serialized data.
    //
    serialization::MMSieve2Object saveState() const noexcept;

    // return the stats for this container.
    MMContainerStat getStats() const noexcept;

    static LruType getLruType(const T& /* node */) noexcept {
      return LruType{};
    }

   private:
    EvictionAgeStat getEvictionAgeStatLocked(
        uint64_t projectedLength) const noexcept;

    static Time getUpdateTime(const T& node) noexcept {
      return (node.*HookPtr).getUpdateTime();
    }

    static void setUpdateTime(T& node, Time time) noexcept {
      (node.*HookPtr).setUpdateTime(time);
    }

    // This function is invoked by remove or record access prior to
    // removing the node (or moving the node to head) to ensure that
    // the node being moved is not the insertion point and if it is
    // adjust it accordingly.
    void ensureNotInsertionPoint(T& node) noexcept;

    // update the sieve2 insertion point after doing an insert or removal.
    // We need to ensure the insertionPoint_ is set to the correct node
    // to maintain the tailSize_, for the next insertion.
    void updatesieve2InsertionPoint() noexcept;

    // remove node from sieve2 and adjust insertion points
    // @param node          node to remove
    void removeLocked(T& node);

    // Bit MM_BIT_0 is used to record if the item is in tail. This
    // is used to implement sieve2 insertion points
    void markTail(T& node) noexcept {
      node.template setFlag<RefFlags::kMMFlag0>();
    }

    void unmarkTail(T& node) noexcept {
      node.template unSetFlag<RefFlags::kMMFlag0>();
    }

    bool isTail(T& node) const noexcept {
      return node.template isFlagSet<RefFlags::kMMFlag0>();
    }

    // Bit MM_BIT_1 is used to record if the item has been accessed since
    // being written in cache. Unaccessed items are ignored when determining
    // projected update time.
    void markAccessed(T& node) noexcept {
      node.template setFlag<RefFlags::kMMFlag1>();
    }

    void unmarkAccessed(T& node) noexcept {
      node.template unSetFlag<RefFlags::kMMFlag1>();
    }

    bool isAccessed(const T& node) const noexcept {
      return node.template isFlagSet<RefFlags::kMMFlag1>();
    }

    // protects all operations on the sieve2. We never really just read the
    // state of the sieve2. Hence we dont really require a RW mutex at this
    // point of time.
    mutable folly::cacheline_aligned<Mutex> sieve2Mutex_;

    const PtrCompressor compressor_{};

    // the sieve2
    sieve2List sieve2_{};

    // insertion point
    T* insertionPoint_{nullptr};

    // size of tail after insertion point
    size_t tailSize_{0};

    // The next time to reconfigure the container.
    std::atomic<Time> nextReconfigureTime_{};

    // How often to promote an item in the eviction queue.
    std::atomic<uint32_t> sieve2RefreshTime_{};

    // Config for this sieve2.
    // Write access to the MMSieve2 Config is serialized.
    // Reads may be racy.
    Config config_{};

    // Max sieve2FreshTime.
    static constexpr uint32_t ksieve2RefreshTimeCap{900};

    FRIEND_TEST(MMSieve2Test, Reconfigure);
  };
};
}  // namespace cachelib
}  // namespace facebook

#include "cachelib/allocator/MMSieve2-inl.h"
