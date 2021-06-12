// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./endpoint.h"
#include "./federation.h"
#include "./runtime.h"
#include "./session.h"
#include "./session-federate.h"
#include "./supervision-policy.h"
#include <sstream>


#define LOG_TRACE(format, ...)   LOG_X(format, ##__VA_ARGS__)

static std::atomic_int debugCounter = 0;


const char* str(ProcessType value) {
  switch (value) {
    case ProcessType::None:
      return "-";
    case ProcessType::Agent:
      return "Agent";
    case ProcessType::Headup:
      return "Headup";
    case ProcessType::Player:
      return "Player";
    case ProcessType::Daemon:
      return "Daemon";
    case ProcessType::Module:
      return "Module";
    default:
      return "?";
  }
}


Runtime::Runtime(ProcessType processType, SupervisionPolicy* supervisionPolicy)
    : processType_{processType},
    processId_{ObjectId::create()},
    supervisionPolicy_{supervisionPolicy}
{
  processes_[processId_] = { processId_, processType_ };
  LOG_LIFECYCLE("%p Runtime + %d %s", this, ++debugCounter, str(processType));
}


Runtime::~Runtime() {
  LOG_LIFECYCLE("%p Runtime ~ %d", this, --debugCounter);
  LOG_ASSERT(shutdownCompleted());
  std::lock_guard lock(mutex_);
  LOG_ASSERT(!endpoint_); // must destruct Endpoint before Runtime
  for (auto& federation : federations_) {
    federation->runtime_ = nullptr;
  }
}


Promise<void> Runtime::shutdown_() {
  auto supervisors = std::vector<std::shared_ptr<Shutdownable>>{};
  std::unique_lock lock{mutex_};
  for (auto& federation : federations_) {
    if (federation->supervisor_) {
      supervisors.push_back(federation->supervisor_);
      federation->supervisor_ = nullptr;
    }
  }
  lock.unlock();

  for (auto& supervisor : supervisors) {
    co_await supervisor->shutdown();
  }

  co_return;
}


std::vector<ProcessInfo> Runtime::addRuntimeObserver_safe(RuntimeObserver& observer) {
  std::vector<ProcessInfo> result{};
  std::lock_guard lock{mutex_};
  observers_.push_back(&observer);
  for (const auto& i : federationIdProcessId_) {
    auto federationId = i.first;
    auto processId = i.second;
    auto process = processes_.find(processId);
    if (process != processes_.end()) {
      result.emplace_back(ProcessInfo{process->second.type, process->second.id, federationId});
    }
  }
  return result;
}


void Runtime::removeRuntimeObserver_safe(RuntimeObserver& observer) {
  std::lock_guard lock{mutex_};
  observers_.erase(
      std::remove(observers_.begin(), observers_.end(), &observer),
      observers_.end());
}

bool Runtime::hasRuntimeObserver_safe(const RuntimeObserver& observer) const {
  std::lock_guard lock{mutex_};
  auto i = std::find(observers_.begin(), observers_.end(), &observer);
  return i != observers_.end();
}

ProcessType Runtime::getProcessType_safe(ObjectId processId) const {
  std::lock_guard lock{mutex_};
  auto i = processes_.find(processId);
  return i != processes_.end() ? i->second.type : ProcessType::None;
}


ProcessAuth Runtime::getProcessAuth_safe(ObjectId processId) const {
  std::lock_guard lock{mutex_};
  auto i = processes_.find(processId);
  return i != processes_.end() ? i->second.auth : ProcessAuth{};
}


ProcessAddr Runtime::getProcessAddr_safe() const {
  std::lock_guard lock{mutex_};
  auto i = processes_.find(processId_);
  return i != processes_.end() ? i->second.addr : ProcessAddr{};
}


std::string Runtime::getSubjectId_safe() const {
  std::lock_guard lock{mutex_};
  auto i = processes_.find(processId_);
  return i != processes_.end() ? i->second.auth.subjectId : std::string{};
}


Session* Runtime::getProcessSession(ObjectId processId) const {
  auto i = processes_.find(processId);
  return i != processes_.end() ? i->second.session : nullptr;
}


Session* Runtime::getProcessSession_safe(ObjectId processId) const {
  std::lock_guard lock{mutex_};
  return getProcessSession(processId);
}


bool Runtime::registerProcess_safe(ObjectId processId, ProcessType processType, Session* session) {
  std::lock_guard lock{mutex_};
  auto i = processes_.find(processId);
  if (i == processes_.end()) {
    if (processType == ProcessType::None) {
      LOG_E("Runtime::RegisterProcess, missing type");
      return false;
    }
    processes_[processId] = {processId, processType, session};
    return true;
  }
  if (processType != ProcessType::None) {
    if (processType != i->second.type) {
      LOG_E("Runtime::RegisterProcess, mismatching type");
      return false;
    }
    i->second.type = processType;
  }
  if (session != nullptr) {
    if (i->second.session != nullptr && session != i->second.session) {
      LOG_E("Runtime::RegisterProcess, mismatching session");
      return false;
    }
    i->second.session = session;
  }
  return true;
}


void Runtime::registerProcessAuth_safe(ObjectId processId, const ProcessAuth& processAuth) {
  std::unique_lock lock{mutex_};
  auto i = processes_.find(processId);
  if (i == processes_.end()) {
    LOG_E("Runtime::RegisterProcess, invalid process");
    return;
  }
  i->second.auth = processAuth;
  lock.unlock();
  notifyProcessAuth_safe(processId, processAuth);
}


void Runtime::notifyProcessAuth_safe(ObjectId processId, const ProcessAuth& processAuth) {
  std::lock_guard lock{mutex_};
  for (auto observer : observers_) {
    PromiseUtils::Strand()->setImmediate([this_weak = weak_from_this(), observer, processId, processAuth]() {
      if (auto this_ = this_weak.lock(); this_ && this_->hasRuntimeObserver_safe(*observer)) {
        observer->onProcessAuthenticated_main(processId, processAuth);
      }
    });
  }
}


void Runtime::registerProcessAddr_safe(ObjectId processId, const char* host, const char* port) {
  LOG_ASSERT(host);
  LOG_ASSERT(port);
  std::lock_guard lock{mutex_};
  auto i = processes_.find(processId);
  if (i == processes_.end()) {
    LOG_E("Runtime::RegisterProcess, invalid process");
    return;
  }
  i->second.addr.host = host;
  i->second.addr.port = port;
}


void Runtime::unregisterProcessSession_safe(ObjectId processId) {
  std::lock_guard lock{mutex_};
  auto i = processes_.find(processId);
  if (i == processes_.end()) {
    LOG_E("Runtime::RegisterProcess, invalid process");
    return;
  }
  i->second.session = nullptr;
}


void Runtime::unregisterProcess_safe(ObjectId processId) {
  LOG_TRACE("%s[%s] Runtime::DeleteProcess([%s])",
      str(processType_),
      processId_.debug_str().c_str(),
      processId.debug_str().c_str());

  LOG_ASSERT(!isProcessActive_safe(processId));

  std::lock_guard lock{mutex_};
  processes_.erase(processId);
}


bool Runtime::isProcessActive_safe(ObjectId processId) const {
  if (processId == processId_) {
    return true;
  }
  std::lock_guard lock{mutex_};
  auto i = processes_.find(processId);
  if (i != processes_.end() && i->second.session) {
    return true;
  }

  for (auto& j : federationIdProcessId_) {
    if (j.second == processId) {
      return true;
    }
  }

  return false;
}


void Runtime::federationProcessAdded_safe(ObjectId federationId, ObjectId processId) {
  std::unique_lock lock{mutex_};

  auto process = [this, processId] {
    auto i = processes_.find(processId);
    return i != processes_.end() ? &i->second : nullptr;
  }();
  if (!process) {
    return;
  }

  LOG_TRACE("%s[%s] Runtime::FederationProcessAdded({%s}, %s[%s] %s:%s)",
      str(processType_),
      processId_.debug_str().c_str(),
      federationId.debug_str().c_str(),
      str(process->type),
      process->id.debug_str().c_str(),
      process->addr.host.c_str(),
      process->addr.port.c_str());

  auto federationProcess = std::make_pair(federationId, process->id);
  if (federationIdProcessId_.find(federationProcess) != federationIdProcessId_.end()) {
    return; // already added
  }
  federationIdProcessId_.insert(federationProcess);

  auto i = std::find_if(federations_.begin(), federations_.end(), [federationId](auto& x) {
    return x->getFederationId() == federationId;
  });
  auto federation = i != federations_.end() ? i->get() : nullptr;
  if (federation) {
    for (auto observer : observers_) {
      PromiseUtils::Strand()->setImmediate([this_weak = weak_from_this(), observer, federationId, processId, processType = process->type]() {
        if (auto this_ = this_weak.lock(); this_ && this_->hasRuntimeObserver_safe(*observer)) {
          observer->onProcessAdded_main(federationId, processId, processType);
        }
      });
    }
  }

  lock.unlock();

  if (federation) {
    if (processId != processId_ && process->type == ProcessType::Daemon && !getProcessSession_safe(processId)) {
      std::ostringstream os{};
      os << "ws://" << process->addr.host << ":" << process->addr.port;
      lock.lock();
      endpoint_->makeSession_safe(os.str()).get();
      lock.unlock();
    }

    joinSessionsToFederation_safe(federationId);
  }
}


void Runtime::federationProcessRemoved_safe(ObjectId federationId, ObjectId processId) {
  LOG_TRACE("%s[%s] Runtime::FederationProcessRemoved({%s}, [%s])",
      str(processType_),
      processId_.debug_str().c_str(),
      federationId.debug_str().c_str(),
      processId.debug_str().c_str());

  auto federationProcess = std::make_pair(federationId, processId);
  std::unique_lock lock{mutex_};
  if (federationIdProcessId_.find(federationProcess) == federationIdProcessId_.end()) {
    return; // already removed
  }
  federationIdProcessId_.erase(std::make_pair(federationId, processId));
  lock.unlock();

  if (auto session = getProcessSession_safe(processId)) {
    if (auto federate = session->getSessionFederate_safe(federationId)) {
      if (!federate->shutdownStarted()) {
        session->leaveFederation(federationId);
      }
    }
  }

  notifyFederationProcessRemoved_safe_(federationId, processId);

  if (!isProcessActive_safe(processId)) {
    unregisterProcess_safe(processId);
  }
}

void Runtime::notifyFederationProcessRemoved_safe_(ObjectId federationId, ObjectId processId) {
  std::unique_lock lock{mutex_};
  auto i = std::find_if(federations_.begin(), federations_.end(), [federationId](auto& x) {
    return x->getFederationId() == federationId;
  });
  auto federation = i != federations_.end() ? i->get() : nullptr;
  if (federation) {
    for (auto* observer : observers_) {
      PromiseUtils::Strand()->setImmediate([this_weak = weak_from_this(), observer, federationId, processId]() {
        if (auto this_ = this_weak.lock(); this_ && this_->hasRuntimeObserver_safe(*observer)) {
          observer->onProcessRemoved_main(federationId, processId);
        }
      });
    }
  }
}

void Runtime::joinSessionsToFederation_safe(ObjectId federationId) {
  std::vector<std::shared_ptr<Session>> sessions{};
  std::unique_lock lock{mutex_};
  auto i = std::lower_bound(federationIdProcessId_.begin(), federationIdProcessId_.end(), std::make_pair(federationId, ObjectId{}));
  while (i != federationIdProcessId_.end() && i->first == federationId) {
    const auto& process = [this, processId = i->second] {
      auto i = processes_.find(processId);
      return i != processes_.end() ? &i->second : nullptr;
    }();
    if (process) {
      if (auto session = getProcessSession(process->id)) {
        sessions.push_back(session->shared_from_this());
      }
    }
    ++i;
  }
  lock.unlock();

  for (auto& session : sessions) {
    if (!session->getSessionFederate_safe(federationId)) {
      session->joinFederation_safe(federationId);
    }
  }
}


std::vector<ObjectId> Runtime::getProcessFederations_safe(ObjectId processId) {
  std::vector<ObjectId> result{};
  std::lock_guard lock{mutex_};
  for (const auto& i : federationIdProcessId_) {
    if (i.second == processId) {
      result.push_back(i.first);
    }
  }
  return result;
}


FederationType Runtime::getFederationType_safe(ObjectId federationId) const {
  std::lock_guard lock{mutex_};
  for (auto& federation : federations_) {
    if (federation->getFederationId() == federationId) {
      return federation->getFederationType();
    }
  }
  return FederationType::None;
}


void Runtime::requestHostMatch_safe(ObjectId lobbyId, ObjectId matchId) {
  std::lock_guard lock{mutex_};
  if (endpoint_) {
    endpoint_->requestHostMatch_safe(lobbyId, matchId);
  }
}


void Runtime::processHostMatch_safe(ObjectId lobbyId, ObjectId matchId, const std::string& subjectId) {
  if (authorizeCreateBattleFederation_safe(subjectId)) {
    hostFederation_safe(FederationType::Lobby, lobbyId);
    hostFederation_safe(FederationType::Battle, matchId);
  }
}


void Runtime::hostFederation_safe(FederationType federationType, ObjectId federationId) {
  auto federation = initiateFederation_safe(federationId, federationType);
  if (supervisionPolicy_) {
    bool makeSupervisor = false;
    std::unique_lock lock{federation->mutex_};
    makeSupervisor = !federation->supervisor_;

    if (makeSupervisor) {
      lock.unlock();
      auto supervisor = supervisionPolicy_->makeSupervisor(*this, federationType, federationId);
      lock.lock();
      if (supervisor) {
        federation->supervisor_ = std::move(supervisor);
      }
    }
  }
}


bool Runtime::authorizeCreateBattleFederation_safe(std::string const &subjectId) const {
  return processType_ == ProcessType::Player || !subjectId.empty();
}


Federation* Runtime::initiateFederation_safe(ObjectId federationId, FederationType federationType) {
  auto federation = acquireFederation_safe(federationId, true);
  if (federation->getFederationType() != FederationType::None) {
    LOG_ASSERT(federation->getFederationType() == federationType);
  } else {
    federation->federationType_ = federationType;

    LOG_TRACE("%s[%s] Runtime::InitiateFederation({%s}, %s)",
        str(processType_),
        processId_.debug_str().c_str(),
        federationId.debug_str().c_str(),
        str(federationType));

    federationProcessAdded_safe(federationId, processId_);
  }
  return federation;
}


Federation* Runtime::acquireFederation_safe(ObjectId federationId, bool createIfNotExists) {
  LOG_TRACE("%s[%s] Runtime::AcquireFederation({%s})",
      str(processType_),
      processId_.debug_str().c_str(),
      federationId.debug_str().c_str());
  Federation* result = nullptr;
  bool added = false;
  std::unique_lock lock{mutex_};
  for (auto& federation : federations_) {
    if (federation->getFederationId() == federationId) {
      ++federation->acquireCount_;
      return federation.get();
    }
  }
  if (createIfNotExists) {
    LOG_TRACE("%s[%s] Runtime::CreateFederation({%s})",
        str(processType_),
        processId_.debug_str().c_str(),
        federationId.debug_str().c_str());

    auto federation = std::make_unique<Federation>(this, federationId);
    ++federation->acquireCount_;
    result = federation.get();
    federations_.push_back(std::move(federation));
    added = true;
  }
  lock.unlock();

  if (added) {
    if (endpoint_ && federationId) {
      endpoint_->broadcastFederationProcessAdded_safe(federationId, processId_, processType_, getProcessAddr_safe(), nullptr);
    }
  }
  if (result) {
    joinSessionsToFederation_safe(federationId);
  }

  return result;
}


void Runtime::releaseFederation_safe(Federation* federation) {
  auto federationId = federation->getFederationId();
  LOG_TRACE("%s[%s] Runtime::ReleaseFederation({%s})",
      str(processType_),
      processId_.debug_str().c_str(),
      federationId.debug_str().c_str());

  bool deleted = false;
  std::unique_lock lock{mutex_};
  if (--federation->acquireCount_ == 0) {
    LOG_TRACE("%s[%s] Runtime::DeleteFederation({%s})",
        str(processType_),
        processId_.debug_str().c_str(),
        federationId.debug_str().c_str());

    auto i = std::find_if(federations_.begin(), federations_.end(), [federation](auto& x) {
      return x.get() == federation;
    });
    if (i != federations_.end()) {
      federations_.erase(i);
    } else {
      assert(i != federations_.end());
    }

    if (federationId && endpoint_) {
      endpoint_->broadcastFederationProcessRemoved_safe(federationId, processId_);
    }
    deleted = true;
  }
  lock.unlock();

  if (deleted && federationId) {
    federationProcessRemoved_safe(federationId, processId_);
  }
}
