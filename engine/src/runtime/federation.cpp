// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./event-class.h"
#include "./federation.h"
#include "./federate.h"
#include "./runtime.h"


const ObjectId Federation::SystemFederationId = ObjectId{};

static std::atomic_int debugCounter = 0;


Federation::Federation(Runtime* runtime, ObjectId federationId) :
    runtime_{runtime},
    federationId_{federationId} {
  LOG_LIFECYCLE("%p Federation + %d", this, ++debugCounter);
}


Federation::~Federation() {
  LOG_LIFECYCLE("%p Federation ~ %d", this, --debugCounter);
  std::lock_guard lock{mutex_};
  for (auto* federate : federates_) {
    LOG_ASSERT(federate->shutdownCompleted());
    std::lock_guard lock2{federate->federationMutex_};
    LOG_ASSERT(!federate->federation_);
  }
}


void Federation::setOwnershipPolicy(std::function<bool(const Federate&, const std::string&)> policy) {
  std::lock_guard federate_lock(mutex_);
  if (policy) {
    ownershipPolicy_ = std::move(policy);
  } else {
    ownershipPolicy_ = defaultOwnershipPolicy;
  }
}


bool Federation::defaultOwnershipPolicy(const Federate&, const std::string&) {
  return true;
}


void Federation::removeUnreferencedMasterInstances_unsafe() {
  masterInstances_.erase(
      std::remove_if(masterInstances_.begin(), masterInstances_.end(), [](auto& x) {
        return !x->refCount_;
      }),
      masterInstances_.end());
}


void Federation::dispatchEvent(Federate& originator, const char* event, const Value& params, double delay, double latency) {
  std::lock_guard federation_lock{mutex_};

  for (auto federate : federates_) {
    if (federate != &originator) {
      std::string eventName{event};
      auto federate_weak = federate->weak_from_this();
      federate->postAsyncTask([federate_weak, eventName, params, delay, latency]() {
        if (auto federate_lock = federate_weak.lock()) {
          federate_lock->eventDelay_ = delay;
          federate_lock->eventLatency_ = latency;
          federate_lock->enterBlock_strand();
          for (auto eventSubscriber : federate_lock->getEventClass(eventName.c_str()).eventSubscribers_) {
            eventSubscriber(params);
          }
          if (federate_lock->eventCallback_) {
            federate_lock->eventCallback_(eventName.c_str(), params);
          }
          federate_lock->leaveBlock_strand();
          federate_lock->eventDelay_ = 0.0;
          federate_lock->eventLatency_ = 0.0;
        }
      });
    }
  }
}


Promise<Value> Federation::requestService(const char* service, const Value& params, const std::string& subjectId, Federate* originator) {
  std::lock_guard federation_lock{mutex_};

  if (auto serviceClass = findServiceProvider_unsafe(service, originator)) {
    Promise<Value> deferred{};
    auto serviceProvider = serviceClass->serviceProvider_;
    serviceClass->federate_->postAsyncTask([deferred, serviceProvider, params, subjectId]() {
      deferred.resolve(serviceProvider(params, subjectId)).done();
    });
    return deferred;
  }

  std::vector<std::weak_ptr<Federate>> federates{};
  for (auto federate : federates_) {
    if (federate != originator) {
      federates.push_back(federate->weak_from_this());
    }
  }

  return requestService(service, params, REASON(500, "unknown service: %s", service), subjectId, federates);
}


Promise<Value> Federation::requestService(
    std::string service,
    const Value& params,
    const Value& defaultReason,
    std::string subjectId,
    std::vector<std::weak_ptr<Federate>> federates) {
  while (!federates.empty()) {
    auto federate = federates.back().lock();
    federates.pop_back();
    if (federate && Federate::tryGetServiceCallback(federate)) {
      Promise<Value> deferred{};
      federate->postAsyncTask(
          [deferred, federate, service = std::move(service), params, defaultReason, subjectId = std::move(subjectId), federates = std::move(federates)]() {
            if (auto serviceCallback = Federate::tryGetServiceCallback(federate)) {
              serviceCallback(service.c_str(), params, subjectId).then<void>([deferred, service](const Value& value) {
                deferred.resolve(value).done();
              }, [deferred, federate, service, params, defaultReason, subjectId, federates](const std::exception_ptr& e) {
                Value reason = defaultReason;
                try {
                  std::rethrow_exception(e);
                } catch (const Value& r) {
                  reason = r;
                } catch (...) {
                }
                auto federation = federate->federation_;
                deferred.resolve(federation->requestService(service, params, reason, subjectId, federates)).done();
              }).done();
            } else {
              auto federation = federate->federation_;
              deferred.resolve(federation->requestService(service, params, defaultReason, subjectId, federates)).done();
            }
          });
      return deferred;
    }
  }

  return Promise<Value>{}.reject<Value>(defaultReason);
}


ServiceClass* Federation::findServiceProvider_unsafe(const char* name, Federate* exclude) {
  // assert(!mutex_.try_lock());

  for (auto federate : federates_) {
    if (federate != exclude) {
      std::lock_guard federate_lock{federate->mutex_};
      for (auto& serviceClass : federate->serviceClasses_)
        if (serviceClass->className_ == name && serviceClass->serviceProvider_)
          return serviceClass.get();
    }
  }
  return nullptr;
}


void Federation::tryScheduleImmediateSynchronizeOthers_unsafe(Federate* exception) {
  // assert(!mutex_.try_lock());

  for (auto federate : federates_) {
    if (federate != exception) {
      std::lock_guard federate_lock{federate->mutex_};
      federate->tryScheduleImmediateSynchronize_unsafe();
    }
  }
}


const char* str(FederationType value) {
  switch (value) {
    case FederationType::None: return "-";
    case FederationType::Battle: return "Battle";
    case FederationType::Lobby: return "Match";
  }
}
