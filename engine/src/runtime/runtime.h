// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__RUNTIME_H
#define WARSTAGE__RUNTIME__RUNTIME_H

#include "./event-class.h"
#include "./federate.h"
#include "./federation.h"
#include "./object.h"
#include "./object-class.h"
#include "./service-class.h"
#include "async/promise.h"
#include "async/strand.h"
#include "async/shutdownable.h"
#include "value/value.h"
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <set>


enum class ProcessType {
  None = 0,
  Agent = 1,
  Headup = 2,
  Player = 3,
  Module = 6,
  Daemon = 5
};

inline bool isLocalProcessType(ProcessType value) {
  return value == ProcessType::Headup || value == ProcessType::Module;
}

const char* str(ProcessType value);

struct ProcessAddr {
  std::string host{};
  std::string port{};
};

struct ProcessAuth {
  std::string subjectId{};
  std::string nickname{};
  std::string imageUrl{};
  std::string accessToken{};
};

class ProcessInfo {
public:
  ProcessType processType;
  ObjectId processId;
  ObjectId federationId;
};


enum class ObjectChange {
  None = 0,
  Discover = 1,
  Update = 2,
  Delete = 3,
};

class Analytics;
class Endpoint;
class SupervisionPolicy;

class RuntimeObserver {
  friend class Runtime;
public:
  virtual ~RuntimeObserver() = default;
protected:
  virtual void onProcessAdded_main(ObjectId federationId, ObjectId processId, ProcessType processType) {}
  virtual void onProcessRemoved_main(ObjectId federationId, ObjectId processId) {}
  virtual void onProcessAuthenticated_main(ObjectId processId, const ProcessAuth& processAuth) = 0;
};

class Runtime :
    public Shutdownable,
    public std::enable_shared_from_this<Runtime>
{
  friend class Endpoint;
  friend class Federate;
  friend class Federation;
  friend class Session;

  struct Process {
    ObjectId id = {};
    ProcessType type = {};
    Session* session = nullptr;
    ProcessAddr addr = {};
    ProcessAuth auth = {};
  };

  const ProcessType processType_;
  const ObjectId processId_;
  mutable std::mutex mutex_{};
  SupervisionPolicy* supervisionPolicy_;
  Endpoint* endpoint_{};
  // Analytics* analytics_{};
  std::vector<std::unique_ptr<Federation>> federations_{}; // mutex
  std::unordered_map<ObjectId, Process> processes_{}; // mutex
  std::set<std::pair<ObjectId, ObjectId>> federationIdProcessId_{}; // mutex
  std::vector<RuntimeObserver*> observers_{}; // mutex

public:
  explicit Runtime(ProcessType processType, SupervisionPolicy* supervisionPolicy = nullptr);
  ~Runtime() override;

  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

public:
  std::vector<ProcessInfo> addRuntimeObserver_safe(RuntimeObserver& observer);
  void removeRuntimeObserver_safe(RuntimeObserver& observer);
  bool hasRuntimeObserver_safe(const RuntimeObserver& observer) const;

  // [[nodiscard]] Analytics* getAnalytics() const { return analytics_; }
  // void setAnalytics(Analytics* value) { analytics_ = value; }

  [[nodiscard]] ObjectId getProcessId() const { return processId_; }
  [[nodiscard]] ProcessType getProcessType() const { return processType_; }
  [[nodiscard]] ProcessType getProcessType_safe(ObjectId processId) const;
  [[nodiscard]] ProcessAuth getProcessAuth_safe(ObjectId processId) const;
  [[nodiscard]] ProcessAuth getProcessAuth_safe() const { return getProcessAuth_safe(processId_); }
  [[nodiscard]] ProcessAddr getProcessAddr_safe() const;
  [[nodiscard]] std::string getSubjectId_safe() const;
  [[nodiscard]] Session* getProcessSession(ObjectId processId) const;
  [[nodiscard]] Session* getProcessSession_safe(ObjectId processId) const;
  [[nodiscard]] bool isProcessActive_safe(ObjectId processId) const;

  bool registerProcess_safe(ObjectId processId, ProcessType processType, Session* session);
  void registerProcessAuth_safe(ObjectId processId, const ProcessAuth& processAuth);
  void notifyProcessAuth_safe(ObjectId processId, const ProcessAuth& processAuth);
  void registerProcessAddr_safe(ObjectId processId, const char* host, const char* port);
  void unregisterProcessSession_safe(ObjectId processId);
  void unregisterProcess_safe(ObjectId processId);

  void federationProcessAdded_safe(ObjectId federationId, ObjectId processId);
  void federationProcessRemoved_safe(ObjectId federationId, ObjectId processId);
  void notifyFederationProcessRemoved_safe_(ObjectId federationId, ObjectId processId);

  void joinSessionsToFederation_safe(ObjectId federationId);

  [[nodiscard]] std::vector<ObjectId> getProcessFederations_safe(ObjectId processId);

  FederationType getFederationType_safe(ObjectId federationId) const;

  void requestHostMatch_safe(ObjectId lobbyId, ObjectId matchId);
  void processHostMatch_safe(ObjectId lobbyId, ObjectId matchId, const std::string& subjectId);
  void hostFederation_safe(FederationType federationType, ObjectId federationId);
  bool authorizeCreateBattleFederation_safe(const std::string& subjectId) const;

  Federation* initiateFederation_safe(ObjectId federationId, FederationType federationType);

  Federation* acquireFederation_safe(ObjectId federationId, bool createIfNotExists = false);
  void releaseFederation_safe(Federation* federation);
};


#endif
