// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./strand-manual.h"
#include "utilities/logging.h"
#include <unordered_set>


std::shared_ptr<ImmediateObject> Strand_Manual::setImmediate(std::function<void()> callback) {
  auto result = std::make_shared<ImmediateObject_Manual>();
  result->strand_ = weak_from_this();
  result->callback_ = std::move(callback);
  std::lock_guard lock{mutex_};
  immediates_.push_back(result);
  return result;
}


std::shared_ptr<IntervalObject> Strand_Manual::setInterval(std::function<void()> callback, double delay) {
  auto result = std::make_shared<IntervalObject_Manual>();
  result->strand_ = weak_from_this();
  result->callback_ = std::move(callback);
  result->delay_ = std::chrono::milliseconds{static_cast<long>(delay)};
  result->timeout_ = std::chrono::steady_clock::now() + result->delay_;
  std::lock_guard lock{mutex_};
  intervals_.push_back(result);
  return result;
}


std::shared_ptr<TimeoutObject> Strand_Manual::setTimeout(std::function<void()> callback, double delay) {
  auto result = std::make_shared<TimeoutObject_Manual>();
  result->strand_ = weak_from_this();
  result->callback_ = std::move(callback);
  result->timeout_ = std::chrono::steady_clock::now() + std::chrono::milliseconds{static_cast<long>(delay)};
  std::lock_guard lock{mutex_};
  timeouts_.push_back(result);
  return result;
}


void Strand_Manual::execute(std::function<void()> callback) {
  LOG_ASSERT(!isCurrent());
  SetCurrent current{shared_from_this()};
  callback();
}


void Strand_Manual::run() {
  LOG_ASSERT(!isCurrent());
  SetCurrent current{ClearCurrent{}};
  runImmediate();
  runTimeout();
  runInterval();
  runImmediate();
}


bool Strand_Manual::isDone() {
  std::lock_guard lock{mutex_};
  return immediates_.empty(); // && _timeouts.empty() && _intervals.empty();
}


void Strand_Manual::runUntilDone() {
  while (!isDone()) {
    run();
  }
}


void Strand_Manual::runImmediate() {
  std::vector<std::shared_ptr<ImmediateObject_Manual>> immediates{};
  std::unique_lock lock{mutex_};
  immediates = std::move(immediates_);
  lock.unlock();
  SetCurrent current{shared_from_this()};
  for (auto& immediate : immediates) {
    if (immediate->callback_) {
      immediate->callback_();
    }
  }
}


void Strand_Manual::runInterval() {
  std::vector<std::shared_ptr<IntervalObject_Manual>> intervals{};
  {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard lock{mutex_};
    auto i = intervals_.begin();
    while (i != intervals_.end()) {
      if (now >= (*i)->timeout_) {
        intervals.push_back(std::move(*i));
        i = intervals_.erase(i);
      } else {
        ++i;
      }
    }
  }
  {
    SetCurrent current{shared_from_this()};
    for (auto &interval : intervals) {
      if (interval->callback_) {
        interval->callback_();
      }
    }
  }
  {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard lock{mutex_};
    for (auto& interval : intervals) {
      interval->timeout_ = now + interval->delay_;
      intervals_.push_back(std::move(interval));
    }
  }
}


void Strand_Manual::runTimeout() {
  std::vector<std::shared_ptr<TimeoutObject_Manual>> timeouts{};
  auto now = std::chrono::steady_clock::now();

  std::unique_lock lock{mutex_};
  auto i = timeouts_.begin();
  while (i != timeouts_.end()) {
    if (now >= (*i)->timeout_) {
      timeouts.push_back(std::move(*i));
      i = timeouts_.erase(i);
    } else {
      ++i;
    }
  }
  lock.unlock();

  SetCurrent current{shared_from_this()};
  for (auto& timeout : timeouts) {
    if (timeout->callback_) {
      timeout->callback_();
    }
  }
}


void ImmediateObject_Manual::clear() {
  if (auto strand = strand_.lock()) {
    std::lock_guard lock{strand->mutex_};
    strand->immediates_.erase(
        std::remove_if(strand->immediates_.begin(), strand->immediates_.end(), [this](auto& x) {
          return x.get() == this;
        }), strand->immediates_.end());
  }
}

void IntervalObject_Manual::clear() {
  if (auto strand = strand_.lock()) {
    std::lock_guard lock{strand->mutex_};
    strand->intervals_.erase(
        std::remove_if(strand->intervals_.begin(), strand->intervals_.end(), [this](auto& x) {
          return x.get() == this;
        }), strand->intervals_.end());
  }
}

void TimeoutObject_Manual::clear() {
  if (auto strand = strand_.lock()) {
    std::lock_guard lock{strand->mutex_};
    strand->timeouts_.erase(
        std::remove_if(strand->timeouts_.begin(), strand->timeouts_.end(), [this](auto& x) {
          return x.get() == this;
        }), strand->timeouts_.end());
  }
}
