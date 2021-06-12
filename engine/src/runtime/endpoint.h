// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__ENDPOINT_H
#define WARSTAGE__RUNTIME__ENDPOINT_H

#include "async/promise.h"
#include "async/shutdownable.h"
#include "async/strand.h"
#include "value/object-id.h"
#include <string>
#include <vector>

enum class ProcessType;
struct ProcessAddr;
class Federation;
class Runtime;
class Session;

class Endpoint :
    public Shutdownable,
    public std::enable_shared_from_this<Endpoint>
{
  friend class Runtime;
  friend class Session;

private:
  Runtime* const runtime_{};
  std::mutex mutex_{};
  std::vector<Session*> sessions_{};
  std::string serverUrl_{};
  std::weak_ptr<Session> masterSession_{};
  std::shared_ptr<TimeoutObject> masterConnectObject_{};
  int masterConnectDelay_{};
  std::function<void(const Session& session)> sessionClosedHandler_{};

public:
  explicit Endpoint(Runtime& runtime);
  ~Endpoint() override;

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

public:
  void setMasterUrl_safe(std::string value);
  void requestHostMatch_safe(ObjectId lobbyId, ObjectId matchId);
  void setSessionClosedHandler(std::function<void(const Session& session)> value) {
    sessionClosedHandler_ = std::move(value);
  }

protected:
  virtual std::shared_ptr<Session> makeSession_safe(const std::string& url) = 0;

  void onSessionClosed_safe(Session& session);

private:
  void broadcastFederationProcessAdded_safe(ObjectId federationId, ObjectId processId, ProcessType processType, const ProcessAddr& processAddr, Session* origin);
  void broadcastFederationProcessRemoved_safe(ObjectId federationId, ObjectId processId);
  [[nodiscard]] static bool shouldRelayFederationProcessAdded(const Session& origin, const Session& target);

  void addSession_safe(Session* session);
  void removeSession_safe(Session* session);

  void tryConnectMaster_mutex();
};


#endif
