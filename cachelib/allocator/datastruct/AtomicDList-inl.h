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

/* Linked list implemenation */
template <typename T, AtomicDListHook<T> T::*HookPtr>
void AtomicDList<T, HookPtr>::linkAtHead(T& node) noexcept {
  setPrev(node, nullptr);

  T* oldHead = head_.load();
  setNext(node, oldHead);

  while (!head_.compare_exchange_weak(oldHead, &node)) {
    setNext(node, oldHead);
  }

  if (oldHead == nullptr) {
    // this is the thread that first makes head_ points to the node
    // other threads must follow this, o.w. oldHead will be nullptr
    XDCHECK_EQ(tail_, nullptr);

    T *tail = nullptr, *curr = nullptr;
    tail_.compare_exchange_strong(tail, &node);
    // curr_.compare_exchange_strong(curr, &node);
  } else {
    setPrev(*oldHead, &node);
  }

  size_++;
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
void AtomicDList<T, HookPtr>::linkAtHeadMultiple(T& start, T& end, size_t n) noexcept {
  setPrev(start, nullptr);

  // int c = 1;
  // T *curr = &start;
  // while (curr && curr != &end) {
  //   c += 1;
  //   curr = getNext(*curr);
  // }
  // if (c != n) {
  //   printf("c = %d, n = %ld\n", c, n);
  //   abort();
  // }

  T* oldHead = head_.load();
  setNext(end, oldHead);

  while (!head_.compare_exchange_weak(oldHead, &start)) {
    setNext(end, oldHead);
  }

  if (oldHead == nullptr) {
    // this is the thread that first makes head_ points to the node
    // other threads must follow this, o.w. oldHead will be nullptr
    XDCHECK_EQ(tail_, nullptr);

    T *tail = nullptr;
    tail_.compare_exchange_strong(tail, &end);
  } else {
    setPrev(*oldHead, &end);
  }

  size_ += n;
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
void AtomicDList<T, HookPtr>::linkAtHeadFromADList(AtomicDList<T, HookPtr> &o) noexcept {

  T* oHead = &o.getHead();
  T* oTail = &o.getTail();

  // setPrev(o.getHead(), nullptr);

  T* oldHead = head_.load();
  setNext(*oTail, oldHead);

  while (!head_.compare_exchange_weak(oldHead, oHead)) {
    setNext(oTail, oldHead);
  }

  if (oldHead == nullptr) {
    // this is the thread that first makes head_ points to the node
    // other threads must follow this, o.w. oldHead will be nullptr
    XDCHECK_EQ(tail_, nullptr);

    T *tail = nullptr;
    tail_.compare_exchange_strong(tail, &oTail);
  } else {
    setPrev(*oldHead, &oTail);
  }

  size_ += o.size();
}

/* note that the next of the tail may not be nullptr  */
template <typename T, AtomicDListHook<T> T::*HookPtr>
T* AtomicDList<T, HookPtr>::removeTail() noexcept {
  T* tail = tail_.load();
  if (tail == nullptr) {
    // empty list
    return nullptr;
  }
  T* prev = getPrev(*tail);

  while (!tail_.compare_exchange_weak(tail, prev)) {
    prev = getPrev(*tail);
  }

  // if the tail was also the head
  T* oldHead = tail;
  head_.compare_exchange_weak(oldHead, nullptr);

  // do not do this since this object will be evicted/promoted
  // else {
  //   setNext(*prev, nullptr);
  // }

  setPrev(*tail, nullptr);

  size_--;

  return tail;
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
void AtomicDList<T, HookPtr>::unlink(const T& node) noexcept {
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

  // fix the next and prev ptrs of the node before and after us.
  if (prev != nullptr) {
    setNextFrom(*prev, node);
  }
  if (next != nullptr) {
    setPrevFrom(*next, node);
  }
  size_--;
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
void AtomicDList<T, HookPtr>::remove(T& node) noexcept {
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

template <typename T, AtomicDListHook<T> T::*HookPtr>
void AtomicDList<T, HookPtr>::replace(T& oldNode, T& newNode) noexcept {
  printf("replace\n");
  LockHolder l(*mtx_);

  // Update head and tail links if needed
  if (&oldNode == head_) {
    head_ = &newNode;
  }
  if (&oldNode == tail_) {
    tail_ = &newNode;
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

/* Iterator Implementation */
template <typename T, AtomicDListHook<T> T::*HookPtr>
void AtomicDList<T, HookPtr>::Iterator::goForward() noexcept {
  if (dir_ == Direction::FROM_TAIL) {
    curr_ = AtomicDList_->getPrev(*curr_);
  } else {
    curr_ = AtomicDList_->getNext(*curr_);
  }
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
void AtomicDList<T, HookPtr>::Iterator::goBackward() noexcept {
  if (dir_ == Direction::FROM_TAIL) {
    curr_ = AtomicDList_->getNext(*curr_);
  } else {
    curr_ = AtomicDList_->getPrev(*curr_);
  }
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
typename AtomicDList<T, HookPtr>::Iterator&
AtomicDList<T, HookPtr>::Iterator::operator++() noexcept {
  XDCHECK(curr_ != nullptr);
  if (curr_ != nullptr) {
    goForward();
  }
  return *this;
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
typename AtomicDList<T, HookPtr>::Iterator&
AtomicDList<T, HookPtr>::Iterator::operator--() noexcept {
  XDCHECK(curr_ != nullptr);
  if (curr_ != nullptr) {
    goBackward();
  }
  return *this;
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
typename AtomicDList<T, HookPtr>::Iterator AtomicDList<T, HookPtr>::begin()
    const noexcept {
  return AtomicDList<T, HookPtr>::Iterator(
      head_, Iterator::Direction::FROM_HEAD, *this);
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
typename AtomicDList<T, HookPtr>::Iterator AtomicDList<T, HookPtr>::rbegin()
    const noexcept {
  return AtomicDList<T, HookPtr>::Iterator(
      tail_, Iterator::Direction::FROM_TAIL, *this);
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
typename AtomicDList<T, HookPtr>::Iterator AtomicDList<T, HookPtr>::end()
    const noexcept {
  return AtomicDList<T, HookPtr>::Iterator(
      nullptr, Iterator::Direction::FROM_HEAD, *this);
}

template <typename T, AtomicDListHook<T> T::*HookPtr>
typename AtomicDList<T, HookPtr>::Iterator AtomicDList<T, HookPtr>::rend()
    const noexcept {
  return AtomicDList<T, HookPtr>::Iterator(
      nullptr, Iterator::Direction::FROM_TAIL, *this);
}
} // namespace cachelib
} // namespace facebook
