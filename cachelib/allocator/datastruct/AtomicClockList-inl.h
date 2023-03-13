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

#include "AtomicClockList.h"

namespace facebook {
namespace cachelib {

/* Linked list implemenation */
// template <typename T, AtomicClockListHook<T> T::*HookPtr>
// void AtomicClockList<T, HookPtr>::linkAtHead(T& node) noexcept {
//   setPrev(node, nullptr);
//   LockHolder l(*mtx_);

//   T* head = head_;
//   setNext(node, head);

//   if (head_ != nullptr) {
//     setPrev(*head_, &node);
//   }
//   head_ = &node;

//   if (tail_ == nullptr) {
//     tail_ = &node;
//   }
//   if (curr_ == nullptr) {
//     curr_ = &node;
//   }

//   size_++;
// }

/* Linked list implemenation */
template <typename T, AtomicClockListHook<T> T::*HookPtr>
void AtomicClockList<T, HookPtr>::linkAtHead(T& node) noexcept {
  setPrev(node, nullptr);
  // LockHolder l(*mtx_);

  T* head = head_.load();
  setNext(node, head);

  while (!head_.compare_exchange_weak(head, &node)) {
    setNext(node, head);
  }

  if (head != nullptr) {
    setPrev(*head, &node);
  } else {
    XDCHECK_EQ(tail_, nullptr);
    XDCHECK_EQ(curr_, nullptr);

    T *tail = nullptr, *curr = nullptr;
    tail_.compare_exchange_strong(tail, &node);
    curr_.compare_exchange_strong(curr, &node);
  }

  size_++;
}

// template <typename T, AtomicClockListHook<T> T::*HookPtr>
// void AtomicClockList<T, HookPtr>::linkAtTail(T& node) noexcept {
//   XDCHECK_NE(reinterpret_cast<uintptr_t>(&node),
//              reinterpret_cast<uintptr_t>(tail_));

//   setNext(node, nullptr);
//   setPrev(node, tail_);
//   // Fix the next ptr for tail
//   if (tail_ != nullptr) {
//     setNext(*tail_, &node);
//   }
//   tail_ = &node;
//   if (head_ == nullptr) {
//     head_ = &node;
//   }
//   size_++;
// }

// template <typename T, AtomicClockListHook<T> T::*HookPtr>
// void AtomicClockList<T, HookPtr>::insertBefore(T& nextNode, T& node) noexcept
// {
//   XDCHECK_NE(reinterpret_cast<uintptr_t>(&nextNode),
//              reinterpret_cast<uintptr_t>(&node));
//   XDCHECK(getNext(node) == nullptr);
//   XDCHECK(getPrev(node) == nullptr);

//   LockHolder l(*mtx_);
//   auto* const prev = getPrev(nextNode);

//   XDCHECK_NE(reinterpret_cast<uintptr_t>(prev),
//              reinterpret_cast<uintptr_t>(&node));

//   setPrev(node, prev);
//   if (prev != nullptr) {
//     setNext(*prev, &node);
//   } else {
//     head_ = &node;
//   }

//   setPrev(nextNode, &node);
//   setNext(node, &nextNode);
//   size_++;
// }

template <typename T, AtomicClockListHook<T> T::*HookPtr>
void AtomicClockList<T, HookPtr>::unlink(const T& node) noexcept {
  XDCHECK_GT(size_, 0u);
  // fix head_ and tail_ if the node is either of that.
  auto* const prev = getPrev(node);
  auto* const next = getNext(node);

  if (&node == head_) {
    head_ = next;
  }
  if (&node == tail_) {
    tail_ = prev;
  }
  if (&node == curr_) {
    curr_ = prev;
  }

  // fix the next and prev ptrs of the node before and after us.
  if (prev != nullptr) {
    setNextFrom(*prev, node);
  }
  if (next != nullptr) {
    setPrevFrom(*next, node);
  }
  size_--;
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
void AtomicClockList<T, HookPtr>::remove(T& node) noexcept {
  auto* const prev = getPrev(node);
  auto* const next = getNext(node);
  if (prev == nullptr && next == nullptr) {
    return;
  }

  LockHolder l(*mtx_);
  unlink(node);
  setNext(node, nullptr);
  setPrev(node, nullptr);
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
void AtomicClockList<T, HookPtr>::replace(T& oldNode, T& newNode) noexcept {
  LockHolder l(*mtx_);

  // Update head and tail links if needed
  if (&oldNode == head_) {
    head_ = &newNode;
  }
  if (&oldNode == tail_) {
    tail_ = &newNode;
  }
  if (&oldNode == curr_) {
    curr_ = &newNode;
  }

  // Make the previous and next nodes point to the new node
  auto* const prev = getPrev(oldNode);
  auto* const next = getNext(oldNode);
  if (prev != nullptr) {
    setNext(*prev, &newNode);
  }
  if (next != nullptr) {
    setPrev(*next, &newNode);
  }

  // Make the new node point to the previous and next nodes
  setPrev(newNode, prev);
  setNext(newNode, next);

  // Cleanup the old node
  setPrev(oldNode, nullptr);
  setNext(oldNode, nullptr);
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
void AtomicClockList<T, HookPtr>::moveToHead(T& node) noexcept {
  if (&node == head_) {
    return;
  }
  unlink(node);
  linkAtHead(node);
}

#ifdef USE_MPMC_QUEUE
template <typename T, AtomicClockListHook<T> T::*HookPtr>
T* AtomicClockList<T, HookPtr>::getEvictionCandidate() noexcept {
  if (evictCandidateQueue_.sizeGuess() < nMaxEvictionCandidates_ / 4) {
    prepareEvictionCandidates();
  }

  T *ret = nullptr;
  int nTries = 0;
  while (!evictCandidateQueue_.read(ret)) {
    if (nTries++ > 10) {
      abort();
    }
    prepareEvictionCandidates();
  }

  return ret;
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
void AtomicClockList<T, HookPtr>::prepareEvictionCandidates() noexcept {
  LockHolder l(*mtx_);
  if (evictCandidateQueue_.sizeGuess() > nMaxEvictionCandidates_ / 4 * 3) {
    return;
  }
  int nCandidate = nCandidateToPrepare();

  int n_iters = 0;

  T* curr = curr_.load();
  T* next;
  T* firstRetainedNode = curr;
  while (nCandidate > 0) {
    if (curr == head_.load()) {
      curr = tail_.load();
      if (n_iters++ > 2) {
        printf("n_iters = %d\n", n_iters);
        abort();
      }
    }
    if (isAccessed(*curr)) {
      unmarkAccessed(*curr);
#ifdef USE_MYCLOCK_ATOMIC
      curr = getPrev(*curr);
#else
      next = getPrev(*curr);
      // move the node to the head of the list
      moveToHead(*curr);
      curr = next;
#endif
    } else {
      nCandidate--;
      evictCandidateQueue_.write(curr);
      next = getPrev(*curr);
      if (next == nullptr) {
        printf("error2 %p %p\n", getPrev(*curr), getNext(*curr));
        abort();
      }

      unlink(*curr);
      setNext(*curr, nullptr);
      setPrev(*curr, nullptr);
      curr = next;
    }
  }
  // printf("curr = %p, %d %d\n", curr, idx, nEvictionCandidates_.load());
  curr_.store(curr);
}
#else
template <typename T, AtomicClockListHook<T> T::*HookPtr>
T* AtomicClockList<T, HookPtr>::getEvictionCandidate() noexcept {
  // TODO it may happen that the prepare happen while some other threads are
  // still using the old eviction candidates. This is not a correctness issue
  LockHolder l(*mtx_);
  size_t idx = bufIdx_.fetch_add(1);
  T* ret = evictCandidateBuf_[idx];
  while (idx >= nEvictionCandidates_.load()) {
    prepareEvictionCandidates();
    idx = 0;
    ret = evictCandidateBuf_[idx];
    bufIdx_.store(1);
  }
  return ret;
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
void AtomicClockList<T, HookPtr>::prepareEvictionCandidates() noexcept {
  // LockHolder l(*mtx_);
  if (bufIdx_.load() < nEvictionCandidates_.load()) {
    return;
  }

  size_t idx = 0;
  int n_iters = 0;
  nEvictionCandidates_ = size_.load() < nMaxEvictionCandidates_
                             ? size_.load() / 4
                             : nMaxEvictionCandidates_;
  if (nEvictionCandidates_ == 0) {
    nEvictionCandidates_ = 1;
  }

  T* curr = curr_.load();
  T* next;
  // the nodes between first and last retaiend ndoes (curr)
  // are moved to eviction candidate buffer
  T* firstRetainedNode = curr;
  while (idx < nEvictionCandidates_) {
    if (curr == head_.load()) {
      curr = tail_.load();
      if (n_iters++ > 2) {
        printf("n_iters = %d\n", n_iters);
        abort();
      }
    }
    if (isAccessed(*curr)) {
      unmarkAccessed(*curr);
#ifdef USE_MYCLOCK_ATOMIC
      curr = getPrev(*curr);
#else
      next = getPrev(*curr);
      // move the node to the head of the list
      moveToHead(*curr);
      curr = next;
#endif
    } else {
      evictCandidateBuf_[idx++] = curr;
      next = getPrev(*curr);
      if (next == nullptr) {
        printf("error2 %p %p\n", getPrev(*curr), getNext(*curr));
        abort();
      }

      unlink(*curr);
      setNext(*curr, nullptr);
      setPrev(*curr, nullptr);
      curr = next;
    }
  }
  curr_.store(curr);
}
#endif

/* Iterator Implementation */
template <typename T, AtomicClockListHook<T> T::*HookPtr>
void AtomicClockList<T, HookPtr>::Iterator::goForward() noexcept {
  if (dir_ == Direction::FROM_TAIL) {
    curr_ = AtomicClockList_->getPrev(*curr_);
  } else {
    curr_ = AtomicClockList_->getNext(*curr_);
  }
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
void AtomicClockList<T, HookPtr>::Iterator::goBackward() noexcept {
  if (dir_ == Direction::FROM_TAIL) {
    curr_ = AtomicClockList_->getNext(*curr_);
  } else {
    curr_ = AtomicClockList_->getPrev(*curr_);
  }
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
typename AtomicClockList<T, HookPtr>::Iterator&
AtomicClockList<T, HookPtr>::Iterator::operator++() noexcept {
  XDCHECK(curr_ != nullptr);
  if (curr_ != nullptr) {
    goForward();
  }
  return *this;
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
typename AtomicClockList<T, HookPtr>::Iterator&
AtomicClockList<T, HookPtr>::Iterator::operator--() noexcept {
  XDCHECK(curr_ != nullptr);
  if (curr_ != nullptr) {
    goBackward();
  }
  return *this;
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
typename AtomicClockList<T, HookPtr>::Iterator
AtomicClockList<T, HookPtr>::begin() const noexcept {
  return AtomicClockList<T, HookPtr>::Iterator(
      head_, Iterator::Direction::FROM_HEAD, *this);
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
typename AtomicClockList<T, HookPtr>::Iterator
AtomicClockList<T, HookPtr>::rbegin() const noexcept {
  return AtomicClockList<T, HookPtr>::Iterator(
      tail_, Iterator::Direction::FROM_TAIL, *this);
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
typename AtomicClockList<T, HookPtr>::Iterator
AtomicClockList<T, HookPtr>::end() const noexcept {
  return AtomicClockList<T, HookPtr>::Iterator(
      nullptr, Iterator::Direction::FROM_HEAD, *this);
}

template <typename T, AtomicClockListHook<T> T::*HookPtr>
typename AtomicClockList<T, HookPtr>::Iterator
AtomicClockList<T, HookPtr>::rend() const noexcept {
  return AtomicClockList<T, HookPtr>::Iterator(
      nullptr, Iterator::Direction::FROM_TAIL, *this);
}

}  // namespace cachelib
}  // namespace facebook
