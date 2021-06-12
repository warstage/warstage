// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__FEDERATE_H
#define WARSTAGE__RUNTIME__FEDERATE_H

#include "./event-class.h"
#include "./object.h"
#include "./object-class.h"
#include "./ownership.h"
#include "./service-class.h"
#include "value/value.h"
#include "async/promise.h"
#include "async/shutdownable.h"
#include "async/strand.h"
#include <memory>
#include <mutex>

class Federation;
class Runtime;


class Federate :
    public Shutdownable,
    public std::enable_shared_from_this<Federate>
{
  friend class EventClass;
  friend class Federation;
  friend struct MasterProperty;
  friend class ObjectRef;
  friend class ObjectClass;
  friend struct ObjectInstance;
  friend class ObjectIterator;
  friend class Property;
  friend class ServiceClass;
  friend class Runtime;
  friend class Session;
  friend class SessionFederate;

  // read-only
  Runtime* const runtime_{};
  std::mutex mutex_{};
  const std::string federateName_;
  const std::shared_ptr<Strand_base> strand_{};
  const std::chrono::steady_clock::time_point baseTimePoint_ = std::chrono::steady_clock::now();

  // federate strand only
  std::vector<std::shared_ptr<ObjectInstance>> discoveredInstances_{};
  std::vector<MasterInstance*> undiscoveredInstances_{};
  int lastInstanceId_{};
  double eventDelay_{};
  double eventLatency_{};
  double currentTime_{};

  // multiple threads
  ObjectId federationId_{};
  Federation* federation_{}; // federationMutex
  std::mutex federationMutex_{};
  std::mutex startupShutdownMutex_{};
  std::vector<std::unique_ptr<EventClass>> eventClasses_{};
  std::vector<std::unique_ptr<ServiceClass>> serviceClasses_{};
  std::vector<std::unique_ptr<ObjectClass>> objectClasses_{};
  std::vector<std::shared_ptr<ObjectInstance>> objectInstances_{};
  std::function<void(ObjectRef)> objectCallback_{};
  std::function<void(const char*, const Value&)> eventCallback_{};
  std::function<Promise<Value>(const char*, const Value&, const std::string&)> serviceCallback_{};
  std::function<void(ObjectRef object, Property& property, OwnershipNotification notification)> ownershipCallback_{Federate::defaultOwnershipCallback};
  std::shared_ptr<ImmediateObject> immediateSynchronize_{};
  int blockCounter_{};
  bool deferredSynchronize_{};

public: // protected:
  Federation* getFederation() const { return federation_; }

public:
  Federate(Runtime& runtime, const char* federateName, std::shared_ptr<Strand_base> strand);
  virtual ~Federate() override;

  virtual void startup(ObjectId federationId);

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

public:
  bool isPrincipalFederate() const;
  bool ownershipPolicy(const std::string& propertyName) const;

  [[nodiscard]] ObjectId getFederationId() const;
  [[nodiscard]] const std::string& getFederateName() const { return federateName_; }
  [[nodiscard]] std::string getDescription() const;

  [[nodiscard]] Runtime& getRuntime() const { return *runtime_; }

  [[nodiscard]] ObjectRef getObject(ObjectId objectId) const; // AssertFederateStrand
  [[nodiscard]] ObjectClass& getObjectClass(const char* name);
  [[nodiscard]] ObjectClass* getObjectClass_unsafe(const char* name);
  void setObjectCallback(std::function<void(ObjectRef)> callback);

  [[nodiscard]] EventClass& getEventClass(const char* name);
  void setEventCallback(std::function<void(const char*, const Value&)> callback);
  void dispatchEvent(Federate& originator, const char* event, const Value& params, double delay, double latency);

  [[nodiscard]] ServiceClass& getServiceClass(const char* name);
  void setServiceCallback(std::function<Promise<Value>(const char*, const Value&, const std::string&)> callback);
  [[nodiscard]] static std::function<Promise<Value>(const char*, const Value&, const std::string&)> tryGetServiceCallback(const std::weak_ptr<Federate>& federate);
  [[nodiscard]] Promise<Value> requestService(const char* service, const Value& params, const std::string& subjectId, Federate* originator);

  void setOwnershipCallback(std::function<void(ObjectRef object, Property& property, OwnershipNotification notification)> callback);
  static void defaultOwnershipCallback(ObjectRef object, Property& property, OwnershipNotification notification);

  void synchronize_strand();

  virtual void enterBlock_strand();
  virtual void leaveBlock_strand();

  void updateCurrentTime_strand();

  [[nodiscard]] double getEventDelay() const { LOG_ASSERT(isFederateStrandCurrent()); return eventDelay_; }
  [[nodiscard]] double getEventLatency() const { LOG_ASSERT(isFederateStrandCurrent()); return eventLatency_; }

private:
  [[nodiscard]] bool isFederateStrandCurrent() const {
    return strand_->isCurrent();
  }

  void setFederation_safe(Federation* federation);
  [[nodiscard]] Federation* clearFederation_safe();

  void postAsyncTask(std::function<void()> task);

  void clearImmediateSyncrhonize_safe();
  void tryScheduleImmediateSynchronize_unsafe();

  [[nodiscard]] bool synchronizeChangesFromFederateToFederation_strand(Federation* federation);
  [[nodiscard]] bool synchronizeChangesFromFederationToFederate_strand(Federation* federation);

  [[nodiscard]] static bool shouldUpdateOwnership(const Property& property);
  bool updateOwnership(const std::shared_ptr<ObjectInstance>& objectInstance, Property& objectProperty);

  void unpublishAndRemoveObjectInstanceFromOwnershipMap(ObjectInstance& objectInstance);

  void notifyChangesToFederateObservers_strand();
  void removeDeletedByMaster();

  [[nodiscard]] bool hasObjectInstance_unsafe(MasterInstance& masterInstance);
  [[nodiscard]] bool isWellDefined_unsafe(MasterInstance& masterInstance);
  [[nodiscard]] bool isWellDefined_unsafe(MasterInstance& masterInstance, const Value& value, bool required);

  std::function<void(bool)> makeRequestLogger(std::string name, std::string subjectId) const;
};


#endif
