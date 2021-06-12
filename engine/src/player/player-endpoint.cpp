// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./player-session.h"
#include "./player-endpoint.h"

#include "utilities/logging.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio.hpp>


using namespace std::placeholders;

#define LOG_WS(format, ...)          LOG_X(format, ##__VA_ARGS__)

static std::atomic_int debugCounter = 0;


PlayerEndpoint::PlayerEndpoint(std::shared_ptr<boost::asio::io_context> ioc) :
    ioc_{ioc},
    acceptor_{*ioc},
    socket_{*ioc}
{
  LOG_LIFECYCLE("%p PlayerEndpoint + %d", this, ++debugCounter);

  renderInterval_ = Strand::getMain()->setInterval([]() {
    Strand::getRender()->run();
  }, 100);
}


PlayerEndpoint::~PlayerEndpoint() {
  LOG_LIFECYCLE("%p PlayerEndpoint ~ %d", this, --debugCounter);
  LOG_ASSERT(shutdownCompleted());
  std::lock_guard lock{mutex_};
  LOG_ASSERT(!acceptor_.is_open());
  LOG_ASSERT(sessions_.empty());

  clearInterval(*renderInterval_);
}


unsigned short PlayerEndpoint::startup(unsigned short port) {
  boost::system::error_code ec;

  auto const address = boost::asio::ip::make_address("0.0.0.0");
  auto const ep = boost::asio::ip::tcp::endpoint{address, port};

  acceptor_.open(ep.protocol(), ec);
  if (ec) {
    return 0;
  }

  acceptor_.set_option(boost::asio::socket_base::reuse_address{true}, ec);
  if (ec)
    onError(ec, "set_option(reuse_address)");

  acceptor_.bind(ep, ec);
  if (ec) {
    onError(ec, "bind");
    return 0;
  }

  acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
  if (ec) {
    onError(ec, "listen");
    return 0;
  }


  if (acceptor_.is_open()) {
    port = acceptor_.local_endpoint().port();
    LOG_I("PlayerEndpoint listing on port %d", port);
    doAccept();
    return port;
  }

  return 0;
}


Promise<void> PlayerEndpoint::shutdown_() {
  LOG_LIFECYCLE("%p PlayerEndpoint shutdown", this);

  boost::system::error_code ec;
  acceptor_.close(ec); // TODO: make close async
  if (ec) {
    onError(ec, "_acceptor.close");
  }

  std::vector<Promise<void>> promises{};
  std::unique_lock lock{mutex_};
  for (const auto& session : sessions_) {
    promises.push_back(session->shutdown().onResolve<Promise<void>>([capture_until_done = session] {
      return Promise<void>{}.resolve();
    }));
  }
  lock.unlock();

  co_await PromiseUtils::all(promises);
}


void PlayerEndpoint::doAccept() {
  LOG_WS("PlayerEndpoint doAccept");
  acceptor_.async_accept(socket_, [this_ = shared_from_this()](auto ec) {
    this_->onAccept(ec);
  });
}


void PlayerEndpoint::onAccept(boost::system::error_code ec) {
  LOG_WS("PlayerEndpoint onAccept");
  if (ec) {
    onError(ec, "accent_async");
    socket_.close(ec);
    if (ec)
      onError(ec, "accent_async/close");
  } else {
    socket_.set_option(boost::asio::ip::tcp::no_delay{true}, ec);
    if (ec)
      onError(ec, "set_option(no_delay)");

    auto session = std::make_shared<PlayerSession>(*this, ioc_, std::move(socket_));
    std::unique_lock lock{mutex_};
    sessions_.push_back(session);
    lock.unlock();

    session->strand_->setImmediate([session]() {
      session->doAccept();
    });
  }
  if (acceptor_.is_open())
    doAccept();
}


void PlayerEndpoint::removeConnection(PlayerSession* session) {
  std::lock_guard lock{mutex_};
  sessions_.erase(
      std::remove_if(sessions_.begin(), sessions_.end(), [session](auto& x) {
        return x.get() == session;
      }),
      sessions_.end());
}


void PlayerEndpoint::onError(boost::system::error_code ec, const char* op) {
  if (ec != boost::asio::error::operation_aborted) {
    LOG_E("PlayerEndpoint, error: %s: %s", ec.message().c_str(), op);
  }
}
