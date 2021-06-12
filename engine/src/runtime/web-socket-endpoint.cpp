// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./web-socket-session.h"
#include "./web-socket-endpoint.h"
#include "./session.h"
#include "utilities/logging.h"
#include "web-socket-endpoint.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio.hpp>

using namespace std::placeholders;

#define LOG_TRACE(format, ...)          LOG_X(format, ##__VA_ARGS__)


WebSocketEndpoint::WebSocketEndpoint(Runtime& runtime, std::shared_ptr<boost::asio::io_context> ioc) : Endpoint{runtime},
    ioc_{std::move(ioc)},
    acceptor_{*ioc_},
    socket_{*ioc_},
    resolver_{*ioc_}
{
  LOG_LIFECYCLE("%p WebSocketEndpoint +", this);
}


WebSocketEndpoint::~WebSocketEndpoint() {
  LOG_LIFECYCLE("%p WebSocketEndpoint ~", this);
  std::lock_guard lock{mutex_};
  LOG_ASSERT(sessions_.empty());
  LOG_ASSERT(!acceptor_.is_open());
  LOG_ASSERT(!socket_.is_open());
}


unsigned short WebSocketEndpoint::startup_safe(unsigned short port) {
  {
    std::lock_guard lock{mutex_};
    boost::system::error_code ec;

    auto const address = boost::asio::ip::make_address("0.0.0.0");
    auto const ep = boost::asio::ip::tcp::endpoint{address, port};

    acceptor_.open(ep.protocol(), ec);
    if (ec) {
      return 0;
    }

    acceptor_.set_option(boost::asio::socket_base::reuse_address{true}, ec);
    if (ec) {
      logError(ec, "set_option(reuse_address)");
    }

    acceptor_.bind(ep, ec);
    if (ec) {
      logError(ec, "bind");
      return 0;
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
      logError(ec, "listen");
      return 0;
    }

    if (acceptor_.is_open()) {
      port = acceptor_.local_endpoint().port();
      LOG_TRACE("WebSocketEndpoint listing on port %d", port);
    } else {
      port = 0;
    }
  }

  if (port) {
    doAccept_safe();
  }

  return port;
}


Promise<void> WebSocketEndpoint::shutdown_() {
  LOG_LIFECYCLE("%p WebSocketEndpoint Shutdown", this);

  std::unique_lock lock{mutex_};
  boost::system::error_code ec;
  acceptor_.close(ec);
  if (ec) {
    logError(ec, "_acceptor.close");
  }
  lock.unlock();

  co_await Endpoint::shutdown_();
}


/***/


std::shared_ptr<Session> WebSocketEndpoint::makeSession_safe(const std::string& url) {
  LOG_ASSERT(!shutdownStarted());

  auto i = url.rfind(':');
  if (i < 5 || std::strncmp(url.c_str(), "ws://", 5) != 0) {
    LOG_E("WebSocketEndpoint::MakeSession, invalid url %s", url.c_str());
    return nullptr;
  }

  auto session = std::make_shared<WebSocketSession>(ioc_, *this);
  std::unique_lock lock(mutex_);
  sessions_.push_back(session);
  lock.unlock();

  std::string host = url.substr(5, i - 5);
  std::string port = url.substr(i + 1);
  session->doResolve(resolver_, host.c_str(), port.c_str());

  return session;
}


/***/


void WebSocketEndpoint::doAccept_safe() {
  LOG_TRACE("WebSocketEndpoint doAccept");
  std::lock_guard lock{mutex_};
  acceptor_.async_accept(socket_, [this_ = std::static_pointer_cast<WebSocketEndpoint>(shared_from_this())](auto ec) {
    this_->onAccept_safe(ec);
  });
}


void WebSocketEndpoint::onAccept_safe(boost::system::error_code ec) {
  LOG_TRACE("WebSocketEndpoint onAccept");
  bool doAcceptAgain = false;
  std::unique_lock lock{mutex_};
  if (shutdownStarted() || !acceptor_.is_open()) {
    logError(ec, "onAccept/Shutdown");
    socket_.close(ec);
    if (ec) {
      logError(ec, "accept_async/close");
    }
  } else {
    if (ec) {
      if (ec && ec != boost::asio::error::operation_aborted) {
        logError(ec, "accept_async");
      }
      socket_.close(ec);
      if (ec) {
        logError(ec, "accept_async/close");
      }
    } else {
      socket_.set_option(boost::asio::ip::tcp::no_delay{true}, ec);
      if (ec) {
        logError(ec, "set_option(no_delay)");
      }
      auto session = std::make_shared<WebSocketSession>(ioc_, *this, std::move(socket_));
      sessions_.push_back(session);
      session->doAccept();
    }
    doAcceptAgain = true;
  }
  lock.unlock();

  if (doAcceptAgain) {
    doAccept_safe();
  }
}


void WebSocketEndpoint::removeConnection_safe(WebSocketSession* session) {
  std::lock_guard lock(mutex_);
  sessions_.erase(
      std::remove_if(sessions_.begin(), sessions_.end(), [session](auto& x) {
        return x.get() == session;
      }),
      sessions_.end());
}


void WebSocketEndpoint::logError(boost::system::error_code ec, const char* op) {
  LOG_E("WebSocketEndpoint, error: %s: %s", ec.message().c_str(), op);
}
