// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__ASYNC__MUTEX_H
#define WARSTAGE__ASYNC__MUTEX_H

#include "./promise.h"
#include <list>

class Mutex;

class [[nodiscard]] MutexLock {
  friend class Mutex;
  std::shared_ptr<Mutex> holder_;
  explicit MutexLock(std::shared_ptr<Mutex> mutex) : holder_{std::move(mutex)} {}
public:
  MutexLock() = default;
  void unlock() { holder_ = nullptr; }
};

class Mutex {
  std::mutex mutex_;
  Promise<MutexLock> last_;
  std::list<Promise<void>> locks_ = { Promise<void>{}.resolve() };
public:
  ~Mutex() {
      LOG_ASSERT(locks_.size() == 1);
      LOG_ASSERT(locks_.front().isFulfilled());
  }

  [[nodiscard]] Promise<MutexLock> lock() {
      std::lock_guard lock(mutex_);
      auto result = locks_.back();
      locks_.emplace_back();
      return result.onResolve<Promise<MutexLock>>([this]() {
        return Promise<MutexLock>{}.resolve(MutexLock{makeHolder()});
      });
  }

  template <typename T>
  [[nodiscard]] Promise<T> lock(std::function<Promise<T>()> action) {
      MutexLock mutexLock = co_await lock();
      Promise<T> promise = action();
      T result = co_await promise;
      co_return result;
  }

  template <>
  [[nodiscard]] Promise<void> lock(std::function<Promise<void>()> action) {
      MutexLock mutexLock = co_await lock();
      Promise<void> promise = action();
      co_await promise;
  }

private:
  [[nodiscard]] std::shared_ptr<Mutex> makeHolder() {
      return std::unique_ptr<Mutex, std::function<void(Mutex*)>>{this, [this](Mutex*) {
        std::lock_guard lock(mutex_);
        locks_.erase(locks_.begin());
        locks_.front().resolve().done();
      }};
  };
};

#endif
