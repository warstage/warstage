// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "shutdownable.h"


Promise<void> Shutdownable::shutdown() {
  std::unique_lock lock{shutdownMutex_};
  if (!shutdownPromise_) {
    shutdownPromise_ = std::make_unique<Promise<void>>();
    lock.unlock();
    shutdownPromise_->resolve(shutdown_()).done();
  }
  return *shutdownPromise_;
}
