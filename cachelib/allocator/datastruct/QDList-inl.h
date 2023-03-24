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

// used for single thread
template <typename T, AtomicDListHook<T> T::*HookPtr>
T* QDList<T, HookPtr>::getEvictionCandidate0() noexcept {
  size_t listSize = pfifo_->size() + mfifo_->size();
  if (listSize == 0) {
    return nullptr;
  }

  T* curr = nullptr;
  if (!hist_.initialized()) {
    LockHolder l(*mtx_);

    hist_.setFIFOSize(listSize / 2);
    hist_.initHashtable();
  }

  while (true) {
    if (pfifo_->size() > (double)(pfifo_->size() + mfifo_->size()) * pRatio_) {
      // evict from probationary FIFO
      curr = pfifo_->removeTail();
      if (curr == nullptr) {
        continue;
      }
      if (pfifo_->isAccessed(*curr)) {
        pfifo_->unmarkAccessed(*curr);
        // XDCHECK(isProbationary(*curr));
        unmarkProbationary(*curr);
        markMain(*curr);
        mfifo_->linkAtHead(*curr);
      } else {
        hist_.insert(hashNode(*curr));
        return curr;
      }
    } else {
      curr = mfifo_->removeTail();
      if (curr == nullptr) {
        continue;
      }
      if (mfifo_->isAccessed(*curr)) {
        mfifo_->unmarkAccessed(*curr);
        mfifo_->linkAtHead(*curr);
      } else {
        return curr;
      }
    }
  }
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
T* QDList<T, HookPtr>::getEvictionCandidate() noexcept {
  size_t listSize = pfifo_->size() + mfifo_->size();
  if (listSize == 0) {
    return nullptr;
  }

  T* curr = nullptr;
  if (!hist_.initialized()) {
    LockHolder l(*mtx_);
#define ENABLE_SCALABILITY
#ifdef ENABLE_SCALABILITY
    if (evThread_.get() == nullptr) {
      evThread_ = std::make_unique<std::thread>(&QDList::threadFunc, this);
    }
#endif

    hist_.setFIFOSize(listSize / 2);
    hist_.initHashtable();
  }

  if (evictCandidateQueue_.sizeGuess() < nMaxEvictionCandidates_ / 4) {
    prepareEvictionCandidates();
  }

  int nTries = 0;
  while (!evictCandidateQueue_.read(curr)) {
    if ((nTries++) % 100 == 0) {
      prepareEvictionCandidates();
    }
  }

  return curr;
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
void QDList<T, HookPtr>::prepareEvictionCandidates() noexcept {
  T* curr = nullptr;
  T* mfifo_head = nullptr;
  T* mfifo_tail = nullptr;
  int nNodeLocal = 0;

  for (int i = 0; i < nCandidateToPrepare(); i++) {
    if (evictCandidateQueue_.sizeGuess() >=
        (ssize_t)nMaxEvictionCandidates_ - 16) {
      return;
    }

    if (pfifo_->size() > (double)(pfifo_->size() + mfifo_->size()) * pRatio_) {
      // evict from probationary FIFO
      curr = pfifo_->removeTail();
      if (curr != nullptr) {
        if (pfifo_->isAccessed(*curr)) {
          pfifo_->unmarkAccessed(*curr);
          // XDCHECK(isProbationary(*curr));
          unmarkProbationary(*curr);
          markMain(*curr);

          // link to local list
          if (mfifo_head == nullptr) {
            mfifo_head = curr;
            mfifo_tail = curr;
          } else {
            mfifo_->setNext(*curr, mfifo_head);
            mfifo_->setPrev(*mfifo_head, curr);
            mfifo_head = curr;
          }
          nNodeLocal += 1;

        } else {
          hist_.insert(hashNode(*curr));
          if (!evictCandidateQueue_.write(curr)) {
            pfifo_->linkAtHead(*curr);
          }
        }
      }
    } else {
      curr = mfifo_->removeTail();
      if (curr != nullptr) {
        if (mfifo_->isAccessed(*curr)) {
          mfifo_->unmarkAccessed(*curr);

          // link to local list
          if (mfifo_head == nullptr) {
            mfifo_head = curr;
            mfifo_tail = curr;
          } else {
            mfifo_->setNext(*curr, mfifo_head);
            mfifo_->setPrev(*mfifo_head, curr);
            mfifo_head = curr;
          }
          nNodeLocal += 1;

        } else {
          if (!evictCandidateQueue_.write(curr)) {
            pfifo_->linkAtHead(*curr);
          }
        }
      }
    }
  }

  if (mfifo_head != nullptr) {
    // link local list to the global list
    mfifo_->linkAtHeadMultiple(*mfifo_head, *mfifo_tail, nNodeLocal);
  }
}

} // namespace cachelib
} // namespace facebook
