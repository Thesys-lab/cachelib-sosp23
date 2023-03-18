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

#include <folly/AtomicHashMap.h>
#include <folly/MPMCQueue.h>
#include <folly/logging/xlog.h>

#include <algorithm>
#include <atomic>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include "cachelib/allocator/serialize/gen-cpp2/objects_types.h"
#pragma GCC diagnostic pop

#include <folly/lang/Aligned.h>
#include <folly/synchronization/DistributedMutex.h>

#include "cachelib/allocator/datastruct/AtomicDList.h"
#include "cachelib/allocator/datastruct/AtomicFIFOHashTable.h"
#include "cachelib/allocator/datastruct/DList.h"
#include "cachelib/common/BloomFilter.h"
#include "cachelib/common/CompilerUtils.h"
#include "cachelib/common/Mutex.h"

namespace facebook {
namespace cachelib {

template <typename T, AtomicDListHook<T> T::*HookPtr>
class QDList {
 public:
  using Mutex = folly::DistributedMutex;
  using LockHolder = std::unique_lock<Mutex>;
  using CompressedPtr = typename T::CompressedPtr;
  using PtrCompressor = typename T::PtrCompressor;
  using ADList = AtomicDList<T, HookPtr>;
  using RefFlags = typename T::Flags;
  using QDListObject = serialization::QDListObject;

  QDList() = default;
  QDList(const QDList&) = delete;
  QDList& operator=(const QDList&) = delete;

  QDList(PtrCompressor compressor) noexcept {
    pfifo_ = std::make_unique<ADList>(compressor);
    mfifo_ = std::make_unique<ADList>(compressor);
  }

  // Restore QDList from saved state.
  //
  // @param object              Save QDList object
  // @param compressor          PtrCompressor object
  QDList(const QDListObject& object, PtrCompressor compressor) {
    pfifo_ = std::make_unique<ADList>(*object.pfifo(), compressor);
    mfifo_ = std::make_unique<ADList>(*object.mfifo(), compressor);
  }

  /**
   * Exports the current state as a thrift object for later restoration.
   */
  QDListObject saveState() const {
    QDListObject state;
    *state.pfifo() = pfifo_->saveState();
    *state.mfifo() = mfifo_->saveState();
    return state;
  }

  // SingleCList& getList(int index) const noexcept {
  //   return *(lists_[index].get());
  // }

  ADList& getListProbationary() const noexcept { return *pfifo_; }

  ADList& getListMain() const noexcept { return *mfifo_; }

  T* getTail() const noexcept { return pfifo_->getTail(); }

  size_t size() const noexcept { return pfifo_->size() + mfifo_->size(); }

  T* getEvictionCandidate() noexcept;

  void add(T& node) noexcept {
    if (hist_.initialized() && hist_.contains(hashNode(node))) {
        mfifo_->linkAtHead(node);
        markMain(node);
        unmarkProbationary(node);
    } else {
      pfifo_->linkAtHead(node);
      markProbationary(node);
      unmarkMain(node);
    }
  }

  // Bit MM_BIT_0 is used to record if the item is hot.
  void markProbationary(T& node) noexcept {
    node.template setFlag<RefFlags::kMMFlag0>();
  }

  void unmarkProbationary(T& node) noexcept {
    node.template unSetFlag<RefFlags::kMMFlag0>();
  }

  bool isProbationary(const T& node) const noexcept {
    return node.template isFlagSet<RefFlags::kMMFlag0>();
  }

  // Bit MM_BIT_2 is used to record if the item is cold.
  void markMain(T& node) noexcept {
    node.template setFlag<RefFlags::kMMFlag2>();
  }

  void unmarkMain(T& node) noexcept {
    node.template unSetFlag<RefFlags::kMMFlag2>();
  }

  bool isMain(const T& node) const noexcept {
    return node.template isFlagSet<RefFlags::kMMFlag2>();
  }

  // T* getNext(const T& node) const noexcept {
  //   return (node.*HookPtr).getNext(compressor_);
  // }

  // T* getPrev(const T& node) const noexcept {
  //   return (node.*HookPtr).getPrev(compressor_);
  // }

  // T* getEvictionCandidateProbationary() noexcept;

  // T* getEvictionCandidateMain() noexcept;

  // // Iterator interface for the double linked list. Supports both iterating
  // // from the tail and head.
  // class Iterator {
  //  public:
  //   enum class Direction { FROM_HEAD, FROM_TAIL };

  //   // Initializes the iterator to the beginning.
  //   explicit Iterator(const QDList<T, HookPtr>& mlist) noexcept
  //       : currIter_(mlist.lists_[mlist.lists_.size() - 1]->rbegin()),
  //         mlist_(mlist) {
  //     resetToBegin();
  //     // We should either point to an element or the end() iterator
  //     // which has an invalid index_.
  //     XDCHECK(index_ == kInvalidIndex || currIter_.get() != nullptr);
  //   }

  //   explicit Iterator(const QDList<T, HookPtr>& mlist, size_t listIdx)
  //   noexcept
  //       : currIter_(mlist.lists_[mlist.lists_.size() - 1]->rbegin()),
  //         mlist_(mlist) {
  //     XDCHECK_LT(listIdx, mlist.lists_.size());
  //     initToValidRBeginFrom(listIdx);
  //     // We should either point to an element or the end() iterator
  //     // which has an invalid index_.
  //     XDCHECK(index_ == kInvalidIndex || currIter_.get() != nullptr);
  //   }

  //   virtual ~Iterator() = default;

  //   // copyable and movable
  //   Iterator(const Iterator&) = default;
  //   Iterator& operator=(const Iterator&) = default;
  //   Iterator(Iterator&&) noexcept = default;
  //   Iterator& operator=(Iterator&&) noexcept = default;

  //   // moves the iterator forward and backward. Calling ++ once the
  //   iterator
  //   // has reached the end is undefined.
  //   Iterator& operator++() {  goForward(); return *this; };
  //   Iterator& operator--() { goBackward(); return *this; };

  //   T* operator->() const noexcept { return currIter_.operator->(); }
  //   T& operator*() const noexcept { return currIter_.operator*(); }

  //   bool operator==(const Iterator& other) const noexcept {
  //     return &mlist_ == &other.mlist_ && currIter_ == other.currIter_ &&
  //            index_ == other.index_;
  //   }

  //   bool operator!=(const Iterator& other) const noexcept {
  //     return !(*this == other);
  //   }

  //   // explicit operator bool() const noexcept {
  //   //   return curr_ != nullptr && QDList_ != nullptr;
  //   // }

  //   explicit operator bool() const noexcept {
  //     return index_ < mlist_.lists_.size();
  //   }

  //   T* get() const noexcept { return currIter_.get(); }

  //   // Invalidates this iterator
  //   void reset() noexcept {
  //     // Set index to before first list
  //     index_ = kInvalidIndex;
  //     // Point iterator to first list's rend
  //     currIter_ = mlist_.lists_[0]->rend();
  //   }

  //   // Reset the iterator back to the beginning
  //   void resetToBegin() noexcept {
  //     initToValidRBeginFrom(mlist_.lists_.size() - 1);
  //   }

  //  protected:
  //   void goForward() noexcept;
  //   void goBackward() noexcept;

  //   // reset iterator to the beginning of a speicific queue
  //   void initToValidRBeginFrom(size_t listIdx) noexcept;

  //   // Index of current list
  //   size_t index_{0};
  //   // the current position of the iterator in the list
  //   CListIterator currIter_;
  //   const QDList<T, HookPtr>& mlist_;

  //   static constexpr size_t kInvalidIndex =
  //   std::numeric_limits<size_t>::max();
  // };

  // // provides an iterator starting from the tail of the linked list.
  // Iterator rbegin() const noexcept;

  // // provides an iterator starting from the tail of the linked list.
  // Iterator rbegin(size_t idx) const;

  // // Iterator to compare against for the end.
  // Iterator rend() const noexcept;

 private:
  static uint32_t hashNode(const T& node) noexcept {
    return static_cast<uint32_t>(
        folly::hasher<folly::StringPiece>()(node.getKey()));
  }

  void prepEvictCandProbationary() noexcept;

  void prepEvictCandMain() noexcept;

  std::unique_ptr<ADList> pfifo_;

  std::unique_ptr<ADList> mfifo_;

  mutable folly::cacheline_aligned<Mutex> mtx_;

  constexpr static double pRatio_ = 0.1;

  AtomicFIFOHashTable hist_;

};
} // namespace cachelib
} // namespace facebook

#include "cachelib/allocator/datastruct/QDList-inl.h"
