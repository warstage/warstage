// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__ASYNC__STRAND_MANUAL_H
#define WARSTAGE__ASYNC__STRAND_MANUAL_H

#include "strand.h"
#include "strand-base.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

class ImmediateObject_Manual;
class IntervalObject_Manual;
class TimeoutObject_Manual;

class Strand_Manual : public Strand_base, public std::enable_shared_from_this<Strand_Manual> {
  friend class ImmediateObject_Manual;
  friend class IntervalObject_Manual;
  friend class TimeoutObject_Manual;

  mutable std::mutex mutex_{};
  std::vector<std::shared_ptr<ImmediateObject_Manual>> immediates_{};
  std::vector<std::shared_ptr<IntervalObject_Manual>> intervals_{};
  std::vector<std::shared_ptr<TimeoutObject_Manual>> timeouts_{};

public:
  std::shared_ptr<ImmediateObject> setImmediate(std::function<void()> callback) override;
  std::shared_ptr<IntervalObject> setInterval(std::function<void()> callback, double delay) override;
  std::shared_ptr<TimeoutObject> setTimeout(std::function<void()> callback, double delay) override;

  void execute(std::function<void()> callback);

  void run();

  bool isDone();
  void runUntilDone();

private:
  void runImmediate();
  void runInterval();
  void runTimeout();
};


class ImmediateObject_Manual : public ImmediateObject {
  friend class Strand_Manual;
  std::mutex mutex_{};
  std::weak_ptr<Strand_Manual> strand_{};
  std::function<void()> callback_{};
  void clear() override;
};

class IntervalObject_Manual : public IntervalObject {
  friend class Strand_Manual;
  std::mutex mutex_{};
  std::weak_ptr<Strand_Manual> strand_{};
  std::function<void()> callback_;
  std::chrono::steady_clock::time_point timeout_;
  std::chrono::steady_clock::duration delay_;
  void clear() override;
};

class TimeoutObject_Manual : public TimeoutObject {
  friend class Strand_Manual;
  std::mutex mutex_{};
  std::weak_ptr<Strand_Manual> strand_{};
  std::function<void()> callback_;
  std::chrono::steady_clock::time_point timeout_;
  void clear() override;
};

#endif
