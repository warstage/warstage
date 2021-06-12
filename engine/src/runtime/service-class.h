// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__SERVICE_CLASS_H
#define WARSTAGE__RUNTIME__SERVICE_CLASS_H

#include "async/promise.h"


class ServiceClass {
  friend class Federate;
  friend class Federation;

  Federate* federate_;
  std::string className_;
  std::function<Promise<Value>(const Value&, const std::string&)> serviceProvider_{};

  ServiceClass(Federate& federate, std::string className);

public:
  [[nodiscard]] const std::string& getName() const { return className_; }

  void define(std::function<Promise<Value>(const Value&, const std::string&)> serviceProvider);
  void define(std::function<Promise<Value>(const Value&)> serviceProvider) {
    define([serviceProvider](const Value& params, const std::string&) {
      return serviceProvider(params);
    });
  }

  void undefine();

  //[[nodiscard]] Promise<Value> requestFulhack(const std::string& subjectId, const Value& params); // AssertFederateStrand
  [[nodiscard]] Promise<Value> request(const Value& params); // AssertFederateStrand
};


#endif
