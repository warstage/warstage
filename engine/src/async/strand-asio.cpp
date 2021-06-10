// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./strand-asio.h"
#include "utilities/logging.h"

#ifdef WARSTAGE_ENABLE_ASYNC_MONKEY
#include <random>
#endif


Strand_Asio::Context Strand_Asio::context__;


Strand_Asio::Strand_Asio(const boost::asio::any_io_executor& executor, const char* label) : strand_{executor}, label_{label} {
}


std::shared_ptr<Strand_Asio> Strand_Asio::Context::getMain() {
  std::lock_guard lock{mainMutex_};
  if (!main_)
    main_ = std::make_shared<Strand_Asio>(context_->get_executor(), "main");
  return main_;
}


std::shared_ptr<Strand_Asio> Strand_Asio::Context::makeStrand(const char* label) {
  LOG_ASSERT(label);
  return std::make_shared<Strand_Asio>(context_->get_executor(), label);
}


void Strand_Asio::Context::runUntilStopped(int threadCount) {
  work_ = std::make_unique<asio_work_guard>(context_->get_executor());

  {
    std::lock_guard lock{threadsMutex_};
    for (int i = 1; i < threadCount; ++i) {
      threads_.push_back(std::make_unique<std::thread>([this]() {
        context_->run();
      }));
    }
  }
  context_->run();
  assert(!work_);
  context_->reset();
  {
    std::lock_guard lock{threadsMutex_};
    assert(threads_.empty());
  }
}


void Strand_Asio::Context::runUntilDone() {
  context_->run();
  context_->reset();
}


void Strand_Asio::Context::stop() {
  work_ = nullptr;
  context_->stop();
  for (auto& thread : threads_) {
    thread->join();
  }
  std::lock_guard lock{threadsMutex_};
  threads_.clear();
}


std::shared_ptr<TimeoutObject> Strand_Asio::setTimeout(std::function<void()> callback, double delay) {
  auto weak_ = weak_from_this();
  auto result = std::make_shared<TimeoutObject_Asio>(strand_.get_inner_executor());
  std::weak_ptr<TimeoutObject> r = result;
  result->callback_ = callback;
  result->timer_.expires_after(std::chrono::milliseconds{static_cast<long>(delay)});
  result->timer_.async_wait(boost::asio::bind_executor(strand_, [weak_, result](boost::system::error_code ec) {
    if (ec) {
      LOG_W("error_code %s", ec.message().c_str());
    }
    TimeoutObject_Asio::dispatch(weak_, result);
  }));
  return result;
}


std::shared_ptr<IntervalObject> Strand_Asio::setInterval(std::function<void()> callback, double delay) {
  auto weak_ = weak_from_this();
  auto result = std::make_shared<IntervalObject_Asio>(strand_.get_inner_executor());
  std::weak_ptr<IntervalObject> r = result;
  auto d = std::chrono::milliseconds{static_cast<long>(delay)};
  result->callback_ = callback;
  result->timer_.expires_after(d);
  result->timer_.async_wait(boost::asio::bind_executor(strand_, [weak_, result, d](boost::system::error_code ec) {
    if (ec) {
      LOG_W("error_code %s", ec.message().c_str());
    }
    IntervalObject_Asio::dispatch(weak_, result, d);
  }));
  return result;
}


std::shared_ptr<ImmediateObject> Strand_Asio::setImmediate(std::function<void()> callback) {
  auto weak_ = weak_from_this();
  auto result = std::make_shared<ImmediateObject_Asio>();
#ifdef WARSTAGE_ENABLE_ASYNC_MONKEY
  thread_local std::random_device random_device;
    thread_local std::default_random_engine random_engine(random_device());
    result->_callback = [this_shared = shared_from_this(), callback = std::move(callback)]() {
        this_shared->SetTimeout(callback, std::uniform_int_distribution<int>(100, 400)(random_engine));
    };
#else
  result->callback_ = std::move(callback);
#endif
  boost::asio::post(strand_, [weak_, result]() {
    ImmediateObject_Asio::dispatch(weak_, result);
  });
  return result;
}


void TimeoutObject_Asio::dispatch(const std::weak_ptr<Strand_Asio>& strand, const std::shared_ptr<TimeoutObject_Asio>& timeout) {
  auto strandLock = strand.lock();
  if (!strandLock) {
    LOG_X("TimeoutObject_Asio: deleted strand");
    return;
  }
  std::function<void()> callback;
  std::unique_lock lock{timeout->mutex_};
  callback = std::move(timeout->callback_);
  lock.unlock();
  if (callback) {
    try {
      Strand_base::SetCurrent current{strandLock};
      callback();
    } catch (std::exception& e) {
      LOG_EXCEPTION(e);
    } catch (...) {
      LOG_EXCEPTION(std::runtime_error("setTimeout"));
    }
  }
}


void TimeoutObject_Asio::clear() {
  std::lock_guard lock{mutex_};
  callback_ = nullptr;
}


void IntervalObject_Asio::dispatch(const std::weak_ptr<Strand_Asio>& strand, const std::shared_ptr<IntervalObject_Asio>& interval, std::chrono::milliseconds delay) {
  std::function<void()> callback;
  std::unique_lock lock{interval->mutex_};
  callback = interval->callback_;
  lock.unlock();
  if (callback) {
    try {
      interval->timer_.expires_after(std::chrono::milliseconds{delay});
      auto strandLock = strand.lock();
      if (!strandLock) {
        LOG_X("IntervalObject_Asio: deleted strand");
        return;
      }
      interval->timer_.async_wait(boost::asio::bind_executor(strandLock->strand_, [strand, interval, delay](boost::system::error_code ec) {
        if (ec) {
          LOG_W("error_code %s", ec.message().c_str());
        }
        dispatch(strand, interval, delay);
      }));
      Strand_base::SetCurrent current{strandLock};
      callback();
    } catch (std::exception& e) {
      LOG_EXCEPTION(e);
    } catch (...) {
      LOG_EXCEPTION(std::runtime_error("setInterval"));
    }
  }
}


void IntervalObject_Asio::clear() {
  std::lock_guard lock{mutex_};
  callback_ = nullptr;
}


void ImmediateObject_Asio::dispatch(const std::weak_ptr<Strand_Asio>& strand, const std::shared_ptr<ImmediateObject_Asio>& immediate) {
  auto strandLock = strand.lock();
  if (!strandLock) {
    LOG_X("ImmediateObject_Asio: deleted strand");
    return;
  }
  std::function<void()> callback;
  std::unique_lock lock{immediate->mutex_};
  callback = std::move(immediate->callback_);
  lock.unlock();
  if (callback) {
    try {
      Strand_base::SetCurrent current{strandLock};
      callback();
    } catch (std::exception& e) {
      LOG_EXCEPTION(e);
    } catch (...) {
      LOG_EXCEPTION(std::runtime_error("setImmediate"));
    }
  }
}


void ImmediateObject_Asio::clear() {
  std::lock_guard lock{mutex_};
  callback_ = nullptr;
}
