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

#include <folly/MPMCQueue.h>
#include <folly/logging/xlog.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include "cachelib/allocator/serialize/gen-cpp2/objects_types.h"
#pragma GCC diagnostic pop
#include <folly/lang/Aligned.h>
#include <folly/synchronization/DistributedMutex.h>

#include <atomic>
#include <algorithm>

#include "cachelib/common/CompilerUtils.h"
#include "cachelib/common/Mutex.h"

namespace facebook {
namespace cachelib {

// node information for the double linked list modelling the lru. It has the
// previous, next information with the last time the item was updated in the
// LRU.
template <typename T>
struct CACHELIB_PACKED_ATTR AtomicClockListBufferedHook {
  using Time = uint32_t;
  using CompressedPtr = typename T::CompressedPtr;
  using PtrCompressor = typename T::PtrCompressor;

  void setNext(T* const n, const PtrCompressor& compressor) noexcept {
    next_ = compressor.compress(n);
  }

  void setNext(CompressedPtr next) noexcept { next_ = next; }

  void setPrev(T* const p, const PtrCompressor& compressor) noexcept {
    prev_ = compressor.compress(p);
  }

  void setPrev(CompressedPtr prev) noexcept { prev_ = prev; }

  CompressedPtr getNext() const noexcept { return CompressedPtr(next_); }

  T* getNext(const PtrCompressor& compressor) const noexcept {
    return compressor.unCompress(next_);
  }

  CompressedPtr getPrev() const noexcept { return CompressedPtr(prev_); }

  T* getPrev(const PtrCompressor& compressor) const noexcept {
    return compressor.unCompress(prev_);
  }

  // set and get the time when the node was updated in the lru.
  void setUpdateTime(Time time) noexcept { updateTime_ = time; }

  Time getUpdateTime() const noexcept {
    // Suppress TSAN here because we don't care if an item is promoted twice by
    // two get operations running concurrently. It should be very rarely and is
    // just a minor inefficiency if it happens.
    folly::annotate_ignore_thread_sanitizer_guard g(__FILE__, __LINE__);
    return updateTime_;
  }

 private:
  CompressedPtr next_{};  // next node in the linked list
  CompressedPtr prev_{};  // previous node in the linked list
  // timestamp when this was last updated to the head of the list
  Time updateTime_{0};
  //   int32_t visited_{0};
};

// uses a double linked list to implement an LRU. T must be have a public
// member of type Hook and HookPtr must point to that.
template <typename T, AtomicClockListBufferedHook<T> T::*HookPtr>
class AtomicClockListBuffered {
 public:
  using Mutex = folly::DistributedMutex;
  using LockHolder = std::unique_lock<Mutex>;
  using RefFlags = typename T::Flags;
  using CompressedPtr = typename T::CompressedPtr;
  using PtrCompressor = typename T::PtrCompressor;
  using AtomicClockListBufferedObject = serialization::AtomicClockListBufferedObject;

  AtomicClockListBuffered() = default;
  AtomicClockListBuffered(const AtomicClockListBuffered&) = delete;
  AtomicClockListBuffered& operator=(const AtomicClockListBuffered&) = delete;

  explicit AtomicClockListBuffered(PtrCompressor compressor) noexcept
      : compressor_(std::move(compressor)) {}

  // Restore AtomicClockListBuffered from saved state.
  //
  // @param object              Save AtomicClockListBuffered object
  // @param compressor          PtrCompressor object
  AtomicClockListBuffered(const AtomicClockListBufferedObject& object, PtrCompressor compressor)
      : compressor_(std::move(compressor)),
        head_(compressor_.unCompress(CompressedPtr{*object.compressedHead()})),
        tail_(compressor_.unCompress(CompressedPtr{*object.compressedTail()})),
        size_(*object.size()) {}

  /**
   * Exports the current state as a thrift object for later restoration.
   */
  AtomicClockListBufferedObject saveState() const {
    AtomicClockListBufferedObject state;
    *state.compressedHead() = compressor_.compress(head_).saveState();
    *state.compressedTail() = compressor_.compress(tail_).saveState();
    *state.size() = size_;
    return state;
  }

  T* getNext(const T& node) const noexcept {
    return (node.*HookPtr).getNext(compressor_);
  }

  T* getPrev(const T& node) const noexcept {
    return (node.*HookPtr).getPrev(compressor_);
  }

  void setNext(T& node, T* next) noexcept {
    (node.*HookPtr).setNext(next, compressor_);
  }

  void setNextFrom(T& node, const T& other) noexcept {
    (node.*HookPtr).setNext((other.*HookPtr).getNext());
  }

  void setPrev(T& node, T* prev) noexcept {
    (node.*HookPtr).setPrev(prev, compressor_);
  }

  void setPrevFrom(T& node, const T& other) noexcept {
    (node.*HookPtr).setPrev((other.*HookPtr).getPrev());
  }

  // Links the passed node to the head of the double linked list
  // @param node node to be linked at the head
  void linkAtHead(T& node) noexcept;

  // Add node before nextNode.
  //
  // @param nextNode    node before which to insert
  // @param node        node to insert
  // @note nextNode must be in the list and node must not be in the list
  // void insertBefore(T& nextNode, T& node) noexcept;

  // removes the node completely from the linked list and cleans up the node
  // appropriately by setting its next and prev as nullptr.
  void remove(T& node) noexcept;

  // Unlinks the destination node and replaces it with the source node
  //
  // @param oldNode   destination node
  // @param newNode   source node
  void replace(T& oldNode, T& newNode) noexcept;

  // moves a node that belongs to the linked list to the head of the linked
  // list.
  void moveToHead(T& node) noexcept;

  T* getHead() const noexcept { return head_; }
  T* getTail() const noexcept { return tail_; }

  size_t size() const noexcept { return size_; }

  // Iterator interface for the double linked list. Supports both iterating
  // from the tail and head.
  class Iterator {
   public:
    enum class Direction { FROM_HEAD, FROM_TAIL };

    Iterator(T* p, Direction d,
             const AtomicClockListBuffered<T, HookPtr>& AtomicClockListBuffered) noexcept
        : curr_(p), dir_(d), AtomicClockListBuffered_(&AtomicClockListBuffered) {}
    virtual ~Iterator() = default;

    // copyable and movable
    Iterator(const Iterator&) = default;
    Iterator& operator=(const Iterator&) = default;
    Iterator(Iterator&&) noexcept = default;
    Iterator& operator=(Iterator&&) noexcept = default;

    // moves the iterator forward and backward. Calling ++ once the iterator
    // has reached the end is undefined.
    Iterator& operator++() noexcept;
    Iterator& operator--() noexcept;

    T* operator->() const noexcept { return curr_; }
    T& operator*() const noexcept { return *curr_; }

    bool operator==(const Iterator& other) const noexcept {
      return AtomicClockListBuffered_ == other.AtomicClockListBuffered_ &&
             curr_ == other.curr_ && dir_ == other.dir_;
    }

    bool operator!=(const Iterator& other) const noexcept {
      return !(*this == other);
    }

    explicit operator bool() const noexcept {
      return curr_ != nullptr && AtomicClockListBuffered_ != nullptr;
    }

    T* get() const noexcept { return curr_; }

    // Invalidates this iterator
    void reset() noexcept { curr_ = nullptr; }

    // Reset the iterator back to the beginning
    void resetToBegin() noexcept {
      curr_ = dir_ == Direction::FROM_HEAD ? AtomicClockListBuffered_->head_
                                           : AtomicClockListBuffered_->tail_;
    }

   protected:
    void goForward() noexcept;
    void goBackward() noexcept;

    // the current position of the iterator in the list
    T* curr_{nullptr};
    // the direction we are iterating.
    Direction dir_{Direction::FROM_HEAD};
    const AtomicClockListBuffered<T, HookPtr>* AtomicClockListBuffered_{nullptr};
  };

  // provides an iterator starting from the head of the linked list.
  Iterator begin() const noexcept;

  // provides an iterator starting from the tail of the linked list.
  Iterator rbegin() const noexcept;

  // Iterator to compare against for the end.
  Iterator end() const noexcept;
  Iterator rend() const noexcept;

  T* getEvictionCandidate() noexcept;

 private:
  // unlinks the node from the linked list. Does not correct the next and
  // previous.
  void unlink(const T& node) noexcept;

  void prepareEvictionCandidates() noexcept;

  void markAccessed(T& node) noexcept {
    node.template setFlag<RefFlags::kMMFlag1>();
  }

  void unmarkAccessed(T& node) noexcept {
    node.template unSetFlag<RefFlags::kMMFlag1>();
  }

  bool isAccessed(const T& node) const noexcept {
    return node.template isFlagSet<RefFlags::kMMFlag1>();
  }

  const PtrCompressor compressor_{};

  mutable folly::cacheline_aligned<Mutex> mtx_;

  constexpr static size_t nMaxEvictionCandidates_ = 64;

  // head of the linked list
  std::atomic<T*> head_{nullptr};

  // tail of the linked list
  std::atomic<T*> tail_{nullptr};

  // AtomicClock hand
  std::atomic<T*> curr_{nullptr};

  // size of the list
  std::atomic<size_t> size_{0};

// #define USE_MYCLOCK_ATOMIC
// #define USE_NO_BUFFER
#define USE_MPMC_QUEUE

#ifdef USE_EVICTION_BUFFER
  std::atomic<size_t> nEvictionCandidates_{0};

  std::atomic<T*> evictCandidateBuf_[nMaxEvictionCandidates_]{nullptr};

  std::atomic<size_t> bufIdx_{nMaxEvictionCandidates_};

  size_t nCandidateToPrepare() {
    size_t n = 0;
    n = std::min(size_.load() / 4, nMaxEvictionCandidates_);
    n = std::max(n, 1ul);
    return n;
  }
#endif
#ifdef USE_MPMC_QUEUE
  folly::MPMCQueue<T*> evictCandidateQueue_{nMaxEvictionCandidates_};

  /* different from previous one - we load 1/4 of the nMax */
  size_t nCandidateToPrepare() {
    size_t n = 0;
    n = std::min(size_.load(), nMaxEvictionCandidates_);
    n = std::max(n / 4, 1ul);
    return n;
  }

#endif
};
}  // namespace cachelib
}  // namespace facebook

#include "cachelib/allocator/datastruct/AtomicClockListBuffered-inl.h"
