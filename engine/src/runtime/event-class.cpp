// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./event-class.h"
#include "federation.h"
#include "./federate.h"


EventClass::EventClass(Federate& federate, std::string className) :
    federate_{&federate},
    className_{std::move(className)} {
}


void EventClass::subscribe(std::function<void(const Value&)> eventSubscriber) {
  std::lock_guard federate_lock{federate_->mutex_};
  eventSubscribers_.push_back(std::move(eventSubscriber));
}


void EventClass::dispatch(const Value& params, double delay) {
  federate_->dispatchEvent(*federate_, className_.c_str(), params, delay, 0.0);
}
