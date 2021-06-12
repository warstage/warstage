// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./endpoint.h"
#include "./federation.h"
#include "./runtime.h"
#include "./session.h"
#include <random>

static std::atomic_int debugCounter = 0;


Endpoint::Endpoint(Runtime& runtime) : runtime_{&runtime} {
  LOG_LIFECYCLE("%p Endpoint + %d", this, ++debugCounter);
  std::lock_guard lock{runtime_->mutex_};
  LOG_ASSERT(!runtime_->endpoint_);
  runtime_->endpoint_ = this;
}


Endpoint::~Endpoint() {
  LOG_LIFECYCLE("%p Endpoint ~ %d", this, --debugCounter);
  LOG_ASSERT(shutdownCompleted());
  std::lock_guard lock{mutex_};
  LOG_ASSERT(sessions_.empty());
}


Promise<void> Endpoint::shutdown_() {
  LOG_LIFECYCLE("%p Endpoint ShutdownInternal_safe", this);

  std::unique_lock lock{runtime_->mutex_};
  LOG_ASSERT(runtime_->endpoint_ == this);
  runtime_->endpoint_ = nullptr;
  lock.unlock();

  std::vector<std::shared_ptr<Session>> sessions;
  for (auto& session : sessions_)
    sessions.push_back(session->shared_from_this());
  sessions_.clear();

  for (auto& session : sessions) {
    LOG_LIFECYCLE("%p Endpoint ShutdownInternal_safe session %p queue", this, session.get());
    co_await session->shutdown();
    session.reset();
  }
  LOG_LIFECYCLE("%p Endpoint ShutdownInternal_safe done", this);
  LOG_ASSERT(sessions_.empty());
}


void Endpoint::setMasterUrl_safe(std::string value) {
  std::lock_guard lock{mutex_};
  serverUrl_ = std::move(value);
  tryConnectMaster_mutex();
}


void Endpoint::requestHostMatch_safe(ObjectId lobbyId, ObjectId matchId) {
  std::shared_ptr<Session> session;
  std::unique_lock lock{mutex_};
  session = masterSession_.lock();
  lock.unlock();

  if (session) {
    session->strand_->setImmediate([session, lobbyId, matchId]() {
      session->sendHostRequest_strand(lobbyId, matchId);
    });
  }
}


void Endpoint::onSessionClosed_safe(Session& session) {
  if (sessionClosedHandler_) {
    sessionClosedHandler_(session);
  }

  std::lock_guard lock{mutex_};

  if (!masterSession_.expired()) {
    if (auto masterSession = masterSession_.lock()) {
      if (&session == masterSession.get()) {
        masterSession_.reset();
      }
    }
  }

  tryConnectMaster_mutex();
}


void Endpoint::broadcastFederationProcessAdded_safe(ObjectId federationId, ObjectId processId, ProcessType processType, const ProcessAddr& processAddr, Session* origin) {
  assert(federationId);
  auto packet = Struct{}
      << "m" << static_cast<int>(Session::Packet::FederationProcessAdded)
      << "x" << federationId.str()
      << "id" << processId.str()
      << "type" << static_cast<int>(processType)
      << "host" << processAddr.host
      << "port" << processAddr.port
      << ValueEnd{};

  std::lock_guard lock{mutex_};
  for (auto session : sessions_) {
    if (session->getProcessType() != ProcessType::None) {
      if (!origin || shouldRelayFederationProcessAdded(*origin, *session)) {
        if (processId != session->getProcessId()) {
          session->getStrand().setImmediate([session = session->shared_from_this(), packet]() {
            session->sendPacket_strand(packet);
          });
        }
      }
    }
  }
}


void Endpoint::broadcastFederationProcessRemoved_safe(ObjectId federationId, ObjectId processId) {
  auto packet = Struct{}
      << "m" << static_cast<int>(Session::Packet::FederationProcessRemoved)
      << "x" << federationId.str()
      << "id" << processId.str()
      << ValueEnd{};

  std::lock_guard lock{mutex_};
  for (auto session : sessions_) {
    if (session->getProcessType() != ProcessType::None) {
      session->getStrand().setImmediate([session = session->shared_from_this(), packet]() {
        session->sendPacket_strand(packet);
      });
    }
  }
}


bool Endpoint::shouldRelayFederationProcessAdded(const Session& origin, const Session& target) {
  if (&origin == &target)
    return false;
  auto p = origin.getProcessType();
  auto t = target.getProcessType();
  if (p != ProcessType::None && t != ProcessType::None) {
    bool m1 = p == ProcessType::Daemon;
    bool m2 = t == ProcessType::Daemon;
    return m1 != m2;
  }
  return false;
}


void Endpoint::addSession_safe(Session* session) {
  LOG_ASSERT(!shutdownStarted());
  std::lock_guard lock{mutex_};
  sessions_.push_back(session);
}


void Endpoint::removeSession_safe(Session* session) {
  std::lock_guard lock{mutex_};
  sessions_.erase(
      std::remove(sessions_.begin(), sessions_.end(), session),
      sessions_.end());
}


void Endpoint::tryConnectMaster_mutex() {
  if (shutdownStarted()) {
    return;
  }

  if (masterSession_.expired() && !serverUrl_.empty() && !masterConnectObject_) {
    masterConnectObject_ = PromiseUtils::Strand()->setTimeout([weak_ = weak_from_this()]() {
      if (auto this_ = weak_.lock()) {
        if (!this_->shutdownStarted()) {
          this_->masterSession_ = this_->makeSession_safe(this_->serverUrl_);
          this_->masterConnectObject_ = nullptr;
          this_->masterConnectDelay_ = 500 + 2 * this_->masterConnectDelay_;
          if (this_->masterConnectDelay_ > 4000) {
            this_->masterConnectDelay_ = 4000;
          }
        }
      }
    }, masterConnectDelay_);
  }
}
