// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./federate.h"
#include "./federation.h"
#include "./object.h"
#include "./ownership.h"
#include "./runtime.h"
#include "./session.h"
#include "./session-federate.h"
#include "value/json.h"
#include <iostream>


static std::atomic_int debugCounter = 0;


Federate::Federate(Runtime& runtime, const char* federateName, std::shared_ptr<Strand_base> strand) :
    runtime_{&runtime},
    federateName_{federateName},
    strand_{std::move(strand)}
{
  LOG_ASSERT(strand_);
  LOG_LIFECYCLE("%p Federate + %d %s", this, ++debugCounter, federateName);
}


Federate::~Federate() {
  LOG_LIFECYCLE("%p Federate ~ %d %s", this, --debugCounter, federateName_.c_str());
  LOG_ASSERT(shutdownStarted());
  LOG_ASSERT(shutdownCompleted());
  {
    std::lock_guard lock{federationMutex_};
    LOG_ASSERT(!federation_);
  }
  std::lock_guard federate_lock{mutex_};
  LOG_ASSERT(objectInstances_.empty());
}


void Federate::startup(ObjectId federationId) {
  LOG_ASSERT(!weak_from_this().expired()); // federates must be allocated with std::make_shared

  std::lock_guard startupShutdownLock{startupShutdownMutex_};

  auto federation = runtime_->acquireFederation_safe(federationId, true);

  if (shutdownStarted()) {
    log_error("FEDERATE",
        makeString("FEDERATE %s %s startup shutdown", federateName_.c_str(), str(federation->getFederationType())).c_str(),
        makeStack(__FILE__, __LINE__).c_str(),
        LogLevel::Warning);
    return;
  }

  federationId_ = federationId;
  setFederation_safe(federation);

  std::lock_guard federate_lock{mutex_};
  tryScheduleImmediateSynchronize_unsafe();
}


Promise<void> Federate::shutdown_() {
  LOG_LIFECYCLE("%p Federate Shutdown %s", this, federateName_.c_str());

  std::lock_guard startupShutdownLock{startupShutdownMutex_};

  co_await *strand_;
  LOG_ASSERT(isFederateStrandCurrent());

  clearImmediateSyncrhonize_safe();

  Federation* const federation = clearFederation_safe();

  std::shared_ptr<Federate> shutdownAnotherFederate{};

  {
    std::unique_lock<std::mutex> federation_lock{};
    if (federation) {
      federation_lock = std::unique_lock{federation->mutex_};
    }
    std::lock_guard federate_lock1{mutex_};

    for (auto& objectInstance : objectInstances_) {
      if (auto masterInstance = objectInstance->masterInstance_) {
        unpublishAndRemoveObjectInstanceFromOwnershipMap(*objectInstance);
        --masterInstance->refCount_;
        objectInstance->masterInstance_ = nullptr;
      }
    }
    objectInstances_.clear();
    discoveredInstances_.clear();
    undiscoveredInstances_.clear();

    if (federation) {
      federation->removeUnreferencedMasterInstances_unsafe();
      federation->tryScheduleImmediateSynchronizeOthers_unsafe(this);
    }
    LOG_ASSERT(objectInstances_.empty());

    if (federation) {
      bool shutdownFederation = std::find_if(
          federation->federates_.begin(),
          federation->federates_.end(),
          [](auto x) { return x->isPrincipalFederate(); }) == federation->federates_.end();
      if (shutdownFederation && !federation->federates_.empty()) {
        shutdownAnotherFederate = federation->federates_.front()->shared_from_this();
      }
    }
  }

  if (shutdownAnotherFederate) {
    LOG_ASSERT(isFederateStrandCurrent());
    co_await shutdownAnotherFederate->shutdown();
    LOG_ASSERT(isFederateStrandCurrent());
  }

  if (federation) {
    runtime_->releaseFederation_safe(federation);
  }
}


bool Federate::isPrincipalFederate() const {
  return runtime_->getProcessType() == ProcessType::Daemon || dynamic_cast<const SessionFederate*>(this) == nullptr;
}


bool Federate::ownershipPolicy(const std::string& propertyName) const {
  return federation_->ownershipPolicy_(*this, propertyName);
}


ObjectId Federate::getFederationId() const {
  return federationId_;
}


std::string Federate::getDescription() const {
  std::ostringstream os{};
  os << str(runtime_->getProcessType());
  os << runtime_->getProcessId().str().c_str();
  os << "-" << federateName_;
  return os.str();
}


ObjectRef Federate::getObject(ObjectId objectId) const {
  LOG_ASSERT(isFederateStrandCurrent());

  for (auto& instance : objectInstances_) {
    if (instance->objectId_ == objectId) {
      return ObjectRef{instance};
    }
  }
  return ObjectRef{};
}


ObjectClass& Federate::getObjectClass(const char* name) {
  std::lock_guard federate_lock(mutex_);
  return *getObjectClass_unsafe(name);
}


ObjectClass* Federate::getObjectClass_unsafe(const char* name) {
  for (auto& objectClass : objectClasses_) {
    if (objectClass->className_ == name) {
      return objectClass.get();
    }
  }
  auto result = new ObjectClass{*this, name};
  objectClasses_.push_back(std::unique_ptr<ObjectClass>{result});
  return result;
}


void Federate::setObjectCallback(std::function<void(ObjectRef)> callback) {
  std::lock_guard federate_lock(mutex_);
  objectCallback_ = std::move(callback);
}


EventClass& Federate::getEventClass(const char* name) {
  std::lock_guard federate_lock(mutex_);

  for (auto& eventClass : eventClasses_) {
    if (eventClass->className_ == name) {
      return *eventClass;
    }
  }

  auto result = new EventClass{*this, std::string(name)};
  eventClasses_.push_back(std::unique_ptr<EventClass>{result});

  return *result;
}


void Federate::setEventCallback(std::function<void(const char*, const Value&)> callback) {
  std::lock_guard federate_lock(mutex_);
  eventCallback_ = std::move(callback);
}


void Federate::dispatchEvent(Federate& originator, const char* event, const Value& params, double delay, double latency) {
  std::lock_guard federate_lock{federationMutex_};
  if (federation_) {
    federation_->dispatchEvent(originator, event, params, delay, latency);
  }
}


ServiceClass& Federate::getServiceClass(const char* name) {
  std::lock_guard federate_lock(mutex_);

  for (auto& serviceClass : serviceClasses_) {
    if (serviceClass->className_ == name) {
      return *serviceClass;
    }
  }

  auto result = new ServiceClass{*this, std::string(name)};
  serviceClasses_.push_back(std::unique_ptr<ServiceClass>{result});

  return *result;
}


void Federate::setServiceCallback(std::function<Promise<Value>(const char*, const Value&, const std::string&)> callback) {
  std::lock_guard federate_lock(mutex_);
  serviceCallback_ = std::move(callback);
}


std::function<Promise<Value>(const char*, const Value&, const std::string&)> Federate::tryGetServiceCallback(const std::weak_ptr<Federate>& federate) {
  if (auto federate_lock = federate.lock()) {
    std::lock_guard federate_guard2{federate_lock->mutex_};
    return federate_lock->serviceCallback_;
  }
  return nullptr;
}


Promise<Value> Federate::requestService(const char* service, const Value& params, const std::string& subjectId, Federate* originator) {
  std::lock_guard federate_lock{federationMutex_};
  auto result = federation_
      ? federation_->requestService(service, params, subjectId, originator)
      : Promise<Value>{}.reject<Value>(REASON(404, "%s rejected: no federation", service));
  auto logger = makeRequestLogger(service, subjectId);
  return result.then<Promise<Value>>([logger](const Value& value) {
    logger(true);
    return resolve(value);
  }, [logger](const std::exception_ptr& e) -> Promise<Value> {
    logger(false);
    std::rethrow_exception(e);
  });
}


void Federate::setOwnershipCallback(std::function<void(ObjectRef object, Property& property, OwnershipNotification notification)> callback) {
  std::lock_guard federate_lock(mutex_);
  if (callback) {
    ownershipCallback_ = std::move(callback);
  } else {
    ownershipCallback_ = defaultOwnershipCallback;
  }
}


void Federate::defaultOwnershipCallback(ObjectRef object, Property& property, OwnershipNotification notification) {
  switch (notification) {
    case OwnershipNotification::RequestOwnershipAssumption:
      if (property.getOwnershipState() & OwnershipStateFlag::NotTryingToAcquire) {
        property.modifyOwnershipState(OwnershipOperation::OwnershipAcquisitionIfAvailable);
      }
      break;
    case OwnershipNotification::RequestOwnershipRelease:
      if (property.getOwnershipState() & OwnershipStateFlag::AskedToRelease) {
        property.modifyOwnershipState(OwnershipOperation::OwnershipReleaseSuccess);
      }
      break;
    default:
      break;
  }
}


void Federate::synchronize_strand() {
  LOG_ASSERT(isFederateStrandCurrent());

  Federation* federation;
  {
    std::lock_guard federate_lock{federationMutex_};
    federation = federation_;
  }
  if (federation) {
    enterBlock_strand();
    updateCurrentTime_strand();
    {
      std::lock_guard federation_lock{federation->mutex_};
      std::lock_guard federate_lock{mutex_};
      bool changed = false;
      if (synchronizeChangesFromFederateToFederation_strand(federation)) {
        changed = true;
      }
      if (synchronizeChangesFromFederationToFederate_strand(federation)) {
        changed = true;
      }
      if (changed) {
        federation->tryScheduleImmediateSynchronizeOthers_unsafe(this);
      }
    }
    notifyChangesToFederateObservers_strand();
    {
      std::lock_guard federation_lock{federation->mutex_};
      std::lock_guard federate_lock{mutex_};
      removeDeletedByMaster();
    }
    leaveBlock_strand();
  }
}


void Federate::enterBlock_strand() {
  LOG_ASSERT(isFederateStrandCurrent());
  std::lock_guard federate_lock{mutex_};
  ++blockCounter_;
}


void Federate::leaveBlock_strand() {
  LOG_ASSERT(isFederateStrandCurrent());
  std::lock_guard federate_lock{mutex_};
  if (--blockCounter_ == 0 && deferredSynchronize_) {
    deferredSynchronize_ = false;
    tryScheduleImmediateSynchronize_unsafe();
  }
}

void Federate::updateCurrentTime_strand() {
  LOG_ASSERT(isFederateStrandCurrent());
  auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - baseTimePoint_).count();
  currentTime_ = 0.000001 * static_cast<double>(microseconds);
}


void Federate::setFederation_safe(Federation* federation) {
  LOG_ASSERT(federation);
  {
    std::lock_guard lock{federationMutex_};
    LOG_ASSERT(!federation_);
    federation_ = federation;
  }
  {
    std::lock_guard lock{federation->mutex_};
    federation->federates_.push_back(this);
  }
}


Federation* Federate::clearFederation_safe() {
  Federation* federation;
  {
    std::lock_guard lock{federationMutex_};
    federation = federation_;
    federation_ = nullptr;
  }
  if (federation) {
    std::lock_guard lock{federation->mutex_};
    federation->federates_.erase(
        std::remove(federation->federates_.begin(), federation->federates_.end(), this),
        federation->federates_.end());
  }
  return federation;
}


void Federate::postAsyncTask(std::function<void()> task) {
  strand_->setImmediate(std::move(task));
}


void Federate::clearImmediateSyncrhonize_safe() {
  std::lock_guard federate_lock{mutex_};
  if (immediateSynchronize_) {
    clearImmediate(*immediateSynchronize_);
    immediateSynchronize_ = nullptr;
  }
}


void Federate::tryScheduleImmediateSynchronize_unsafe() {
  if (blockCounter_) {
    deferredSynchronize_ = true;
  } else if (!immediateSynchronize_) {
    auto weak_ = weak_from_this();
    immediateSynchronize_ = strand_->setImmediate([weak_]() {
      try {
        if (auto this_ = weak_.lock()) {
          {
            std::lock_guard federate_lock{this_->mutex_};
            this_->immediateSynchronize_ = nullptr;
          }
          this_->synchronize_strand();
        }
      } catch (const std::bad_weak_ptr&) {
        // sometimes happens when another thread is disposing the federate
      }
    });
  }
}


bool Federate::synchronizeChangesFromFederateToFederation_strand(Federation* federation) {
  assert(federation);
  // assert(!federation->_mutex.try_lock()); // ??
  // assert(!_mutex.try_lock()); // ??
  LOG_ASSERT(isFederateStrandCurrent());

  bool changed = false;

  auto i = objectInstances_.begin();
  while (i != objectInstances_.end()) {
    if ((*i)->deletedByObject_) {
      if (auto masterInstance = (*i)->masterInstance_) {
        unpublishAndRemoveObjectInstanceFromOwnershipMap(**i);
        masterInstance->deleted_ = true;
        --masterInstance->refCount_;
        (*i)->masterInstance_ = nullptr;
      }
      i = objectInstances_.erase(i);
      changed = true;
    } else {
      ++i;
    }
  }

  for (auto& objectInstance : objectInstances_) {
    if (!objectInstance->masterInstance_) {
      if (!objectInstance->spurious_) {
        if (ownershipPolicy(Property::Destructor_str)) {
          auto masterInstance = new MasterInstance{};
          masterInstance->instanceId_ = ++federation->lastInstanceId_;
          masterInstance->objectId_ = objectInstance->objectId_;
          masterInstance->objectClassName_ = objectInstance->objectClass_->className_;
          masterInstance->refCount_ = 1;
          masterInstance->shared_ = std::move(objectInstance->shared_);
          objectInstance->masterInstance_ = masterInstance;
          objectInstance->synchronize_ = true;
          federation->masterInstances_.push_back(std::unique_ptr<MasterInstance>(masterInstance));
          changed = true;
        } else {
          objectInstance->spurious_ = true;
          LOG_W("Spurious object detected: %s (%s)", objectInstance->objectClass_->className_.c_str(), objectInstance->objectId_.str().c_str());
        }
      }
    }
    if (objectInstance->masterInstance_ && objectInstance->synchronize_) {
      for (auto& objectProperty : objectInstance->properties_.Values()) {
        if (objectProperty) {
          auto masterProperty = objectProperty->masterProperty_;
          if (!masterProperty) {
            masterProperty = &objectInstance->masterInstance_->getProperty(*objectProperty);
            objectProperty->masterProperty_ = masterProperty;
            changed = true;
          }

          /*if (!(objectProperty->_instanceOwnership.first & OwnershipStateFlag::Owned)) {
              // TEST
              if (objectProperty->_version3 > masterProperty->_version3) {
                  objectProperty->_version3 = masterProperty->_version3;
              }
          }*/


          if (objectProperty->version3_ > masterProperty->version_) {
            auto& instanceOwnership = objectProperty->instanceOwnership_;
            if (instanceOwnership.first & OwnershipStateFlag::Owned) {
              auto& ownershipMap = objectProperty->masterProperty_->ownershipMap_;
              assertValidateOwnership(ownershipMap, __FILE__, __LINE__);
              if (!masterProperty->owner_) {
                if (objectProperty->masterOwnership_.first == OwnershipState{}) {
                  ownershipMap.push_back(objectProperty.get());
                }
                objectProperty->masterOwnership_ = std::make_pair(instanceOwnership.first, OwnershipNotification::None);
                assertValidateOwnership(ownershipMap, __FILE__, __LINE__);
                objectProperty->masterProperty_->owner_ = findOwnerFederate(ownershipMap);
                masterProperty->assign(*objectProperty);
                changed = true;
              } else if (masterProperty->owner_ == objectProperty.get() || instanceOwnership.second == OwnershipOperation::ForcedOwnershipAcquisition) {
                masterProperty->assign(*objectProperty);
                changed = true;
              } else {
                if (objectProperty->masterOwnership_.first == OwnershipState{}) {
                  ownershipMap.push_back(objectProperty.get());
                }
                objectProperty->masterOwnership_ = std::make_pair(instanceOwnership.first, OwnershipNotification::ForcedOwnershipDivestitureNotification);
                assertValidateOwnership(ownershipMap, __FILE__, __LINE__);
                objectProperty->masterProperty_->owner_ = findOwnerFederate(ownershipMap);
                objectProperty->assign(*masterProperty);
              }
            } else {
              LOG_W("no ownership %s", objectProperty->propertyName_.c_str());
              objectProperty->assign(*masterProperty);
            }
          }
        }
      }
      objectInstance->synchronize_ = false;

      for (auto& objectProperty : objectInstance->properties_.Values()) {
        if (objectProperty && shouldUpdateOwnership(*objectProperty)) {
          if (updateOwnership(objectInstance, *objectProperty)) {
            changed = true;
          }
        }
      }
    }
  }

  federation->removeUnreferencedMasterInstances_unsafe();

  return changed;
}


bool Federate::synchronizeChangesFromFederationToFederate_strand(Federation* federation) {
  assert(federation);
  // assert(!federation->_mutex.try_lock()); // ??
  // assert(!_mutex.try_lock()); // ??
  LOG_ASSERT(isFederateStrandCurrent());

  bool changed = false;
  if (federation->lastInstanceId_ > lastInstanceId_) {
    for (auto& masterInstance : federation->masterInstances_) {
      if (masterInstance->instanceId_ > lastInstanceId_ && !masterInstance->deleted_ && !hasObjectInstance_unsafe(*masterInstance)) {
        undiscoveredInstances_.push_back(masterInstance.get());
        ++masterInstance->refCount_;
      }
    }
    lastInstanceId_ = federation->lastInstanceId_;
  }

  bool tryDiscoverInstances = true;
  while (tryDiscoverInstances) {
    tryDiscoverInstances = false;
    auto i = undiscoveredInstances_.begin();
    while (i != undiscoveredInstances_.end()) {
      auto masterInstance = *i;
      if (masterInstance->deleted_) {
        --masterInstance->refCount_;
        i = undiscoveredInstances_.erase(i);
      } else if (isWellDefined_unsafe(*masterInstance)) {
        i = undiscoveredInstances_.erase(i);

        auto objectId = masterInstance->objectId_;
        std::shared_ptr<ObjectInstance> objectInstance{};
        for (auto& instance : objectInstances_) {
          if (instance->objectId_ == objectId) {
            objectInstance = instance;
            break;
          }
        }
        if (objectInstance) {
          // spurious object
          objectInstance->masterInstance_ = masterInstance;
          for (auto& objectProperty : objectInstance->properties_.Values()) {
            if (objectProperty) {
              LOG_ASSERT(!objectProperty->masterProperty_);
              auto masterProperty = &objectInstance->masterInstance_->getProperty(*objectProperty);
              objectProperty->masterProperty_ = masterProperty;
              objectProperty->version3_ = masterProperty->buffer_ ? masterProperty->version_ - 1 : masterProperty->version_;
              bool owned = objectProperty->canSetValue();
              auto ownershipState = objectProperty->getOwnershipState() & OwnershipStateFlag::AbleToAcquire
                  ? OwnershipState{}
                      + OwnershipStateFlag::Unowned
                      + OwnershipStateFlag::NotAbleToAcquire
                  : OwnershipState{}
                      + OwnershipStateFlag::Unowned
                      + OwnershipStateFlag::AbleToAcquire
                      + OwnershipStateFlag::NotAcquiring
                      + OwnershipStateFlag::NotTryingToAcquire;
              objectProperty->instanceOwnership_.first = ownershipState;
              objectProperty->instanceOwnership_.second = OwnershipOperation::None;
              objectProperty->masterOwnership_.first = ownershipState;
              objectProperty->masterOwnership_.second = owned ? OwnershipNotification::ForcedOwnershipDivestitureNotification : OwnershipNotification::None;
              objectProperty->ownershipVersion_ = 0;
            }
          }
        } else {
          auto objectClass = getObjectClass_unsafe(masterInstance->objectClassName_.c_str());
          objectInstance = std::make_shared<ObjectInstance>(objectClass);
          objectInstance->masterInstance_ = masterInstance;
          objectInstance->objectId_ = masterInstance->objectId_;
          objectInstance->discoveredNotNotified_ = true;
          objectInstance->getProperty(Property::Destructor_cstr);
          objectInstances_.push_back(objectInstance);
          discoveredInstances_.push_back(objectInstance);
          tryDiscoverInstances = true;
        }
      } else {
        ++i;
      }
    }
  }

  for (auto& objectInstance : objectInstances_) {
    if (auto masterInstance = objectInstance->masterInstance_) {
      if (masterInstance->deleted_) {
        objectInstance->deletedByMaster_ = true;
        objectInstance->notify_ = true;
      } else {
        for (auto& objectProperty : objectInstance->properties_.Values()) {
          if (objectProperty) {
            if (auto masterProperty = objectProperty->masterProperty_) {
              if (masterProperty->version_ > objectProperty->version3_) {
                objectProperty->assign(*masterProperty);
                if (objectCallback_ || !objectInstance->objectClass_->observers_.empty()) {
                  objectProperty->changed_ = true;
                  objectInstance->notify_ = true;
                }
              }
              masterProperty->syncFlag_ = true;
            }
          }
        }

        for (auto& masterProperty : masterInstance->properties_.Values()) {
          if (!masterProperty->syncFlag_) {
            auto& objectProperty = objectInstance->getProperty(masterProperty->propertyName_);
            objectProperty.masterProperty_ = masterProperty.get();
            if (masterProperty->version_ > objectProperty.version3_) {
              objectProperty.assign(*masterProperty);
              if (objectCallback_ || !objectInstance->objectClass_->observers_.empty()) {
                objectProperty.changed_ = true;
                objectInstance->notify_ = true;
              }
            }
          }
        }

        for (auto& masterProperty : masterInstance->properties_.Values()) {
          masterProperty->syncFlag_ = false;
        }

        for (auto& objectProperty : objectInstance->properties_.Values()) {
          if (objectProperty && shouldUpdateOwnership(*objectProperty)) {
            if (updateOwnership(objectInstance, *objectProperty)) {
              changed = true;
            }
          }
        }
      }
    }
  }
  return changed;
}


bool Federate::shouldUpdateOwnership(const Property& property) {
  if (auto masterProperty = property.masterProperty_) {
    return property.instanceOwnership_.second != OwnershipOperation::None
        || property.ownershipVersion_ == 0
        || property.ownershipVersion_ != masterProperty->ownershipVersion_;
  }
  return false;
}


bool Federate::updateOwnership(const std::shared_ptr<ObjectInstance>& objectInstance, Property& objectProperty) {
  bool masterOwnershipChanged = false;
  auto masterProperty = objectProperty.masterProperty_;
  LOG_ASSERT(masterProperty);
  auto& instanceOwnership = objectProperty.instanceOwnership_;
  auto& ownershipMap = masterProperty->ownershipMap_;
  assertValidateOwnership(ownershipMap, __FILE__, __LINE__);

  if (objectProperty.masterOwnership_.first != OwnershipState{}) {
    LOG_ASSERT(instanceOwnership.first != OwnershipState{});

    switch (objectProperty.masterOwnership_.second) {
      case OwnershipNotification::ForcedOwnershipAcquisitionNotification:
      case OwnershipNotification::ForcedOwnershipDivestitureNotification: {
        instanceOwnership.first = objectProperty.masterOwnership_.first;
        instanceOwnership.second = OwnershipOperation::None;
        break;
      }
      default: {
        switch (instanceOwnership.second) {
          case OwnershipOperation::None:
            break;
          case OwnershipOperation::ForcedOwnershipAcquisition:
          case OwnershipOperation::ForcedOwnershipDivestiture:
          case OwnershipOperation::Publish:
          case OwnershipOperation::Unpublish:
            // NOTE: masterOwnership_.first will be assigned inside the if-statement below.
            // Meanwhile, the ownershipMap is not guaranteed to be in a valid state until
            // updateOwnershipNotifications has been called.
            objectProperty.masterOwnership_.second = OwnershipNotification::None;
            break;
          default:
            if (objectProperty.masterOwnership_.second != OwnershipNotification::None) {
              instanceOwnership.first = objectProperty.masterOwnership_.first;
              instanceOwnership.second = OwnershipOperation::None;
            }
            break;
        }
      }
    }

    LOG_ASSERT(objectProperty.masterOwnership_.second == OwnershipNotification::None || instanceOwnership.second == OwnershipOperation::None);

    if (instanceOwnership.second != OwnershipOperation::None) {
      beforeUpdateOwnership(ownershipMap);
      objectProperty.masterOwnership_.first = instanceOwnership.first;
      updateOwnershipNotifications(ownershipMap, objectProperty, instanceOwnership.second);
      if (objectProperty.getName() == Property::Destructor_str && !hasPublisher(ownershipMap)) {
        objectInstance->masterInstance_->deleted_ = true;
      }
      afterUpdateOwnership(ownershipMap, objectProperty, instanceOwnership.second, __FILE__, __LINE__);
      ++masterProperty->ownershipVersion_;
      assertValidateOwnership(ownershipMap, __FILE__, __LINE__);
      masterProperty->owner_ = findOwnerFederate(ownershipMap);
      instanceOwnership.second = OwnershipOperation::None;
      masterOwnershipChanged = true;
    }

    auto ownershipNotification = objectProperty.masterOwnership_.second;
    if (ownershipNotification != OwnershipNotification::None) {
      // TODO: invalid state Owned|NotDivesting|NotAskedToRelease for notification OwnershipDivestitureNotification
      LOG_ASSERT(instanceOwnership.first == objectProperty.masterOwnership_.first);
      updateOwnershipState(instanceOwnership.first, ownershipNotification);
      objectProperty.masterOwnership_ = std::make_pair(instanceOwnership.first, OwnershipNotification::None);
      assertValidateOwnership(ownershipMap, __FILE__, __LINE__);
      masterProperty->owner_ = findOwnerFederate(ownershipMap);
      ownershipCallback_(ObjectRef{objectInstance}, objectProperty, ownershipNotification);
    }

  } else if (instanceOwnership.first & OwnershipStateFlag::Owned && masterProperty->owner_) {
    LOG_ASSERT(masterProperty->owner_ != &objectProperty);
    objectProperty.masterOwnership_ = std::make_pair(instanceOwnership.first, OwnershipNotification::ForcedOwnershipDivestitureNotification);
    ownershipMap.push_back(&objectProperty);
    assertValidateOwnership(ownershipMap, __FILE__, __LINE__);
    masterProperty->owner_ = findOwnerFederate(ownershipMap);
    objectProperty.assign(*masterProperty);

  } else if (instanceOwnership.first != OwnershipState{}) {
    objectProperty.masterOwnership_ = std::make_pair(instanceOwnership.first, OwnershipNotification::None);
    ownershipMap.push_back(&objectProperty);
    assertValidateOwnership(ownershipMap, __FILE__, __LINE__);
    masterProperty->owner_ = findOwnerFederate(ownershipMap);

  } else if (masterProperty->owner_) {
    auto ownershipState = objectInstance->objectClass_->getPropertyInfo(objectProperty.getName()).published_
        ? OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire
        : OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::NotAbleToAcquire;
    instanceOwnership = std::make_pair(ownershipState, OwnershipOperation::None);
    objectProperty.masterOwnership_ = std::make_pair(ownershipState, OwnershipNotification::None);
    ownershipMap.push_back(&objectProperty);
    assertValidateOwnership(ownershipMap, __FILE__, __LINE__);
    masterProperty->owner_ = findOwnerFederate(ownershipMap);
  }
  objectProperty.ownershipVersion_ = masterProperty->ownershipVersion_;
  return masterOwnershipChanged;
}


void Federate::unpublishAndRemoveObjectInstanceFromOwnershipMap(ObjectInstance& objectInstance) {
  for (auto& objectProperty : objectInstance.properties_.Values()) {
    if (objectProperty) {
      auto& instanceOwnership = objectProperty->instanceOwnership_;
      instanceOwnership.first = OwnershipState{} + OwnershipStateFlag::Unowned + OwnershipStateFlag::NotAbleToAcquire;
      instanceOwnership.second = OwnershipOperation::None;
      if (auto masterProperty = objectProperty->masterProperty_) {
        auto& ownershipMap = masterProperty->ownershipMap_;
        objectProperty->masterOwnership_.second = OwnershipNotification::None; // force clear any notification
        beforeUpdateOwnership(ownershipMap);
        updateOwnershipNotifications(ownershipMap, *objectProperty, OwnershipOperation::Unpublish);
        objectProperty->masterOwnership_.first = instanceOwnership.first;
        if (objectProperty->getName() == Property::Destructor_str && !hasPublisher(ownershipMap)) {
          objectInstance.masterInstance_->deleted_ = true;
        }
        afterUpdateOwnership(ownershipMap, *objectProperty, OwnershipOperation::Unpublish, __FILE__, __LINE__);
        ++masterProperty->ownershipVersion_;
        ownershipMap.erase(
            std::remove(ownershipMap.begin(), ownershipMap.end(), objectProperty.get()),
            ownershipMap.end());
        assertValidateOwnership(ownershipMap, __FILE__, __LINE__);
        objectProperty->masterProperty_->owner_ = findOwnerFederate(ownershipMap);
      }
    }
  }
}


void Federate::notifyChangesToFederateObservers_strand() {
  LOG_ASSERT(isFederateStrandCurrent());

  for (auto& objectInstance : discoveredInstances_) {
    if (objectCallback_) {
      objectCallback_(ObjectRef{objectInstance});
    }
    for (auto& observer : objectInstance->objectClass_->observers_) {
      observer(ObjectRef{objectInstance});
    }
  }
  for (auto& objectInstance : discoveredInstances_) {
    objectInstance->discoveredAndNotified_ = true;
    objectInstance->discoveredNotNotified_ = false;
  }
  discoveredInstances_.clear();

  for (std::size_t i = 0; i < objectInstances_.size(); ++i) {
    auto& objectInstance = objectInstances_[i];
    if (objectInstance->notify_) {
      if (objectCallback_) {
        objectCallback_(ObjectRef{objectInstance});
      }
      for (auto& observer : objectInstance->objectClass_->observers_) {
        observer(ObjectRef{objectInstance});
      }
    }
  }

  for (auto& objectInstance : objectInstances_) {
    if (objectInstance->notify_) {
      objectInstance->notify_ = false;
      for (auto& p : objectInstance->properties_.Values()) {
        if (p) {
          p->changed_ = false;
        }
      }
    }
  }
}


void Federate::removeDeletedByMaster() {
  auto i = objectInstances_.begin();
  while (i != objectInstances_.end()) {
    auto& objectInstance = *i;
    if (objectInstance->deletedByMaster_) {
      unpublishAndRemoveObjectInstanceFromOwnershipMap(*objectInstance);
      auto& masterInstance = objectInstance->masterInstance_;
      --masterInstance->refCount_;
      objectInstance->masterInstance_ = nullptr;
      i = objectInstances_.erase(i);
    } else {
      ++i;
    }
  }
}


bool Federate::hasObjectInstance_unsafe(MasterInstance& masterInstance) {
  for (auto& objectInstance : objectInstances_) {
    if (objectInstance->masterInstance_ == &masterInstance) {
      return true;
    }
  }
  return false;
}


bool Federate::isWellDefined_unsafe(MasterInstance& masterInstance) {
  auto objectClass = getObjectClass_unsafe(masterInstance.objectClassName_.c_str());
  for (auto& propertyInfo : objectClass->properties_.Values()) {
    if (auto property = masterInstance.properties_.FindValue(propertyInfo.name_.c_str())) {
      auto& value = (*property)->value_;
      if (!isWellDefined_unsafe(masterInstance, value, propertyInfo.required_)) {
        return false;
      }
    } else if (propertyInfo.required_) {
      return false;
    }
  }

  return true;
}


bool Federate::isWellDefined_unsafe(MasterInstance& masterInstance, const Value& value, bool required) {
  switch (value.type()) {
    case ValueType::_undefined:
      return !required;

    case ValueType::_string:
    case ValueType::_binary:
    case ValueType::_boolean:
    case ValueType::_null:
    case ValueType::_int32:
    case ValueType::_double:
      return true;

    case ValueType::_document:
    case ValueType::_array:
      for (auto& v : value) {
        if (!isWellDefined_unsafe(masterInstance, v, true)) {
          return false;
        }
      }
      return true;

    case ValueType::_ObjectId: {
      auto objectId = value._ObjectId();
      for (auto& instance : objectInstances_) {
        if (instance->objectId_ == objectId) {
          return instance->discoveredAndNotified_ || instance->discoveredNotNotified_;
        }
      }
      if (objectId == masterInstance.objectId_) {
        return true;
      }
      // TODO: simultaneously discovery of circular object references
      return false;
    }
  }

  return false;
}


std::function<void(bool)> Federate::makeRequestLogger(std::string name, std::string subjectId) const {
#ifdef WARSTAGE_USE_DAEMON_LOGGER
  using clock_t = std::chrono::steady_clock;
    return [name = std::move(name), subjectId = std::move(subjectId), start = clock_t::now()](bool success) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(clock_t::now() - start).count() / 1.0e6;
        std::cout << (Struct{}
                << "type" << "request"
                << "name" << name
                << "success" << true
                << "duration" << duration
                << "subjectId" << (subjectId.empty() ? nullptr : subjectId.c_str())
                << ValueEnd{}) << '\n';
        std::cout.flush();
    };
#else
  return [](bool) {};
#endif
}
