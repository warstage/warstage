// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./promise.h"


std::shared_ptr<Strand_base> PromiseUtils::strand_;


std::shared_ptr<Strand_base> PromiseUtils::Strand() {
  if (!strand_) {
    strand_ = Strand::getMain();
  }
  return strand_;
}


Promise<void> PromiseUtils::all(const std::vector<Promise<void>>& promises) {
  Promise<void> deferred{};
  auto done = std::make_shared<std::atomic<std::size_t>>(0);
  for (auto& promise : promises) {
    promise.onResolve<void>([deferred, done, size = promises.size()]() {
      if (++*done == size) {
        deferred.resolve().done();
      }
    }).done();
  }

  return deferred;
}
