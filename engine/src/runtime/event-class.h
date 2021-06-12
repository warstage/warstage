// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__EVENT_CLASS_H
#define WARSTAGE__RUNTIME__EVENT_CLASS_H

#include "async/promise.h"

class Federate;


class EventClass {
  friend class Federate;
  friend class Federation;

  Federate* federate_;
  std::string className_;
  std::vector<std::function<void(const Value&)>> eventSubscribers_{};

  EventClass(Federate& federate, std::string className);

public:
  [[nodiscard]] const std::string& getName() const { return className_; }
  void subscribe(std::function<void(const Value&)> eventSubscriber);
  void dispatch(const Value& params, double delay = 0.0);
};


#endif
