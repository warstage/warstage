// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__ASYNC__PROMISE_H
#define WARSTAGE__ASYNC__PROMISE_H

#include "./strand.h"
#include "utilities/logging.h"
#include "value/value.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <experimental/coroutine>


enum class PromiseState { Pending, Fulfilled, Rejected };

template <typename T>
class [[nodiscard]] Promise;

struct PromiseUtils {
  static std::shared_ptr<Strand_base> strand_;
  static std::shared_ptr<Strand_base> Strand();

  [[nodiscard]] static Promise<void> all(const std::vector<Promise<void>>& promises);

  static void ExecuteImmediate(Strand_base* strand, const std::function<void()>& callback) {
      if (strand) {
          strand->setImmediate(callback);
      } else {
          callback();
      }
  }
};

template <typename T>
Promise<T> resolve(const T& value);

template <typename T>
Promise<T> resolve(const Promise<T>& promise) {
    return promise;
}

template <typename T>
Promise<T> reject(const T& value);

template <typename T, typename R>
struct PromiseThen {
  using ResultPromiseType = decltype(resolve(std::declval<R>()));
  static ResultPromiseType then(
      const Promise<T>& promise,
      const std::shared_ptr<Strand_base>& strand,
      const std::function<R(const T&)>& fulfill,
      const std::function<R(const std::exception_ptr&)>& reject);
};

template <typename T>
struct PromiseThen<T, void> {
  using ResultPromiseType = Promise<void>;
  static ResultPromiseType then(
      const Promise<T>& promise,
      const std::shared_ptr<Strand_base>& strand,
      const std::function<void(const T&)>& fulfill,
      const std::function<void(const std::exception_ptr&)>& reject);
};

template <typename R>
struct PromiseThen<void, R> {
  using ResultPromiseType = decltype(resolve(std::declval<R>()));
  static ResultPromiseType then(
      const Promise<void>& promise,
      const std::shared_ptr<Strand_base>& strand,
      const std::function<R()>& fulfill,
      const std::function<R(const std::exception_ptr&)>& reject);
};

template <>
struct PromiseThen<void, void> {
  using ResultPromiseType = Promise<void>;
  static ResultPromiseType then(
      const Promise<void>& promise,
      const std::shared_ptr<Strand_base>& strand,
      const std::function<void()>& fulfill,
      const std::function<void(const std::exception_ptr&)>& reject);
};

template <typename T = void>
class [[nodiscard]] Promise {
public:
  template <typename X> friend class Promise;
  using ValueType = T;
  struct Callback {
    std::shared_ptr<Strand_base> strand;
    std::function<void(ValueType)> resolve;
    std::function<void(const std::exception_ptr&)> reject;
  };

  struct Scope {
    std::mutex mutex{};
    PromiseState state{};
    ValueType value{};
    std::exception_ptr reason{};
    std::vector<Callback> callbacks{};
  };

  std::shared_ptr<Scope> scope_;

  explicit Promise(std::shared_ptr<Scope> scope)
      : scope_{std::move(scope)} {}

public:
  Promise() : scope_{std::make_shared<Scope>()} {}

  [[nodiscard]] bool isFulfilled() const noexcept {
      return scope_ && scope_->state == PromiseState::Fulfilled;
  }

  [[nodiscard]] Promise resolve(const ValueType& value) const;
  [[nodiscard]] Promise resolve(Promise promise) const;
  [[nodiscard]] Promise reject(const std::exception_ptr& reason) const;

  template <typename R>
  [[nodiscard]] Promise reject(const R& reason) const {
      return reject(std::make_exception_ptr(reason));
  }

  template <typename R>
  [[nodiscard]] auto then(
      const std::shared_ptr<Strand_base>& strand,
      const std::function<R(const ValueType&)>& fulfill,
      const std::function<R(const std::exception_ptr&)>& reject = nullptr) const {
      return PromiseThen<T, R>::then(*this, strand, fulfill, reject);
  }

  template <typename R>
  [[nodiscard]] auto then(
      const std::function<R(const ValueType&)>& fulfill,
      const std::function<R(const std::exception_ptr&)>& reject = nullptr) const {
      return PromiseThen<T, R>::then(*this, Strand_base::getCurrent(), fulfill, reject);
  }

  template <typename R>
  [[nodiscard]] auto onReject(
      const std::shared_ptr<Strand_base>& strand,
      const std::function<R(const std::exception_ptr&)>& reject) const {
      return PromiseThen<T, R>::then(*this, strand, nullptr, reject);
  }

  template <typename R>
  [[nodiscard]] auto onReject(
      const std::function<R(const std::exception_ptr&)>& reject) const {
      return PromiseThen<T, R>::then(*this, Strand_base::getCurrent(), nullptr, reject);
  }

  template <typename R>
  [[nodiscard]] auto onResolve(
      const std::shared_ptr<Strand_base>& strand,
      const std::function<R()>& resolve) const {
      return PromiseThen<T, R>::then(*this,
          strand,
          [resolve](const ValueType& value) { return resolve(); },
          [resolve](const std::exception_ptr& reason) { LOG_REJECTION(reason); return resolve(); });
  }

  template <typename R>
  [[nodiscard]] auto onResolve(
      const std::function<R()>& resolve) const {
      return PromiseThen<T, R>::then(*this,
          Strand_base::getCurrent(),
          [resolve](const ValueType& value) { return resolve(); },
          [resolve](const std::exception_ptr& reason) { LOG_REJECTION(reason); return resolve(); });
  }

  void done() noexcept {}

  /* co_return */

  using promise_type = Promise;

  auto initial_suspend() noexcept {
      return std::experimental::suspend_never{};
  }

  auto final_suspend() noexcept {
      return std::experimental::suspend_always{};
  }

  auto get_return_object() {
      return *this;
  }

  auto return_value(ValueType value) {
      resolve(value).done();
      return std::experimental::suspend_never{};
  }

  void unhandled_exception() {
      reject(std::current_exception()).done();
  }

  /* co_await */

  [[nodiscard]] bool await_ready() const noexcept {
      return scope_->state != PromiseState::Pending;
  }

  template <typename P>
  void await_suspend(std::experimental::coroutine_handle<P> handle) noexcept {
      onResolve<void>([address = handle.address()]() {
        std::experimental::coroutine_handle<P>::from_address(address).resume();
      }).done();
  }

  [[nodiscard]] ValueType await_resume() {
      if (scope_->reason) {
          std::rethrow_exception(scope_->reason);
      }
      return scope_->value;
  }

};


template <>
class [[nodiscard]] Promise<void> {
public:
  template <typename X> friend class Promise;
  struct Callback {
    std::shared_ptr<Strand_base> strand;
    std::function<void()> resolve;
    std::function<void(const std::exception_ptr&)> reject;
  };

  struct Scope {
    std::mutex mutex{};
    PromiseState state{};
    std::exception_ptr reason{};
    std::vector<Callback> callbacks{};
  };

  std::shared_ptr<Scope> scope_;

  Promise(std::shared_ptr<Scope> scope)
      : scope_{std::move(scope)} {}

public:
  Promise() : scope_{std::make_shared<Scope>()} {}

  [[nodiscard]] bool isFulfilled() const noexcept {
      return scope_ && scope_->state == PromiseState::Fulfilled;
  }

  [[nodiscard]] Promise resolve() const;
  [[nodiscard]] Promise resolve(Promise promise) const;
  [[nodiscard]] Promise reject(const std::exception_ptr& reason) const;

  template <typename R>
  [[nodiscard]] Promise reject(const R& reason) const {
      return reject(std::make_exception_ptr(reason));
  }

  template <typename R>
  [[nodiscard]] auto then(
      const std::shared_ptr<Strand_base>& strand,
      const std::function<R()>& fulfill,
      const std::function<R(const std::exception_ptr&)>& reject = nullptr) const {
      return PromiseThen<void, R>::then(*this, strand, fulfill, reject);
  }

  template <typename R>
  [[nodiscard]] auto then(
      const std::function<R()>& fulfill,
      const std::function<R(const std::exception_ptr&)>& reject = nullptr) const {
      return PromiseThen<void, R>::then(*this, Strand_base::getCurrent(), fulfill, reject);
  }

  template <typename R>
  [[nodiscard]] auto onReject(
      const std::shared_ptr<Strand_base>& strand,
      const std::function<R(const std::exception_ptr&)>& reject) const {
      return PromiseThen<void, R>::then(*this, strand, nullptr, reject);
  }

  template <typename R>
  [[nodiscard]] auto onReject(
      const std::function<R(const std::exception_ptr&)>& reject) const {
      return PromiseThen<void, R>::then(*this, Strand_base::getCurrent(), nullptr, reject);
  }

  template <typename R>
  [[nodiscard]] auto onResolve(
      const std::shared_ptr<Strand_base>& strand,
      const std::function<R()>& resolve) const {
      return PromiseThen<void, R>::then(*this,
          strand,
          [resolve]() { return resolve(); },
          [resolve](const std::exception_ptr& reason) { LOG_REJECTION(reason); return resolve(); });
  }

  template <typename R>
  [[nodiscard]] auto onResolve(
      const std::function<R()>& resolve) const {
      return PromiseThen<void, R>::then(*this,
          Strand_base::getCurrent(),
          [resolve]() { return resolve(); },
          [resolve](const std::exception_ptr& reason) { LOG_REJECTION(reason); return resolve(); });
  }

  void done() noexcept {}

  /* co_return */

  using promise_type = Promise;

  auto initial_suspend() noexcept {
      return std::experimental::suspend_never{};
  }

  auto final_suspend() noexcept {
      return std::experimental::suspend_always{};
  }

  auto get_return_object() {
      return *this;
  }

  auto return_void() {
      resolve().done();
      return std::experimental::suspend_never{};
  }

  void unhandled_exception() {
      reject(std::current_exception()).done();
  }

  /* co_await */

  [[nodiscard]] bool await_ready() const noexcept {
      return scope_->state != PromiseState::Pending;
  }

  template <typename P>
  void await_suspend(std::experimental::coroutine_handle<P> handle) noexcept {
      onResolve<void>([address = handle.address()]() {
        std::experimental::coroutine_handle<P>::from_address(address).resume();
      }).done();
  }

  void await_resume() {
      if (scope_->reason) {
          std::rethrow_exception(scope_->reason);
      }
  }

};


/* resolve */

template <typename T>
Promise<T> resolve(const T& value) {
    return Promise<T>{}.resolve(value);
}

template <typename T>
inline Promise<T> Promise<T>::resolve(const ValueType& value) const {
    std::lock_guard lock{scope_->mutex};
    if (scope_->state == PromiseState::Pending) {
        scope_->state = PromiseState::Fulfilled;
        scope_->value = value;
        for (auto& callback : scope_->callbacks) {
            if (callback.resolve) {
                PromiseUtils::ExecuteImmediate(callback.strand.get(), [callback = callback.resolve, scope = scope_]() {
                  callback(scope->value);
                });
            }
        }
        scope_->callbacks.clear();
    }
    return Promise{scope_};
}


template <typename T>
inline Promise<T> Promise<T>::resolve(Promise promise) const {
    std::lock_guard lock{promise.scope_->mutex};
    switch (promise.scope_->state) {
        case PromiseState::Pending: {
            promise.scope_->callbacks.push_back({
                nullptr,
                [deferred = *this](const ValueType& result) {
                  deferred.resolve(result).done();
                },
                [deferred = *this](const std::exception_ptr& reason) {
                  deferred.reject(reason).done();
                }
            });
            return Promise{scope_};
        }
        case PromiseState::Fulfilled: {
            return resolve(promise.scope_->value);
        }
        case PromiseState::Rejected: {
            return reject(promise.scope_->reason);
        }
    }
}


inline Promise<void> Promise<void>::resolve() const {
    std::lock_guard lock{scope_->mutex};
    if (scope_->state == PromiseState::Pending) {
        scope_->state = PromiseState::Fulfilled;
        for (auto& callback : scope_->callbacks) {
            if (callback.resolve) {
                PromiseUtils::ExecuteImmediate(callback.strand.get(), [callback = callback.resolve, scope = scope_]() {
                  callback();
                });
            }
        }
        scope_->callbacks.clear();
    }
    return Promise{scope_};
}


inline Promise<void> Promise<void>::resolve(Promise promise) const {
    std::lock_guard lock{promise.scope_->mutex};
    switch (promise.scope_->state) {
        case PromiseState::Pending: {
            promise.scope_->callbacks.push_back({
                nullptr,
                [deferred = *this]() {
                  deferred.resolve().done();
                },
                [deferred = *this](const std::exception_ptr& reason) {
                  deferred.reject(reason).done();
                }
            });
            return Promise{scope_};
        }
        case PromiseState::Fulfilled: {
            return resolve();
        }
        case PromiseState::Rejected: {
            return reject(promise.scope_->reason);
        }
    }
}


/* reject */

template <typename T>
Promise<T> reject(const T& value) {
    return Promise<T>{}.reject(value);
}

template <typename T>
inline Promise<T> Promise<T>::reject(const std::exception_ptr& reason) const {
    std::lock_guard lock{scope_->mutex};
    if (scope_->state == PromiseState::Pending) {
        scope_->state = PromiseState::Rejected;
        scope_->reason = reason;
        for (auto& callback : scope_->callbacks) {
            if (callback.reject) {
                PromiseUtils::ExecuteImmediate(callback.strand.get(), [reject = callback.reject, scope = scope_]() {
                  reject(scope->reason);
                });
            }
        }
        scope_->callbacks.clear();
    }
    return Promise{scope_};
}


inline Promise<void> Promise<void>::reject(const std::exception_ptr& reason) const {
    std::lock_guard lock{scope_->mutex};
    if (scope_->state == PromiseState::Pending) {
        scope_->state = PromiseState::Rejected;
        scope_->reason = reason;
        for (auto& callback : scope_->callbacks) {
            if (callback.reject) {
                PromiseUtils::ExecuteImmediate(callback.strand.get(), [reject = callback.reject, scope = scope_]() {
                  reject(scope->reason);
                });
            }
        }
        scope_->callbacks.clear();
    }
    return Promise{scope_};
}


/* then */

template <typename T, typename R>
inline typename PromiseThen<T, R>::ResultPromiseType PromiseThen<T, R>::then(
    const Promise<T>& promise,
    const std::shared_ptr<Strand_base>& strand,
    const std::function<R(const T&)>& fulfill,
    const std::function<R(const std::exception_ptr&)>& reject) {
    std::lock_guard lock{promise.scope_->mutex};
    switch (promise.scope_->state) {
        case PromiseState::Pending: {
            auto scope = std::make_shared<typename ResultPromiseType::Scope>();
            promise.scope_->callbacks.push_back({
                strand,
                [scope, fulfill](const T& result) {
                  try {
                      if (fulfill) {
                          ResultPromiseType{scope}.resolve(fulfill(result)).done();
                      } else {
                          ResultPromiseType{scope}.resolve(result).done();
                      }
                  } catch (...) {
                      ResultPromiseType{scope}.reject(std::current_exception()).done();
                  }
                },
                [scope, reject](const std::exception_ptr& reason) {
                  try {
                      if (reject) {
                          ResultPromiseType{scope}.resolve(reject(reason)).done();
                      } else {
                          ResultPromiseType{scope}.reject(reason).done();
                      }
                  } catch (...) {
                      ResultPromiseType{scope}.reject(std::current_exception()).done();
                  }
                }
            });
            return ResultPromiseType{scope};
        }
        case PromiseState::Fulfilled: {
            ResultPromiseType deferred{};
            PromiseUtils::ExecuteImmediate(strand.get(), [deferred, fulfill, value = promise.scope_->value]() {
              try {
                  deferred.resolve(fulfill(value)).done();
              } catch (...) {
                  deferred.reject(std::current_exception()).done();
              }
            });
            return deferred;
        }
        case PromiseState::Rejected: {
            ResultPromiseType deferred{};
            PromiseUtils::ExecuteImmediate(strand.get(), [deferred, reject, reason = promise.scope_->reason]() {
              try {
                  if (reject) {
                      deferred.resolve(reject(reason)).done();
                  } else {
                      deferred.reject(reason).done();
                  }
              } catch (...) {
                  deferred.reject(std::current_exception()).done();
              }
            });
            return deferred;
        }
    }
}

template <typename T>
inline typename PromiseThen<T, void>::ResultPromiseType PromiseThen<T, void>::then(
    const Promise<T>& promise,
    const std::shared_ptr<Strand_base>& strand,
    const std::function<void(const T&)>& fulfill,
    const std::function<void(const std::exception_ptr&)>& reject) {
    std::lock_guard lock{promise.scope_->mutex};
    switch (promise.scope_->state) {
        case PromiseState::Pending: {
            auto scope = std::make_shared<typename ResultPromiseType::Scope>();
            promise.scope_->callbacks.push_back({
                strand,
                [scope, fulfill](const T& result) {
                  try {
                      if (fulfill) {
                          fulfill(result);
                      }
                      ResultPromiseType{scope}.resolve().done();
                  } catch (...) {
                      ResultPromiseType{scope}.reject(std::current_exception()).done();
                  }
                },
                [scope, reject](const std::exception_ptr& reason) {
                  try {
                      if (reject) {
                          reject(reason);
                          ResultPromiseType{scope}.resolve().done();
                      } else {
                          ResultPromiseType{scope}.reject(reason).done();
                      }
                  } catch (...) {
                      ResultPromiseType{scope}.reject(std::current_exception()).done();
                  }
                }
            });
            return ResultPromiseType{scope};
        }
        case PromiseState::Fulfilled: {
            ResultPromiseType deferred{};
            PromiseUtils::ExecuteImmediate(strand.get(), [deferred, fulfill, value = promise.scope_->value]() {
              try {
                  fulfill(value);
                  deferred.resolve().done();
              } catch (...) {
                  deferred.reject(std::current_exception()).done();
              }
            });
            return deferred;
        }
        case PromiseState::Rejected: {
            ResultPromiseType deferred{};
            PromiseUtils::ExecuteImmediate(strand.get(), [deferred, reject, reason = promise.scope_->reason]() {
              try {
                  if (reject) {
                      reject(reason);
                      deferred.resolve().done();
                  } else {
                      deferred.reject(reason).done();
                  }
              } catch (...) {
                  deferred.reject(std::current_exception()).done();
              }
            });
            return deferred;
        }
    }
}

template <typename R>
inline typename PromiseThen<void, R>::ResultPromiseType PromiseThen<void, R>::then(
    const Promise<void>& promise,
    const std::shared_ptr<Strand_base>& strand,
    const std::function<R()>& fulfill,
    const std::function<R(const std::exception_ptr&)>& reject) {
    std::lock_guard lock{promise.scope_->mutex};
    switch (promise.scope_->state) {
        case PromiseState::Pending: {
            auto scope = std::make_shared<typename ResultPromiseType::Scope>();
            promise.scope_->callbacks.push_back({
                strand,
                [scope, fulfill]() {
                  try {
                      if (fulfill) {
                          ResultPromiseType{scope}.resolve(fulfill()).done();
                      } else {
                          ResultPromiseType{scope}.resolve(R{}).done();
                      }
                  } catch (...) {
                      ResultPromiseType{scope}.reject(std::current_exception()).done();
                  }
                },
                [scope, reject](const std::exception_ptr& reason) {
                  try {
                      if (reject) {
                          ResultPromiseType{scope}.resolve(reject(reason)).done();
                      } else {
                          ResultPromiseType{scope}.reject(reason).done();
                      }
                  } catch (...) {
                      ResultPromiseType{scope}.reject(std::current_exception()).done();
                  }
                }
            });
            return ResultPromiseType{scope};
        }
        case PromiseState::Fulfilled: {
            ResultPromiseType deferred{};
            PromiseUtils::ExecuteImmediate(strand.get(), [deferred, fulfill]() {
              try {
                  if (fulfill) {
                      deferred.resolve(fulfill()).done();
                  } else {
                      deferred.resolve(R{}).done();
                  }
              } catch (...) {
                  deferred.reject(std::current_exception()).done();
              }
            });
            return deferred;
        }
        case PromiseState::Rejected: {
            ResultPromiseType deferred{};
            PromiseUtils::ExecuteImmediate(strand.get(), [deferred, reject, reason = promise.scope_->reason]() {
              try {
                  if (reject) {
                      deferred.resolve(reject(reason)).done();
                  } else {
                      deferred.reject(reason).done();
                  }
              } catch (...) {
                  deferred.reject(std::current_exception()).done();
              }
            });
            return deferred;
        }
    }
}

inline typename PromiseThen<void, void>::ResultPromiseType PromiseThen<void, void>::then(
    const Promise<void>& promise,
    const std::shared_ptr<Strand_base>& strand,
    const std::function<void()>& fulfill,
    const std::function<void(const std::exception_ptr&)>& reject) {
    std::lock_guard lock{promise.scope_->mutex};
    switch (promise.scope_->state) {
        case PromiseState::Pending: {
            auto scope = std::make_shared<typename ResultPromiseType::Scope>();
            promise.scope_->callbacks.push_back({
                strand,
                [scope, fulfill]() {
                  try {
                      if (fulfill) {
                          fulfill();
                      }
                      ResultPromiseType{scope}.resolve().done();
                  } catch (...) {
                      ResultPromiseType{scope}.reject(std::current_exception()).done();
                  }
                },
                [scope, reject](const std::exception_ptr& reason) {
                  try {
                      if (reject) {
                          reject(reason);
                          ResultPromiseType{scope}.resolve().done();
                      } else {
                          ResultPromiseType{scope}.reject(reason).done();
                      }
                  } catch (...) {
                      ResultPromiseType{scope}.reject(std::current_exception()).done();
                  }
                }
            });
            return ResultPromiseType{scope};
        }
        case PromiseState::Fulfilled: {
            ResultPromiseType deferred{};
            PromiseUtils::ExecuteImmediate(strand.get(), [deferred, fulfill]() {
              try {
                  if (fulfill) {
                      fulfill();
                  }
                  deferred.resolve().done();
              } catch (...) {
                  deferred.reject(std::current_exception()).done();
              }
            });
            return deferred;
        }
        case PromiseState::Rejected: {
            ResultPromiseType deferred{};
            PromiseUtils::ExecuteImmediate(strand.get(), [deferred, reject, reason = promise.scope_->reason]() {
              try {
                  if (reject) {
                      reject(reason);
                      deferred.resolve().done();
                  } else {
                      deferred.reject(reason).done();
                  }
              } catch (...) {
                  deferred.reject(std::current_exception()).done();
              }
            });
            return deferred;
        }
    }
}


#endif