// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "mock-session.h"
#include "mock-endpoint.h"


MockSession::MockSession(MockEndpoint& endpoint, std::shared_ptr<Strand_base> strand) :
    Session{endpoint, std::move(strand)},
    mockEndpoint_{std::static_pointer_cast<MockEndpoint>(endpoint.shared_from_this())} {
}


void MockSession::setRemote(MockSession& remote) {
  remote_ = std::dynamic_pointer_cast<MockSession>(remote.shared_from_this());
  sendHandshake_strand();
}


void MockSession::disconnect() {
  if (!disconnected_) {
    disconnected_ = true;
    if (auto endpoint = mockEndpoint_.lock()) {
      endpoint->onSessionClosed(*this);
    }
    shutdown().done();
    remote_->disconnect();
  }
}


void MockSession::sendPacketImpl_strand(const Value& message) {
  if (!disconnected_ && !remote_->disconnected_) {
    remote_->strand_->setImmediate([remote = remote_, message]() {
      remote->receivePacket_strand(message);
    });
  }
}
