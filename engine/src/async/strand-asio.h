// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__ASYNC__STRAND_ASIO_H
#define WARSTAGE__ASYNC__STRAND_ASIO_H

#include "strand.h"
#include "strand-base.h"
#include <mutex>
#include <boost/asio.hpp>

class Strand_Asio : public Strand_base, public std::enable_shared_from_this<Strand_Asio> {
  friend class TimeoutObject_Asio;
  friend class IntervalObject_Asio;
  using asio_work_guard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

public:
  struct Context {
    std::shared_ptr<boost::asio::io_context> context_ = std::make_shared<boost::asio::io_context>();
    std::unique_ptr<asio_work_guard> work_{};
    mutable std::mutex mainMutex_;
    std::shared_ptr<Strand_Asio> main_{};
    std::vector<std::unique_ptr<std::thread>> threads_{};
    mutable std::mutex threadsMutex_;

    std::shared_ptr<Strand_Asio> getMain();
    std::shared_ptr<Strand_Asio> makeStrand(const char* label);
    void runUntilStopped(int threadCount);
    void runUntilDone();
    void stop();
  };
  static Context context__;

  static std::shared_ptr<boost::asio::io_context> io_context() { return context__.context_; }
  static std::shared_ptr<Strand_Asio> getMain() { return context__.getMain(); }
  static std::shared_ptr<Strand_Asio> makeStrand(const char* label) { return context__.makeStrand(label); }

  static void runUntilStopped(int threadCount = 1) { context__.runUntilStopped(threadCount); }
  static void runUntilDone() { context__.runUntilDone(); }
  static void stop() { context__.stop(); }

  boost::asio::strand<boost::asio::any_io_executor> strand_;
  std::string label_;

  struct SetCurrent {
    std::shared_ptr<Strand_Asio> strand_;
    explicit SetCurrent(std::shared_ptr<Strand_Asio> strand) : strand_{std::move(strand)} {
      assert(!current__);
      assert(strand_->strand_.running_in_this_thread());
      current__ = strand_;
    }
    ~SetCurrent() {
      assert(current__ == strand_);
      current__ = nullptr;
    }
  };

  Strand_Asio(const boost::asio::any_io_executor& executor, const char* label);

  Strand_Asio(const Strand_Asio&) = delete;
  Strand_Asio(Strand_Asio&&) = delete;
  Strand_Asio& operator=(const Strand_Asio&) = delete;
  Strand_Asio& operator=(Strand_Asio&&) = delete;

  std::shared_ptr<TimeoutObject> setTimeout(std::function<void()> callback, double delay) override;
  std::shared_ptr<IntervalObject> setInterval(std::function<void()> callback, double delay) override;
  std::shared_ptr<ImmediateObject> setImmediate(std::function<void()> callback) override;
};

class TimeoutObject_Asio : public TimeoutObject {
  friend class Strand_Asio;
  std::mutex mutex_{};
  std::function<void()> callback_;
  boost::asio::steady_timer timer_;
  static void dispatch(const std::weak_ptr<Strand_Asio>& s, const std::shared_ptr<TimeoutObject_Asio>& timeout);
  void clear() override;
public:
  explicit TimeoutObject_Asio(boost::asio::any_io_executor executor) : timer_{executor} { }
};

class IntervalObject_Asio : public IntervalObject {
  friend class Strand_Asio;
  std::mutex mutex_{};
  std::function<void()> callback_;
  boost::asio::steady_timer timer_;
  static void dispatch(const std::weak_ptr<Strand_Asio>& strand, const std::shared_ptr<IntervalObject_Asio>& interval, std::chrono::milliseconds delay);
  void clear() override;
public:
  explicit IntervalObject_Asio(boost::asio::any_io_executor executor) : timer_{executor} { }
};

class ImmediateObject_Asio : public ImmediateObject {
  friend class Strand_Asio;
  std::mutex mutex_{};
  std::function<void()> callback_;
  static void dispatch(const std::weak_ptr<Strand_Asio>& strand, const std::shared_ptr<ImmediateObject_Asio>& immediate);
  void clear() override;
};


#endif
