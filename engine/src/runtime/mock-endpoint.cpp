// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "mock-endpoint.h"
#include "mock-session.h"


MockEndpoint::MockEndpoint(Runtime& runtime, std::shared_ptr<Strand_base> strand) : Endpoint(runtime),
    strand_{std::move(strand)} {
}


Promise<void> MockEndpoint::shutdown_() {
  for (const auto& i : mockSessions_) {
    if (auto session = i.lock()) {
      co_await session->shutdown();
    }
  }
  mockSessions_.clear();

  if (auto master = master_.lock()) {
    co_await master->shutdown();
    master_.reset();
  }

  co_await Endpoint::shutdown_();
}


void MockEndpoint::setMasterEndpoint(MockEndpoint& endpoint) {
  master_ = std::static_pointer_cast<MockEndpoint>(endpoint.shared_from_this());
  setMasterUrl_safe("master");
}


void MockEndpoint::disconnect() {
  disconnected_ = true;
  for (auto& i : mockSessions_) {
    if (auto session = i.lock()) {
      session->disconnect();
    }
  }
}


void MockEndpoint::reconnect() {
  disconnected_ = false;
}


void MockEndpoint::onSessionClosed(MockSession& session) {
  onSessionClosed_safe(session);
}


std::shared_ptr<Session> MockEndpoint::makeSession_safe(const std::string& url) {
  if (url == "master") {
    if (auto master = master_.lock()) {
      auto result = std::make_shared<MockSession>(*this, strand_);
      auto remote = std::make_shared<MockSession>(*master, master->strand_);
      result->setRemote(*remote);
      remote->setRemote(*result);
      mockSessions_.push_back(result);
      master->mockSessions_.push_back(remote);
      return result;
    }
  }
  return nullptr;
}
