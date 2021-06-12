// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__UTILITIES__MEMORY_H
#define WARSTAGE__UTILITIES__MEMORY_H

#include <cassert>
#include <memory>

template <typename T> class RootPtr;
template <typename T> class BackPtr;
template <typename T> class WeakPtr;

template <typename T>
class BasePtr_ {
  friend class RootPtr<T>;
  friend class BackPtr<T>;
  friend class WeakPtr<T>;
protected:
  struct Node {
    std::unique_ptr<T> value;
    std::size_t refCount;
    std::size_t backCount;
  };
  Node* node_;
  explicit BasePtr_(Node* node) noexcept : node_{node} {}
public:
  using pointer = T*;
  typename std::add_lvalue_reference<T>::type operator*() const noexcept {
    assert(node_ && node_->value);
    return *node_->value;
  }
  pointer operator->() const noexcept {
    assert(node_ && node_->value);
    return node_->value.get();
  }
  pointer get() const noexcept {
    return node_ ? node_->value.get() : nullptr;
  }
  bool operator==(const BasePtr_<T>& other) const noexcept { return node_ == other.node_; }
  bool operator!=(const BasePtr_<T>& other) const noexcept { return node_ != other.node_; }
  bool operator==(const std::nullptr_t&) const noexcept { return !node_ || !node_->value; }
  bool operator!=(const std::nullptr_t&) const noexcept { return node_ && node_->value; }
};

template <typename T>
class RootPtr : public BasePtr_<T> {
  friend class BackPtr<T>;
  friend class WeakPtr<T>;
  using Node = typename BasePtr_<T>::Node;
  using BasePtr_<T>::node_;
public:
  RootPtr() noexcept : BasePtr_<T>{nullptr} {}
  ~RootPtr() noexcept {
    reset(nullptr);
  }
  RootPtr(std::nullptr_t) noexcept : BasePtr_<T>{nullptr} {}
  explicit RootPtr(T* value) : BasePtr_<T>{ new Node{ std::unique_ptr<T>{ value }, 1, 0 } } {
  }
  RootPtr(RootPtr&& other) noexcept : BasePtr_<T>{other.node_} {
    other.node_ = nullptr;
  }
  RootPtr& operator=(RootPtr&& other) noexcept {
    reset();
    node_ = other.node_;
    other.node_ = nullptr;
    return *this;
  }
  RootPtr(const RootPtr&) = delete;
  RootPtr& operator=(const RootPtr&) = delete;
  void reset(T* value = nullptr) noexcept {
    if (node_) {
      node_->value = nullptr;
      assert(!node_->backCount); // no back pointers after destroying value
      if (--node_->refCount == 0) {
        delete node_;
      }
      node_ = nullptr;
    }
    if (value) {
      node_ = { new Node{std::unique_ptr<T>{value}, 1, 0 } };
    }
  }
  explicit operator bool() const noexcept {
    assert(node_ == nullptr || node_->value != nullptr);
    return node_;
  }
};

template <typename T>
class BackPtr : public BasePtr_<T> {
  friend class RootPtr<T>;
  friend class WeakPtr<T>;
  using Node = typename BasePtr_<T>::Node;
  using BasePtr_<T>::node_;
public:
  using pointer = T*;
  ~BackPtr() noexcept {
    assert(node_);
    --node_->backCount;
  }
  BackPtr(const RootPtr<T>& ptr) noexcept : BasePtr_<T>{ptr.node_} {
    assert(node_);
    ++node_->backCount;
  }
  BackPtr(const BackPtr& other) noexcept : BasePtr_<T>{other.node_} {
    assert(node_);
    ++node_->backCount;
  }
  BackPtr(BackPtr&& other) noexcept : BasePtr_<T>{other.node_} {
    assert(node_);
    ++node_->backCount;
  }
  BackPtr& operator=(const BackPtr& other) noexcept {
    assert(other.node_);
    --node_->backCount;
    node_ = other.node_;
    ++node_->backCount;
    return *this;
  }
  BackPtr& operator=(BackPtr&& other) noexcept {
    assert(other.node_);
    --node_->backCount;
    node_ = other.node_;
    ++node_->backCount;
    return *this;
  }
};

template <typename T>
class WeakPtr : public BasePtr_<T> {
  friend class RootPtr<T>;
  friend class BackPtr<T>;
  using Node = typename BasePtr_<T>::Node;
  using BasePtr_<T>::node_;
public:
  using pointer = T*;
  WeakPtr() noexcept : BasePtr_<T>{nullptr} {}
  ~WeakPtr() noexcept {
    reset(nullptr);
  }
  WeakPtr(std::nullptr_t) noexcept : BasePtr_<T>{nullptr} {}
  WeakPtr(const WeakPtr& other) noexcept : BasePtr_<T>{other.node_} {
    if (node_) {
      ++node_->refCount;
    }
  }
  WeakPtr(WeakPtr&& other) noexcept : BasePtr_<T>{other.node_} {
    if (node_) {
      ++node_->refCount;
    }
  }
  WeakPtr(const BasePtr_<T>& ptr) noexcept : BasePtr_<T>{ptr.node_} {
    if (node_) {
      ++node_->refCount;
    }
  }
  WeakPtr& operator=(const WeakPtr& other) noexcept {
    reset(other.node_);
    return *this;
  }
  WeakPtr& operator=(WeakPtr&& other) noexcept {
    reset(other.node_);
    return *this;
  }
  WeakPtr& operator=(const BasePtr_<T>& other) noexcept {
    reset(other.node_);
    return *this;
  }
  WeakPtr& operator=(std::nullptr_t) noexcept {
    reset(nullptr);
    return *this;
  }
  explicit operator bool() const noexcept {
    return node_ && node_->value;
  }
private:
  void reset(Node* node) noexcept {
    if (node_ && --node_->refCount == 0) {
      assert(!node_->value);
      assert(!node_->backCount);
      delete node_;
    }
    node_ = node;
    if (node_) {
      ++node_->refCount;
    }
  }
};

#endif
