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

template <typename T, AtomicDListHook<T> T::*HookPtr>
T* QDList<T, HookPtr>::getEvictionCandidate() noexcept {
  // LockHolder l(*mtx_);
  T* curr = nullptr;
  // if (n % 100000000 == 0)
  //   XLOGF(INFO, "{} list {} size: {}, {}", n, (void*)this, pfifo_->size(),
  //         mfifo_->size());

  while (true) {
    if ((double)pfifo_->size() / (pfifo_->size() + mfifo_->size()) > pRatio_) {
      // evict from probationary FIFO
      while (pfifo_->size() > 0 && curr == nullptr) {
        curr = pfifo_->removeTail();
      }
      if (curr != nullptr && pfifo_->isAccessed(*curr)) {
        pfifo_->unmarkAccessed(*curr);
        XDCHECK(isProbationary(*curr));
        unmarkProbationary(*curr);
        markMain(*curr);
        mfifo_->linkAtHead(*curr);
        curr = nullptr;
      } else {
        return curr;
      }
    } else {
      while (mfifo_->size() > 1 && curr == nullptr) {
        curr = mfifo_->removeTail();
      }
      if (curr != nullptr && mfifo_->isAccessed(*curr)) {
        if (!isMain(*curr)) {
          printf("curr: %p is not main\n", curr);
          abort();
        }
        mfifo_->unmarkAccessed(*curr);
        mfifo_->linkAtHead(*curr);
        curr = nullptr;
      } else {
        return curr;
      }
    }
  }
  abort();
}

// template <typename T, AtomicDListHook<T> T::*HookPtr>
// T* QDList<T, HookPtr>::getEvictionCandidateProbationary() noexcept {
//   LockHolder l(*mtx_);
//   if (evictCandQueueP_.sizeGuess() < nMaxEvictionCandidates_ / 4) {
//     if (pfifo_.size() == 0) {
//       return getEvictionCandidateMain();
//     }

//     prepEvictCandProbationary();
//   }

//   T* ret = nullptr;
//   int nTries = 0;
//   while (!evictCandQueueP_.read(ret)) {
//     if ((nTries++) % 100 == 0) {
//       prepEvictCandProbationary();
//     } else {
//       if (nTries % 100000 == 0) {
//         XLOGF(WARN, "thread {} has {} attempts\n", pthread_self(), nTries);
//       }
//     }
//   }

//   return ret;
// }

// template <typename T, AtomicDListHook<T> T::*HookPtr>
// T* QDList<T, HookPtr>::getEvictionCandidateMain() noexcept {
//   LockHolder l(*mtx_);
//   if (evictCandQueueM_.sizeGuess() < nMaxEvictionCandidates_ / 4) {
//     if (mfifo_.size() == 0) {
//       XLOGF(WARN, "main size is zero, %zu\n", mfifo_.size());
//       return getEvictionCandidateProbationary();
//     }

//     prepEvictCandMain();
//   }

//   T* ret = nullptr;
//   int nTries = 0;
//   while (!evictCandQueueM_.read(ret)) {
//     if ((nTries++) % 100 == 0) {
//       prepEvictCandMain();
//     } else {
//       if (nTries % 100000 == 0) {
//         XLOGF(WARN, "thread {} has {} attempts\n", pthread_self(), nTries);
//       }
//     }
//   }

//   return ret;
// }

// template <typename T, AtomicDListHook<T> T::*HookPtr>
// void QDList<T, HookPtr>::prepEvictCandProbationary() noexcept {
//   T* curr;
//   while (evictCandQueueP_.sizeGuess() < nMaxEvictionCandidates_ / 4 * 3) {
//     curr = pfifo_->removeTail();
//     if (curr == nullptr) {
//       XLOG(WARN, "pfifo_.removeTail() returns nullptr");
//       abort();
//     }

//     if (pfifo_->isAccessed(*curr)) {
//       pfifo_->unmarkAccessed(*curr);
//       mfifo_->linkAtHead(*curr);
//     } else {
//       evictCandQueueP_.blockingWrite(curr);
//     }
//   }
// }

// template <typename T, AtomicDListHook<T> T::*HookPtr>
// void QDList<T, HookPtr>::prepEvictCandMain() noexcept {
//   // LockHolder l(*mtx_);
//   // if (evictCandQueueM_.sizeGuess() > nMaxEvictionCandidates_ / 4 * 3) {
//   //   return;
//   // }
//   // int nCandidate = nCandidateToPrepare();
//   // int n_iters = 0;

//   T* curr;
//   while (evictCandQueueM_.sizeGuess() < nMaxEvictionCandidates_ / 4 * 3) {
//     curr = mfifo_->removeTail();
//     if (curr == nullptr) {
//       XLOG(WARN, "mfifo_.removeTail() returns nullptr");
//       abort();
//     }

//     if (mfifo_->isAccessed(*curr)) {
//       mfifo_->unmarkAccessed(*curr);
//       mfifo_->linkAtHead(*curr);
//     } else {
//       evictCandQueueM_.blockingWrite(curr);
//     }
//   }
// }

/* Iterator Implementation */
// template <typename T, AtomicDListHook<T> T::*HookPtr>
// void QDList<T, HookPtr>::Iterator::goForward() noexcept {
//   if (index_ == kInvalidIndex) {
//     return; // Can't go any further
//   }
//   // Move iterator forward
//   ++currIter_;
//   // If we land at the rend of this list, move to the previous list.
//   while (index_ != kInvalidIndex &&
//          currIter_ == mlist_.lists_[index_]->rend()) {
//     --index_;
//     if (index_ != kInvalidIndex) {
//       currIter_ = mlist_.lists_[index_]->rbegin();
//     }
//   }
// }

// template <typename T, AtomicDListHook<T> T::*HookPtr>
// void QDList<T, HookPtr>::Iterator::goBackward() noexcept {
//   if (index_ == mlist_.lists_.size()) {
//     return; // Can't go backward
//   }
//   // If we're not at rbegin, we can go backward
//   if (currIter_ != mlist_.lists_[index_]->rbegin()) {
//     --currIter_;
//     return;
//   }
//   // We're at rbegin, jump to the head of the next list.
//   while (index_ < mlist_.lists_.size() &&
//          currIter_ == mlist_.lists_[index_]->rbegin()) {
//     ++index_;
//     if (index_ < mlist_.lists_.size()) {
//       currIter_ = CListIterator(mlist_.lists_[index_]->getHead(),
//                                 CListIterator::Direction::FROM_TAIL,
//                                 *(mlist_.lists_[index_].get()));
//     }
//   }
// }

// template <typename T, AtomicDListHook<T> T::*HookPtr>
// void QDList<T, HookPtr>::Iterator::initToValidRBeginFrom(
//     size_t listIdx) noexcept {
//   // Find the first non-empty list.
//   index_ = listIdx;
//   while (index_ != std::numeric_limits<size_t>::max() &&
//          mlist_.lists_[index_]->size() == 0) {
//     --index_;
//   }
//   currIter_ = index_ == std::numeric_limits<size_t>::max()
//                   ? mlist_.lists_[0]->rend()
//                   : mlist_.lists_[index_]->rbegin();
// }

// template <typename T, AtomicDListHook<T> T::*HookPtr>
// typename QDList<T, HookPtr>::Iterator QDList<T, HookPtr>::rbegin()
//     const noexcept {
//   return QDList<T, HookPtr>::Iterator(*this);
// }

// template <typename T, AtomicDListHook<T> T::*HookPtr>
// typename QDList<T, HookPtr>::Iterator QDList<T, HookPtr>::rbegin(
//     size_t listIdx) const {
//   if (listIdx >= lists_.size()) {
//     throw std::invalid_argument("Invalid list index for MultiDList
//     iterator.");
//   }
//   return QDList<T, HookPtr>::Iterator(*this, listIdx);
// }

// template <typename T, AtomicDListHook<T> T::*HookPtr>
// typename QDList<T, HookPtr>::Iterator QDList<T, HookPtr>::rend()
//     const noexcept {
//   auto it = QDList<T, HookPtr>::Iterator(*this);
//   it.reset();
//   return it;
// }

} // namespace cachelib
} // namespace facebook
