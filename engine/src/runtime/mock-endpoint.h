// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__MOCK_ENDPOINT_H
#define WARSTAGE__RUNTIME__MOCK_ENDPOINT_H

#include "endpoint.h"

class MockSession;

class MockEndpoint : public Endpoint {
  std::shared_ptr<Strand_base> strand_{};
  std::weak_ptr<MockEndpoint> master_{};
  std::vector<std::weak_ptr<MockSession>> mockSessions_{};
  bool disconnected_{};

public:
  explicit MockEndpoint(Runtime& runtime, std::shared_ptr<Strand_base> strand);

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

public:
  void setMasterEndpoint(MockEndpoint& endpoint);

  void disconnect();
  void reconnect();

  void onSessionClosed(MockSession& session);

protected: // Endpoint
  [[nodiscard]] std::shared_ptr<Session> makeSession_safe(const std::string& url) override;
};

#endif
