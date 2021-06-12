// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__FEDERATION_H
#define WARSTAGE__RUNTIME__FEDERATION_H

#include "./object.h"
#include "value/value.h"
#include "async/promise.h"
#include <mutex>

class Federate;
class Runtime;
class ServiceClass;
class Shutdownable;

enum class FederationType {
  None = 0,
  Lobby = 1,
  Battle = 2
};


class Federation {
  friend class EventClass;
  friend class Federate;
  friend class ObjectRef;
  friend class ObjectClass;
  friend class Property;
  friend class Runtime;
  friend class ServiceClass;
  friend class Session;

  Runtime* runtime_;
  ObjectId federationId_;
  FederationType federationType_ = {};
  mutable std::mutex mutex_ = {};
  std::vector<Federate*> federates_ = {};
  std::function<bool(const Federate&, const std::string&)> ownershipPolicy_ = Federation::defaultOwnershipPolicy;
  std::vector<std::unique_ptr<MasterInstance>> masterInstances_ = {};
  std::shared_ptr<Shutdownable> supervisor_ = {};
  int lastInstanceId_ = {};
  int acquireCount_ = {}; // Runtime::_mutex
  Federate* exclusiveOwner_ = nullptr;

public:
  static const ObjectId SystemFederationId;

  Federation(Runtime* runtime, ObjectId federationId);
  Federation(const Federation&) = delete;
  Federation& operator=(const Federation&) = delete;

  ~Federation();

  [[nodiscard]] ObjectId getFederationId() const { return federationId_; }
  [[nodiscard]] FederationType getFederationType() const { return federationType_; }

  void setSupervisor(const std::shared_ptr<Shutdownable>& supervisor) {
    supervisor_ = supervisor;
  }

  Federate* getExclusiveOwner() const {
    std::lock_guard lock{mutex_};
    return exclusiveOwner_;
  }

  void setExclusiveOwner(Federate* federate) {
    std::lock_guard lock{mutex_};
    exclusiveOwner_ = federate;
  }

  void setOwnershipPolicy(std::function<bool(const Federate&, const std::string&)> policy);
  static bool defaultOwnershipPolicy(const Federate&, const std::string&);

private:
  void removeUnreferencedMasterInstances_unsafe();
  void dispatchEvent(Federate& originator, const char* event, const Value& params, double delay, double latency);
  [[nodiscard]] Promise<Value> requestService(const char* service, const Value& params, const std::string& subjectId, Federate* originator);

  [[nodiscard]] Promise<Value> requestService(
      std::string service,
      const Value& params,
      const Value& defaultReason,
      std::string subjectId,
      std::vector<std::weak_ptr<Federate>> federates);

  [[nodiscard]] ServiceClass* findServiceProvider_unsafe(const char* name, Federate* exclude);
  void tryScheduleImmediateSynchronizeOthers_unsafe(Federate* exception);
};


const char* str(FederationType value);


#endif
