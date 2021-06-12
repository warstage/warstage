// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./endpoint.h"
#include "./runtime.h"
#include "./session.h"
#include "./session-federate.h"
#include "utilities/logging.h"


#define LOG_TRACE(format, ...)   LOG_X(format, ##__VA_ARGS__)
#define LOG_ROUTING(format, ...)   LOG_X(format, ##__VA_ARGS__)

static std::atomic_int debugCounter = 0;


LatencyHeader LatencyTracker::generateHeader() {
  auto generatedId = ++lastGeneratedId_;
  auto now = clock::now();
  generated_.emplace_back(generatedId, now);
  return { generatedId, lastReceivedId_, durationToIdleTime(now - lastReceivedTime_) };
}


void LatencyTracker::receiveHeader(const LatencyHeader& header) {
  auto i = std::find_if(generated_.begin(), generated_.end(), [header](const auto& x) {
    return x.first == header.receivedId;
  });
  lastReceivedId_ = header.generatedId;
  lastReceivedTime_ = clock::now();
  if (i != generated_.end()) {
    auto rtt = lastReceivedTime_ - i->second - idleTimeToDuration(header.idleTime);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(rtt).count();
    auto latency = 0.0000005 * microseconds; // latency is half the round-trip time
    latency_ = 0.7 * latency_ + 0.3 * latency;
    generated_.erase(generated_.begin(), i + 1);
  }
}


std::uint16_t LatencyTracker::durationToIdleTime(clock::duration value) {
  double d = std::chrono::duration_cast<std::chrono::microseconds>(value).count() / 100.0;
  return d > 65535 ? 65535 : static_cast<std::uint16_t>(d);
}


LatencyTracker::clock::duration LatencyTracker::idleTimeToDuration(std::uint16_t value) {
  return std::chrono::microseconds{value * 100};
}


/***/


Session::Session(Endpoint& endpoint, std::shared_ptr<Strand_base> strand) :
    runtime_{endpoint.runtime_},
    endpoint_{&endpoint},
    strand_{std::move(strand)}
{
  LOG_LIFECYCLE("%p Session + %d", this, ++debugCounter);
  endpoint_->addSession_safe(this);
  runtime_->addRuntimeObserver_safe(*this);
}


Session::~Session() {
  LOG_LIFECYCLE("%p Session ~ %d", this, --debugCounter);
  LOG_ASSERT(shutdownCompleted());
  LOG_ASSERT(!endpoint_);
  std::lock_guard lock{mutex_};
  for (auto& i : federates_) {
    LOG_ASSERT(!i.second || i.second->shutdownCompleted());
  }
}


Promise<void> Session::shutdown_() {
  LOG_LIFECYCLE("%p Session Shutdown", this);

  co_await *strand_;

  runtime_->removeRuntimeObserver_safe(*this);

  stopHeartbeatInterval_strand();

  std::unordered_map<ObjectId, std::shared_ptr<Federate>> federates{};
  std::unique_lock lock{mutex_};
  federates = federates_;
  lock.unlock();

  for (auto& i : federates) {
    auto federate = i.second;
    if (federate) {
      // auto federationId = federate->getFederationId();
      LOG_LIFECYCLE("%p Session Shutdown A", this);
      federate->setObjectCallback(nullptr);
      federate->setEventCallback(nullptr);
      federate->setServiceCallback(nullptr);
      co_await federate->shutdown();
    }
  }

  LOG_LIFECYCLE("%p Session Shutdown B", this);
  lock.lock();
  endpoint_->removeSession_safe(this);
  endpoint_ = nullptr;
  if (processType_ != ProcessType::None) {
    if (runtime_->isProcessActive_safe(processId_)) {
      runtime_->unregisterProcessSession_safe(processId_);
    } else {
      runtime_->unregisterProcess_safe(processId_);
    }
  }

  for (auto& i : federates_) {
    LOG_ASSERT(!i.second || i.second->shutdownCompleted());
  }
}


SessionFederate* Session::getSessionFederate_safe(ObjectId federationId) {
  std::lock_guard lock{mutex_};
  auto i = federates_.find(federationId);
  return i != federates_.end() ? dynamic_cast<SessionFederate*>(i->second.get()) : nullptr;
}


/***/


void Session::receivePacket_strand(const Value& packet) {
  receiveTimestamp_ = std::chrono::system_clock::now();

  bool emptyMessageQueue = false;
  std::unique_lock lock{mutex_};
  if (!connected_) {
    connected_ = true;
    emptyMessageQueue = true;
  }
  lock.unlock();

  if (emptyMessageQueue) {
    emptyOutgoingPacketQueue_strand();
  }

  if (packet["t"].has_value()) {
    LatencyHeader header{};
    header.generatedId = static_cast<std::uint16_t>(packet["i"_int]);
    header.receivedId = static_cast<std::uint16_t>(packet["r"_int]);
    header.idleTime = static_cast<std::uint16_t>(packet["t"_int]);
    latencyTracker_.receiveHeader(header);
  }

  auto payload = packet["p"];
  int packetType = payload["m"_int];

  // if (processType_ == ProcessType::Daemon && runtime_->getProcessType() == ProcessType::Player) {
  //   if (auto analytics = runtime_->getAnalytics()) {
  //     analytics->samplePacketIncoming(packet.size(), latencyTracker_.getLatency());
  //     analytics->samplePacket(packetType);
  //   }
  // }

  switch (static_cast<Packet>(packetType)) {
    case Packet::Handshake:
      return onIncomingHandshake_strand(payload);
    case Packet::Authenticate:
      return onIncomingAuthenticate_strand(payload);
    case Packet::Messages:
      return onIncomingMessages_strand(payload);
    case Packet::FederationProcessAdded:
      return onIncomingFederationProcessAdded_strand(payload);
    case Packet::FederationProcessRemoved:
      return onIncomingFederationProcessRemoved_strand(payload);
    case Packet::FederationHostingRequest:
      return onIncomingFederationHostingRequest_strand(payload);
    default:
      return;
  }
}


void Session::sendPacket_strand(const Value& packet) {
  auto header = latencyTracker_.generateHeader();
  auto data = Struct{}
      << "i" << (int) header.generatedId
      << "r" << (int) header.receivedId
      << "t" << (int) header.idleTime
      << "p" << packet
      << ValueEnd{};
  sendPacketImpl_strand(data);

  // if (processType_ == ProcessType::Daemon && runtime_->getProcessType() == ProcessType::Player) {
  //   if (auto analytics = runtime_->getAnalytics()) {
  //     analytics->samplePacketOutgoing(data.size());
  //   }
  // }

  sendTimestamp_ = std::chrono::system_clock::now();
  startHeartbeatInterval_strand();
}


void Session::sendHandshake_strand() {
  LOG_ASSERT(!handshakeSent_);
  if (runtime_->getProcessType() == ProcessType::Daemon) {
    auto processAddr = runtime_->getProcessAddr_safe();
    sendPacket_strand(Struct{}
        << "m" << static_cast<int>(Packet::Handshake)
        << "pt" << static_cast<int>(runtime_->getProcessType())
        << "id" << runtime_->getProcessId().str()
        << "host" << processAddr.host
        << "port" << processAddr.port
        << ValueEnd{});
  } else {
    sendPacket_strand(Struct{}
        << "m" << static_cast<int>(Packet::Handshake)
        << "pt" << static_cast<int>(runtime_->getProcessType())
        << "id" << runtime_->getProcessId().str()
        << ValueEnd{});
  }
  handshakeSent_ = true;
}


void Session::joinFederation_safe(ObjectId federationId) {
  assert(processType_ != ProcessType::None);
  assert(!shutdownStarted());

  if (getSessionFederate_safe(federationId)) {
    LOG_ASSERT(!getSessionFederate_safe(federationId));
    return;
  }

  LOG_TRACE("%s[%s] Session[%s]::JoinFederation({%s})",
      str(runtime_->getProcessType()),
      runtime_->getProcessId().debug_str().c_str(),
      getProcessId().debug_str().c_str(),
      federationId.debug_str().c_str());

  auto federate = std::make_shared<SessionFederate>(*runtime_, "Session", strand_, *this);

  if (runtime_->getFederationType_safe(federationId) == FederationType::Lobby) {
    federate->getObjectClass("Match").require({"teams"});
    federate->getObjectClass("Team").require({"slots"});
  }

  std::weak_ptr<SessionFederate> federate_weak = federate;

  federate->setObjectCallback([federate_weak, federationId](ObjectRef object) {
    if (auto this_ = federate_weak.lock()) {
      this_->objectCallback(federationId, object);
    }
  });

  federate->setEventCallback([federate_weak, federationId](const char* eventName, const Value& params) {
    if (auto this_ = federate_weak.lock()) {
      this_->eventCallback(federationId, eventName, params);
    }
  });

  if (processType_ != ProcessType::Agent) {
    federate->setServiceCallback([federate_weak, federationId](const char* service, const Value& params, const std::string& subjectId) {
      auto this_ = federate_weak.lock();
      return this_ ? this_->serviceCallback(federationId, service, params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
  }

  if (processType_ != ProcessType::Agent && processType_ != ProcessType::Headup) {
    federate->setOwnershipCallback([federate_weak, federationId](ObjectRef object, const Property& property, OwnershipNotification notification) {
      if (auto this_ = federate_weak.lock()) {
        this_->ownershipCallback(federationId, object, property, notification);
      }
    });
  } else {
    federate->setOwnershipCallback([federate_weak](ObjectRef object, const Property& property, OwnershipNotification notification) {
    });
  }

  {
    std::lock_guard lock{mutex_};
    auto i = federates_.find(federationId);
    if (shutdownStarted() || (i != federates_.end() && i->second)) {
      federate->shutdown().onResolve<void>([capture_until_done = federate, session = shared_from_this()] {}).done();
      return;
    }
    federates_[federationId] = federate;
  }

  strand_->setImmediate([this_ = shared_from_this(), federate_weak = federate->weak_from_this(), federationId]() {
    if (auto federate = federate_weak.lock()) {
      auto processAddr = this_->runtime_->getProcessAddr_safe();
      auto packet = Struct{}
          << "m" << static_cast<int>(Session::Packet::FederationProcessAdded)
          << "x" << federationId.str()
          << "id" << this_->runtime_->getProcessId().str()
          << "type" << static_cast<int>(this_->runtime_->getProcessType())
          << "host" << processAddr.host
          << "port" << processAddr.port
          << ValueEnd{};
      this_->sendPacket_strand(packet);

      federate->startup(federationId);
    }
  });
}


void Session::leaveFederation(ObjectId federationId) {
  LOG_ASSERT(federationId || isLocalProcessType(processType_));
  LOG_TRACE("%s[%s] Session[%s]::leaveFederation({%s})",
      str(runtime_->getProcessType()),
      runtime_->getProcessId().debug_str().c_str(),
      getProcessId().debug_str().c_str(),
      federationId.debug_str().c_str());

  auto federate = getSessionFederate_safe(federationId);
  if (!federate) {
    if (!isKnownFederation_safe(federationId)) {
      LOG_W("Session::leaveFederation: federate not found");
    }
    return;
  }

  auto capture_until_done = federate->shared_from_this();
  federate->shutdown().onResolve<void>([capture_until_done] {}).done();
}


void Session::removeFederation_safe(ObjectId federationId, Federate& federate) {
  // LOG_ASSERT(federationId || IsLocalProcessType(GetProcess()->GetProcessType()));
  std::lock_guard lock{mutex_};
  auto i = federates_.find(federationId);
  if (i != federates_.end() && i->second.get() == &federate) {
    federates_[federationId] = nullptr;
    strand_->setTimeout([weak_ = weak_from_this(), federationId]() {
      if (auto this_ = weak_.lock()) {
        std::lock_guard lock{this_->mutex_};
        auto f = this_->federates_.find(federationId);
        if (f != this_->federates_.end() && f->second == nullptr) {
          this_->federates_.erase(federationId);
        }
      }
    }, FederationForgetTimeout);
  }
}


void Session::sendAuthenticate_strand(const ProcessAuth& processAuth) {
  auto packet = Struct{}
      << "m" << static_cast<int>(Packet::Authenticate)
      << "a" << processAuth.accessToken
      << "s" << processAuth.subjectId
      << "n" << processAuth.nickname
      << "i" << processAuth.imageUrl
      << ValueEnd{};
  sendPacket_strand(packet);
}


void Session::sendHostRequest_strand(ObjectId lobbyId, ObjectId matchId) {
  LOG_ASSERT(strand_->isCurrent());
  sendPacket_strand(Struct{}
      << "m" << static_cast<int>(Session::Packet::FederationHostingRequest)
      << "x" << lobbyId.str()
      << "i" << matchId.str()
      << ValueEnd{});
}


std::pair<int, Promise<Value>> Session::generateServiceRequest_strand() {
  std::lock_guard lock{mutex_};
  int requestId = ++lastServiceRequestId_;
  Promise<Value> deferred;
  serviceRequests_[requestId] = deferred;
  return std::make_pair(requestId, deferred);
}


void Session::startHeartbeatInterval_strand() {
  if (!heartbeatInterval_) {
    heartbeatInterval_ = strand_->setInterval([weak_ = weak_from_this()]() {
      if (auto this_ = weak_.lock()) {
        if (this_->runtime_ && !this_->shutdownStarted()) {
          auto now = std::chrono::system_clock::now();
          if (this_->shouldShutdownDueToTimeout_strand(now)) {
            this_->shutdown().onResolve<void>([this_]() {}).done();
          } else if (this_->shouldSendHeartbeat_strand(now)) {
            this_->sendHeartbeat_strand();
          }
        }
      }
    }, 100);
  }
}


void Session::stopHeartbeatInterval_strand() {
  if (heartbeatInterval_) {
    clearInterval(*heartbeatInterval_);
    heartbeatInterval_.reset();
  }
}


bool Session::shouldShutdownDueToTimeout_strand(std::chrono::system_clock::time_point now) {
  return processType_ == ProcessType::Player && now > receiveTimestamp_ + ShutdownTimeout;
}


bool Session::shouldSendHeartbeat_strand(std::chrono::system_clock::time_point now) {
  return now >= sendTimestamp_ + HeartbeatInterval;
}


void Session::sendHeartbeat_strand() {
  sendPacket_strand(Struct{}
      << "m" << static_cast<int>(Packet::Heartbeat)
      << ValueEnd{});
}


void Session::onIncomingHandshake_strand(const Value& packet) {
  processHandshake_strand(packet);
  if (processType_ == ProcessType::None) {
    LOG_W("Session: handshake failed");
    auto this_ = shared_from_this();
    return shutdown().onResolve<void>([this_]() {}).done();
  }

  LOG_TRACE("%s[%s] Session[%s]::OnIncomingHandshake(%s[%s])",
      str(runtime_->getProcessType()),
      runtime_->getProcessId().debug_str().c_str(),
      processId_.debug_str().c_str(),
      str(processType_),
      processId_.debug_str().c_str());

  std::vector<ObjectId> broadcastFederationIds{};
  std::unique_lock lock{runtime_->mutex_};
  for (auto& federation : runtime_->federations_) {
    auto federationId = federation->getFederationId();
    if (federationId != Federation::SystemFederationId) {
      broadcastFederationIds.push_back(federationId);
    }
  }
  lock.unlock();

  for (auto& federationId : broadcastFederationIds) {
    const auto processAddr = runtime_->getProcessAddr_safe();
    sendPacket_strand(Struct{}
        << "m" << static_cast<int>(Session::Packet::FederationProcessAdded)
        << "x" << federationId.str()
        << "id" << runtime_->getProcessId().str()
        << "type" << static_cast<int>(runtime_->getProcessType())
        << "host" << processAddr.host
        << "port" << processAddr.port
        << ValueEnd{});
  }

  if (processType_ == ProcessType::Headup) {
    if (!getSessionFederate_safe(Federation::SystemFederationId)) {
      joinFederation_safe(Federation::SystemFederationId);
    }
  }

  // fulhack ???
  auto federationIds = runtime_->getProcessFederations_safe(processId_);
  for (const auto& federationId : federationIds) {
    if (auto federation = runtime_->acquireFederation_safe(federationId, true)) { // try create and auto-join
      if (!getSessionFederate_safe(federationId)) {
        joinFederation_safe(federationId);
      }
      runtime_->releaseFederation_safe(federation);
    }
  }
}


void Session::processHandshake_strand(const Value& packet) {
  const auto processId = ObjectId::parse(packet["id"_c_str]);
  const auto processType = static_cast<ProcessType>(packet["pt"_int]);
  if (processType == ProcessType::Headup && runtime_->getProcessType_safe(processId) != ProcessType::Headup) {
    LOG_E("Sesion::ProcessHandshake, headup process must have been pre-registered by adapter");
    return;
  }
  if (!runtime_->registerProcess_safe(processId, processType, this)) {
    return;
  }

  processId_ = processId;
  processType_ = processType;

  if (processType == ProcessType::Daemon) {
    if (const char* host = packet["host"_c_str]) {
      const char* port = packet["port"_c_str] ?: "";
      runtime_->registerProcessAddr_safe(processId_, host, port);
    }
    if (!handshakeSent_) {
      sendHandshake_strand();
    }
    sendAuthenticate_strand(runtime_->getProcessAuth_safe());
  }
}


void Session::onIncomingAuthenticate_strand(const Value& packet) {
  if (processType_ != ProcessType::None) {
    // TODO: validate authentication
    ProcessAuth auth;
    auth.accessToken = packet["a"_c_str];
    auth.subjectId = packet["s"_c_str];
    auth.nickname = packet["n"_c_str];
    auth.imageUrl = packet["i"_c_str];

    subjectId_ = auth.subjectId;

    runtime_->registerProcessAuth_safe(processId_, auth);
    if (processType_ == ProcessType::Headup) {
      runtime_->registerProcessAuth_safe(runtime_->getProcessId(), auth);
    }
  }
}


void Session::onIncomingMessages_strand(const Value& packet) {
  for (const auto& message : packet["mm"_value]) {
    dispatchMessage_strand(message);
  }
}


void Session::dispatchMessage_strand(const Value& message) {
  int messageType = message["m"_int];
  // if (processType_ == ProcessType::Daemon && runtime_->getProcessType() == ProcessType::Player) {
  //   if (auto analytics = runtime_->getAnalytics()) {
  //     analytics->sampleMessage(messageType);
  //   }
  // }
  auto m = static_cast<Message>(messageType);
  switch (m) {
    case Message::ObjectChanges:
      return onIncomingObjectChanges_strand(message);
    case Message::EventDispatch:
      return onIncomingEvent_strand(message);
    case Message::ServiceRequest:
      return onIncomingServiceRequest_strand(message).onResolve<void>([capture_until_done = shared_from_this()]{}).done();
    case Message::ServiceFulfill:
      return onIncomingServiceFulfill_strand(message);
    case Message::ServiceReject:
      return onIncomingServiceReject_strand(message);

    case Message::RoutingRequestDownstream:
      return onIncomingRoutingMessage_strand(message, m, OwnershipOperation::NegotiatedOwnershipDivestiture);
    case Message::RoutingEnableDownstream:
      return onIncomingRoutingMessage_strand(message, m, OwnershipOperation::ForcedOwnershipDivestiture);
    case Message::RoutingRequestUpstream:
      return onIncomingRoutingMessage_strand(message, m, OwnershipOperation::OwnershipAcquisition);
    case Message::RoutingEnableUpstream:
      return onIncomingRoutingMessage_strand(message, m, OwnershipOperation::ForcedOwnershipAcquisition);
    case Message::RoutingUpstreamDenied:
      return onIncomingRoutingMessage_strand(message, m, OwnershipOperation::OwnershipReleaseFailure);
    case Message::RoutingDisable:
      return onIncomingRoutingMessage_strand(message, m, OwnershipOperation::None);

    default:
      return;
  }
}


static std::size_t GetPrecedenceFactor(ProcessType processType) {
  switch (processType) {
    case ProcessType::Daemon: return 2;
    case ProcessType::Player: return 1;
    default: return 0;
  }
}


static std::size_t GetPrecedenceFactor(ObjectId processId) {
  return std::hash<ObjectId>{}(processId);
}


static bool HasPrecedenceLessThan(ProcessType t1, ObjectId p1, ProcessType t2, ObjectId p2) {
  auto f1 = GetPrecedenceFactor(t1);
  auto f2 = GetPrecedenceFactor(t2);
  if (f1 == f2) {
    f1 = GetPrecedenceFactor(p1);
    f2 = GetPrecedenceFactor(p2);
  }
  return f1 < f2;
}


void Session::onIncomingObjectChanges_strand(const Value& message) {
  const char* federationIdString = message["x"_c_str];
  if (!federationIdString) {
    return LOG_W("%s-%s Session::OnIncomingObjectChanges: missing federationId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }
  auto federationId = ObjectId::parse(federationIdString);

  auto federate = findFederate_strand(federationId);
  if (!federate) {
    if (!isKnownFederation_safe(federationId)) {
      LOG_D("%s-%s Session::OnIncomingObjectChanges: federation/federate not found %s",
          str(runtime_->getProcessType()),
          runtime_->getProcessId().str().c_str(),
          federationId.str().c_str());
    }
    return;
  }

  const char* objectClass = message["c"_c_str];
  if (!objectClass) {
    return LOG_W("%s-%s Session::OnIncomingObjectChanges: objectClass not found",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto objectId = message["i"_ObjectId];
  if (!objectId) {
    return LOG_W("%s-%s Session::OnIncomingObjectChanges: missing objectId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  if (federate->shutdownStarted()) {
    return;
  }

  auto objectChange = static_cast<ObjectChange>(message["t"_int]);
  if (objectChange == ObjectChange::Delete) {
    if (auto object = federate->getObject(objectId)) {
      if (!object.canDelete()) {
        if (!federate->ownershipPolicy(Property::Destructor_str)) {
          return LOG_W("Spurious object delete blocked from session: %s (%s)", objectClass, objectId.str().c_str());
        }
        auto& property = object[Property::Destructor_cstr];
        if (property.instanceOwnership_.second == OwnershipOperation::None) {
          property.modifyOwnershipState(OwnershipOperation::ForcedOwnershipAcquisition);
        }
      }
      if (object.canDelete()) {
        object.Delete();
      } else {
        LOG_ASSERT(object.canDelete());
      }
    }
  } else {
    auto object = federate->getObject(objectId) ?: federate->getObjectClass(objectClass).create(objectId);
    for (auto& p : message["p"]) {
      auto& property = object[p.name()];
      auto processId = p["p"_ObjectId];
      if (property.canSetValue() || tryAutoCorrectRouting(*federate, federationIdString, objectId, property, processId)) {
        double delay = p["t"_double] - latencyTracker_.getLatency();
        property.setValue(p["v"_value], delay, this, processId);
      }
    }
  }
}


bool Session::tryAutoCorrectRouting(Federate& federate, const char* federationId, ObjectId objectId, Property& property, ObjectId processId) {
  if (property.session_ != this && processId == property.processId_) {
    dynamic_cast<SessionFederate&>(federate).enqueueMessage(Struct{}
        << "m" << static_cast<std::int32_t>(Session::Message::RoutingDisable)
        << "x" << federationId
        << "i" << objectId
        << "p" << property.getName()
        << ValueEnd{});
    return false;
  }
  bool spurious = !federate.ownershipPolicy(property.getName());
  if (spurious || HasPrecedenceLessThan(processType_, processId_, runtime_->getProcessType(), runtime_->getProcessId())) {
    if (spurious) {
      LOG_W("Spurious object update blocked from session: %s", property.getName().c_str());
    }
    dynamic_cast<SessionFederate&>(federate).enqueueMessage(Struct{}
        << "m" << static_cast<std::int32_t>(Session::Message::RoutingEnableUpstream)
        << "x" << federationId
        << "i" << objectId
        << "p" << property.getName()
        << ValueEnd{});
    return false;
  }
  if (property.instanceOwnership_.second == OwnershipOperation::None) {
    property.modifyOwnershipState(OwnershipOperation::ForcedOwnershipAcquisition);
    return true;
  }
  LOG_W("xxxx");
  return false;
}


void Session::onIncomingEvent_strand(const Value& message) {
  const char* event = message["e"_c_str];
  if (!event) {
    return LOG_W("%s-%s Session::OnIncomingEvent: missing event",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  const char* federationIdString = message["x"_c_str];
  if (!federationIdString) {
    return LOG_W("%s-%s Session::OnIncomingEvent: missing federationId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto federationId = ObjectId::parse(federationIdString);
  auto federate = findFederate_strand(federationId);
  if (!federate) {
    if (!isKnownFederation_safe(federationId)) {
      LOG_D("%s-%s Session::OnIncomingEvent: federation/federate not found",
          str(runtime_->getProcessType()),
          runtime_->getProcessId().str().c_str());
    }
    return;
  }

  if (federate->shutdownStarted()) {
    return;
  }

  double delay = message["d"_double];
  double latency = message["t"_double] + latencyTracker_.getLatency();
  federate->dispatchEvent(*federate, event, message["v"_value], delay, latency);
}


Promise<void> Session::onIncomingServiceRequest_strand(Value message) {
  co_await *strand_;
  int requestId = message["r"_int];
  if (!requestId) {
    sendPacket_strand(makeRejectPacket(requestId, 400, "missing requestId"));
    co_return LOG_W("%s-%s Session::OnIncomingServiceRequest: missing requestId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  const char* serviceName = message["s"_c_str];
  if (!serviceName) {
    sendPacket_strand(makeRejectPacket(requestId, 400, "missing serviceName"));
    co_return LOG_W("%s-%s Session::OnIncomingServiceRequest: missing serviceName",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  const char* federationIdString = message["x"_c_str];
  if (!federationIdString) {
    sendPacket_strand(makeRejectPacket(requestId, 400, "missing federationId"));
    co_return LOG_W("%s-%s Session::OnIncomingServiceRequest: missing federationId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto federationId = ObjectId::parse(federationIdString);
  auto federate = std::static_pointer_cast<SessionFederate>(findFederate_strand(federationId));
  if (!federate) {
    sendPacket_strand(makeRejectPacket(requestId, 404, "federation/federate not found"));
    if (!isKnownFederation_safe(federationId)) {
      LOG_D("%s-%s Session::OnIncomingServiceRequest: federation/federate not found",
          str(runtime_->getProcessType()),
          runtime_->getProcessId().str().c_str());
    }
    co_return;
  }
  if (federate->shutdownStarted()) {
    sendPacket_strand(makeRejectPacket(requestId, 404, "federate is shutdown"));
    co_return;
  }

  const char* subjectId = (processType_ == ProcessType::Daemon ? message["i"_c_str] : nullptr) ?: subjectId_.c_str();

  Value response;
  try {
    auto result = co_await federate->requestService(serviceName, message["v"_value], subjectId, federate.get());
    response = Struct{}
        << "m" << static_cast<std::int32_t>(Message::ServiceFulfill)
        << "r" << requestId
        << "v" << result
        << ValueEnd{};
  } catch (const Value& value) {
    response = Struct{}
        << "m" << static_cast<std::int32_t>(Message::ServiceReject)
        << "r" << requestId
        << "v" << value
        << ValueEnd{};
  } catch (...) {
    response = Struct{}
        << "m" << static_cast<std::int32_t>(Message::ServiceReject)
        << "r" << requestId
        << "v" << REASON(500, "unknown error")
        << ValueEnd{};
  }
  federate->enqueueMessage(response);
}


Value Session::makeRejectPacket(int requestId, int reasonCode, const char* reasonText) {
  return Struct{}
      << "m" << static_cast<std::int32_t>(Packet::Messages) << "mm" << Array{} << Struct{}
      << "m" << static_cast<std::int32_t>(Message::ServiceReject)
      << "r" << requestId
      << "v" << REASON(reasonCode, reasonText)
      << ValueEnd{} << ValueEnd{} << ValueEnd{};
}


void Session::onIncomingServiceFulfill_strand(const Value& message) {
  std::lock_guard lock{mutex_};
  int requestId = message["r"_int];
  auto i = serviceRequests_.find(requestId);
  if (i == serviceRequests_.end()) {
    return LOG_W("%s-%s Session::OnIncomingServiceFulfill: requestId %d not found",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str(),
        requestId);
  }

  i->second.resolve(message["v"_value]).done();
  serviceRequests_.erase(requestId);
}


void Session::onIncomingServiceReject_strand(const Value& message) {
  std::lock_guard lock{mutex_};
  int requestId = message["r"_int];
  auto i = serviceRequests_.find(requestId);
  if (i == serviceRequests_.end()) {
    return LOG_W("%s-%s Session::OnIncomingServiceReject: requestId %d not found",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str(),
        requestId);
  }

  i->second.reject<Value>(message["v"_value]).done();
  serviceRequests_.erase(requestId);
}


void Session::onIncomingRoutingMessage_strand(const Value& message, Message msg, OwnershipOperation operation) {
  const char* federationIdString = message["x"_c_str];
  if (!federationIdString) {
    return LOG_W("%s-%s Session::OnIncomingRoutingMessage: missing federationId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto federationId = ObjectId::parse(federationIdString);
  if (!federationId && processType_ != ProcessType::Headup) {
    return LOG_W("%s-%s Session::OnIncomingRoutingMessage: invalid federationId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto federate = getSessionFederate_safe(federationId);
  if (!federate) {
    if (!isKnownFederation_safe(federationId)) {
      LOG_D("%s-%s Session::OnIncomingRoutingMessage: federate not found",
          str(runtime_->getProcessType()),
          runtime_->getProcessId().str().c_str());
    }
    return;
  }
  if (federate->shutdownStarted()) {
    LOG_W("%s-%s Session::OnIncomingRoutingMessage: federate is shutdown",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
    return;
  }

  auto object = federate->getObject(message["i"_ObjectId]);
  if (!object) {
    return LOG_D("%s-%s Session::OnIncomingRoutingMessage: object not found",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  const char* propertyName = message["p"_c_str];
  if (!propertyName) {
    return LOG_W("%s-%s Session::OnIncomingRoutingMessage: missing propertyName",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto& property = object.getProperty(propertyName);

  const auto forcedOwnershipAcquisitionBlocked = operation == OwnershipOperation::ForcedOwnershipAcquisition
      && federate->getFederation()->getExclusiveOwner()
      && federate->getFederation()->getExclusiveOwner() != federate;
  if (forcedOwnershipAcquisitionBlocked) {
    federate->ownershipCallback(federationId, object, property, OwnershipNotification::OwnershipUnavailable);
    return LOG_ASSERT(!forcedOwnershipAcquisitionBlocked);
  }

  if (msg == Message::RoutingEnableDownstream) {
    property.routing_ = true;
  } else if (msg == Message::RoutingDisable) {
    property.routing_ = false;
    LOG_ROUTING("%s[%s] Session[%s]::OnIncomingRoutingMessage({%s}, '%s', %s, %s)",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().debug_str().c_str(),
        processId_.debug_str().c_str(),
        federationId.debug_str().c_str(),
        propertyName,
        messageToString(msg),
        str(operation));
  }

  if (operation != OwnershipOperation::None) {
    auto ownershipState = property.getOwnershipState();
    if (isValidStateBeforeOperation(ownershipState, operation)) {
      LOG_ROUTING("%s[%s] Session[%s]::OnIncomingRoutingMessage({%s}, '%s', %s, %s)",
          str(runtime_->getProcessType()),
          runtime_->getProcessId().debug_str().c_str(),
          processId_.debug_str().c_str(),
          federationId.debug_str().c_str(),
          propertyName,
          messageToString(msg),
          str(operation));

      object.getProperty(propertyName).modifyOwnershipState(operation);

    } else if (isValidStateAfterOperation(ownershipState, operation)) {
      /*LOG_D("%s Session::OnIncomingRoutingMessage: %s %s, OPERATION %s REDUNDANT FOR STATE %s",
          federate->GetDescription().c_str(),
          propertyName,
          MessageToString(msg),
          str(operation),
          ownershipState.str().c_str());*/

    } else {
      LOG_D("%s Session::OnIncomingRoutingMessage: %s %s, OPERATION %s INVALID FOR STATE %s",
          federate->getDescription().c_str(),
          propertyName,
          messageToString(msg),
          str(operation),
          ownershipState.str().c_str());
    }
  }
}


void Session::onIncomingFederationProcessAdded_strand(const Value& packet) {
  auto processId = ObjectId::parse(packet["id"_c_str]);
  if (!runtime_->registerProcess_safe(processId, static_cast<ProcessType>(packet["type"_int]), nullptr)) {
    return LOG_W("%s-%s Session::OnIncomingFederationProcessAdded: could not register process",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  const char* federationIdString = packet["x"_c_str];
  if (!federationIdString) {
    return LOG_W("%s-%s Session::OnIncomingRoutingMessage: missing federationId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto federationId = ObjectId::parse(federationIdString);
  if (!federationId && !isLocalProcessType(processType_)) {
    return LOG_W("%s-%s Session::OnIncomingFederationProcessAdded: local federationId not allowed",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto processAddr = ProcessAddr{packet["host"_c_str] ?: "", packet["port"_c_str] ?: ""};
  if (!processAddr.host.empty()) {
    runtime_->registerProcessAddr_safe(processId, processAddr.host.c_str(), processAddr.port.c_str());
  }

  const auto processType = runtime_->getProcessType_safe(processId);
  if (endpoint_ && (processType == ProcessType::Player || processType == ProcessType::Daemon)) {
    endpoint_->broadcastFederationProcessAdded_safe(federationId, processId, processType, processAddr, this);
  }

  runtime_->federationProcessAdded_safe(federationId, processId);
}


void Session::onIncomingFederationProcessRemoved_strand(const Value& packet) {
  const char* federationIdString = packet["x"_c_str];
  if (!federationIdString) {
    return LOG_W("%s-%s Session::OnIncomingFederationProcessRemoved: missing federationId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto federationId = ObjectId::parse(federationIdString);
  if (!federationId && processType_ != ProcessType::Headup) {
    return LOG_W("%s-%s Session::OnIncomingFederationProcessRemoved: invalid federationId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto processId = ObjectId::parse(packet["id"_c_str]);
  if (runtime_->getProcessType_safe(processId) == ProcessType::None) {
    return LOG_W("%s-%s Session::OnIncomingFederationProcessRemoved: invalid process",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  runtime_->federationProcessRemoved_safe(federationId, processId);
}


void Session::onIncomingFederationHostingRequest_strand(const Value& packet) {
  const char* lobbyIdString = packet["x"_c_str];
  if (!lobbyIdString) {
    return LOG_W("%s-%s Session::OnIncomingFederationHostingRequest: missing lobbyId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto lobbyId = ObjectId::parse(lobbyIdString);
  if (!lobbyId) {
    return LOG_W("%s-%s Session::OnIncomingFederationHostingRequest: invalid lobbyId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  const char* matchIdString = packet["i"_c_str];
  if (!matchIdString) {
    return LOG_W("%s-%s Session::OnIncomingFederationHostingRequest: missing matchId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  auto matchId = ObjectId::parse(matchIdString);
  if (!matchId) {
    return LOG_W("%s-%s Session::OnIncomingFederationHostingRequest: invalid matchId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  if (processType_ == ProcessType::None) {
    return LOG_W("%s-%s Session::OnIncomingFederationHostingRequest: no process",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  if (subjectId_.empty()) {
    return LOG_W("%s-%s Session::OnIncomingFederationHostingRequest: no subjectId",
        str(runtime_->getProcessType()),
        runtime_->getProcessId().str().c_str());
  }

  runtime_->processHostMatch_safe(lobbyId, matchId, subjectId_);
}


/***/


void Session::trySendOutgoingPacket_strand(const Value& packet) {
  bool connected;
  std::unique_lock lock{mutex_};
  connected = connected_;
  lock.unlock();

  if (connected) {
    sendPacket_strand(packet);
  } else {
    enqueueOutgoingPacket_strand(packet);
  }
}


void Session::enqueueOutgoingPacket_strand(const Value& packet) {
  std::lock_guard lock{mutex_};
  outgoingPacketQueue_.push_back(packet);
}


void Session::emptyOutgoingPacketQueue_strand() {
  std::vector<Value> queue{};
  std::unique_lock lock{mutex_};
  std::swap(queue, outgoingPacketQueue_);
  lock.unlock();

  for (const Value& packet : queue) {
    sendPacket_strand(packet);
  }
}


std::shared_ptr<Federate> Session::findFederate_strand(ObjectId federationId) {
  std::lock_guard lock{mutex_};
  auto i = federates_.find(federationId);
  if (i != federates_.end()) {
    if (auto federate = i->second) {
      if (!federate->shutdownStarted()) {
        return federate;
      }
    }
  }
  return nullptr;
}


bool Session::isKnownFederation_safe(ObjectId federationId) {
  std::lock_guard lock{mutex_};
  return federates_.find(federationId) != federates_.end();
}


char Session::getDoNotDistributePrefix_strand() const {
  switch (processType_) {
    case ProcessType::Headup:
    case ProcessType::Module:
      return '\0';
    default:
      return '_';
  }
}


void Session::onProcessAuthenticated_main(ObjectId processId, const ProcessAuth& processAuth) {
  if (processId == runtime_->getProcessId() && !processAuth.accessToken.empty()) {
    strand_->setImmediate([this_ = shared_from_this(), processAuth] () {
      this_->sendAuthenticate_strand(processAuth);
    });
  }
}


const char* Session::messageToString(Session::Message message) {
  switch (message) {
    case Session::Message::None:
      return "None";
    case Session::Message::ObjectChanges:
      return "ObjectChanges";
    case Session::Message::EventDispatch:
      return "EventDispatch";
    case Session::Message::ServiceRequest:
      return "ServiceRequest";
    case Session::Message::ServiceFulfill:
      return "ServiceFulfill";
    case Session::Message::ServiceReject:
      return "ServiceReject";
    case Session::Message::RoutingRequestDownstream:
      return "RoutingRequestDownstream";
    case Session::Message::RoutingEnableDownstream:
      return "RoutingEnableDownstream";
    case Session::Message::RoutingRequestUpstream:
      return "RoutingRequestUpstream";
    case Session::Message::RoutingEnableUpstream:
      return "RoutingEnableUpstream";
    case Session::Message::RoutingUpstreamDenied:
      return "RoutingUpstreamDenied";
    case Session::Message::RoutingDisable:
      return "RoutingDisable";
  }
}
