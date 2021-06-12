// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./federate.h"
#include "./federation.h"
#include "./service-class.h"
#include "./runtime.h"


ServiceClass::ServiceClass(Federate& federate, std::string className) :
    federate_{&federate},
    className_{std::move(className)} {
}


void ServiceClass::define(std::function<Promise<Value>(const Value&, const std::string&)> serviceProvider) {
  std::lock_guard federate_lock{federate_->mutex_};
  serviceProvider_ = std::move(serviceProvider);
}


void ServiceClass::undefine() {
  std::lock_guard federate_lock{federate_->mutex_};
  serviceProvider_ = nullptr;
}


/*Promise<Value> ServiceClass::requestFulhack(const std::string& subjectId, const Value& params) {
    LOG_ASSERT(_federate->AssertFederateStrand());
    return _federate->RequestService(_className.c_str(), params, subjectId, _federate);
}*/


Promise<Value> ServiceClass::request(const Value& params) {
  LOG_ASSERT(federate_->isFederateStrandCurrent());
  return federate_->requestService(className_.c_str(), params, federate_->runtime_->getSubjectId_safe(), federate_);
}
