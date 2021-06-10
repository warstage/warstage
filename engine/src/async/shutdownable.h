// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__ASYNC__SHUTDOWNABLE_H
#define WARSTAGE__ASYNC__SHUTDOWNABLE_H

#include "./promise.h"


class Shutdownable {
  std::mutex shutdownMutex_{};
  std::unique_ptr<Promise<void>> shutdownPromise_{};

public:
  virtual ~Shutdownable() noexcept {
    LOG_ASSERT(shutdownCompleted());
  }

  Promise<void> shutdown();

  [[nodiscard]] bool shutdownStarted() noexcept {
    std::lock_guard lock{shutdownMutex_};
    return shutdownPromise_ != nullptr;
  }

  [[nodiscard]] bool shutdownCompleted() noexcept {
    std::lock_guard lock{shutdownMutex_};
    return shutdownPromise_ != nullptr && shutdownPromise_->isFulfilled();
  }

protected:
  virtual Promise<void> shutdown_() = 0;
};


#endif
