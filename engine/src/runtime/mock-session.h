// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__MOCK_SESSION_H
#define WARSTAGE__RUNTIME__MOCK_SESSION_H

#include "session.h"

class MockEndpoint;

class MockSession : public Session {
  friend class MockEndpoint;

  std::weak_ptr<MockEndpoint> mockEndpoint_{};
  std::shared_ptr<MockSession> remote_{};
  bool disconnected_{};

public:
  MockSession(MockEndpoint& endpoint, std::shared_ptr<Strand_base> strand);

  void setRemote(MockSession& remote);
  void disconnect();

protected:
  void sendPacketImpl_strand(const Value& message) override;
};

#endif
