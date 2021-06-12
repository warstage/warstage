// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__SESSION_H
#define WARSTAGE__RUNTIME__SESSION_H

#include "./federate.h"
#include "./runtime.h"
#include "async/shutdownable.h"
#include <map>

class Endpoint;
class Session;
class SessionFederate;


struct LatencyHeader {
  std::uint16_t generatedId;
  std::uint16_t receivedId;
  std::uint16_t idleTime; // milliseconds
};

class LatencyTracker {
  using clock = std::chrono::steady_clock;
  std::vector<std::pair<std::uint16_t, clock::time_point>> generated_{};
  std::uint16_t lastGeneratedId_{};
  std::uint16_t lastReceivedId_{};
  clock::time_point lastReceivedTime_ = clock::now();
  double latency_{};

public:
  [[nodiscard]] double getLatency() const { return latency_; }
  [[nodiscard]] LatencyHeader generateHeader();
  void receiveHeader(const LatencyHeader& header);

private:
  [[nodiscard]] static std::uint16_t durationToIdleTime(clock::duration value);
  [[nodiscard]] static clock::duration idleTimeToDuration(std::uint16_t value);
};


class Session :
    public RuntimeObserver,
    public Shutdownable,
    public std::enable_shared_from_this<Session>
{
  friend class Endpoint;
  friend class Federate;
  friend class Runtime;
  friend class SessionFederate;

protected:
  static constexpr std::chrono::duration ShutdownTimeout = std::chrono::milliseconds{6000}; // TODO: should be 2000 ???
  static constexpr std::chrono::duration HeartbeatInterval = std::chrono::milliseconds{1000};
  static constexpr int FederationForgetTimeout = 15 * 1000; // milliseconds

  enum class Packet {
    Heartbeat = 0,
    Handshake = 1,
    Authenticate = 2,
    Messages = 3,
    FederationProcessAdded = 4,
    FederationProcessRemoved = 5,
    FederationHostingRequest = 6
  };

  enum class Message {
    None = 0,
    ObjectChanges = 1,
    EventDispatch = 2,
    ServiceRequest = 3,
    ServiceFulfill = 4,
    ServiceReject = 5,
    RoutingRequestDownstream = 6,
    RoutingEnableDownstream = 7,
    RoutingRequestUpstream = 9,
    RoutingEnableUpstream = 10,
    RoutingUpstreamDenied = 8,
    RoutingDisable = 11
  };

protected:
  Runtime *const runtime_{};
private:
  Endpoint* endpoint_{};
protected:
  std::shared_ptr<Strand_base> strand_{};
private:
  ObjectId processId_{};
  ProcessType processType_{};
  std::string subjectId_{};

  std::map<int, Promise<Value>> serviceRequests_{};
  int lastServiceRequestId_{};
  std::unordered_map<ObjectId, std::shared_ptr<Federate>> federates_{}; // _mutex
  std::vector<Value> outgoingPacketQueue_{};
  bool connected_{};
  bool handshakeSent_{};
  std::mutex mutex_{};
  LatencyTracker latencyTracker_{};
  std::shared_ptr<IntervalObject> heartbeatInterval_{};
  std::chrono::system_clock::time_point sendTimestamp_{};
  std::chrono::system_clock::time_point receiveTimestamp_{};

public:
  explicit Session(Endpoint& endpoint, std::shared_ptr<Strand_base> strand);
  ~Session() override;

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

public:
  [[nodiscard]] ObjectId getProcessId() const { return processId_; }
  [[nodiscard]] ProcessType getProcessType() const { return processType_; }
  [[nodiscard]] Strand_base& getStrand() const { return *strand_; }
  [[nodiscard]] SessionFederate* getSessionFederate_safe(ObjectId federationId);

protected:
  void receivePacket_strand(const Value& packet);
  void sendPacket_strand(const Value& packet);

  virtual void sendPacketImpl_strand(const Value& packet) = 0;

  void sendHandshake_strand();

private:
  void joinFederation_safe(ObjectId federationId);
  void leaveFederation(ObjectId federationId);
  void removeFederation_safe(ObjectId federationId, Federate& federate);

  void sendAuthenticate_strand(const ProcessAuth& processAuth);
  void sendHostRequest_strand(ObjectId lobbyId, ObjectId matchId);

  [[nodiscard]] std::pair<int, Promise<Value>> generateServiceRequest_strand();

  void startHeartbeatInterval_strand();
  void stopHeartbeatInterval_strand();
  bool shouldShutdownDueToTimeout_strand(std::chrono::system_clock::time_point now);
  bool shouldSendHeartbeat_strand(std::chrono::system_clock::time_point now);
  void sendHeartbeat_strand();

  void onIncomingHandshake_strand(const Value& packet);
  void processHandshake_strand(const Value& packet);

  void onIncomingAuthenticate_strand(const Value& packet);

  void onIncomingMessages_strand(const Value& packet);
  void dispatchMessage_strand(const Value& message);
  void onIncomingObjectChanges_strand(const Value& message);
  bool tryAutoCorrectRouting(Federate& federate, const char* federationId, ObjectId objectId, Property& property, ObjectId processId);

  void onIncomingEvent_strand(const Value& message);

  Promise<void> onIncomingServiceRequest_strand(Value message);
  static Value makeRejectPacket(int requestId, int reasonCode, const char* reasonText);
  void onIncomingServiceFulfill_strand(const Value& message);
  void onIncomingServiceReject_strand(const Value& message);
  void onIncomingRoutingMessage_strand(const Value& message, Message msg, OwnershipOperation operation);

  void onIncomingFederationProcessAdded_strand(const Value& packet);
  void onIncomingFederationProcessRemoved_strand(const Value& packet);
  void onIncomingFederationHostingRequest_strand(const Value& packet);

  void trySendOutgoingPacket_strand(const Value& packet);
  void enqueueOutgoingPacket_strand(const Value& packet);
  void emptyOutgoingPacketQueue_strand();

  [[nodiscard]] std::shared_ptr<Federate> findFederate_strand(ObjectId federationId);
  [[nodiscard]] bool isKnownFederation_safe(ObjectId federationId);

  char getDoNotDistributePrefix_strand() const;

protected:
  void onProcessAuthenticated_main(ObjectId processId, const ProcessAuth& processAuth) override;

  [[nodiscard]] static const char* messageToString(Message message);
};


#endif
