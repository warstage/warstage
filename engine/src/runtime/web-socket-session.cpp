// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./session.h"
#include "./runtime.h"
#include "./web-socket-session.h"
#include "./web-socket-endpoint.h"
#include "utilities/logging.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio.hpp>

using namespace std::placeholders;

#define LOG_TRACE(format, ...)          LOG_X(format, ##__VA_ARGS__)

#ifdef WARSTAGE_ENABLE_WEBSOCKET_ERROR_MONKEY
thread_local int webSocketErrorMonkeyCountdown;
static void WebSocketErrorMonkey(boost::system::error_code& ec) {
    switch (webSocketErrorMonkeyCountdown) {
        case 1:
            ec.assign(53, boost::system::generic_category());
            // fall thru to case 0
        case 0:
            webSocketErrorMonkeyCountdown = 128 + static_cast<int>(static_cast<unsigned int>(std::rand()) & 255u);
            break;
        default:
            --webSocketErrorMonkeyCountdown;
            break;
    }
}
#else
inline void WebSocketErrorMonkey(const boost::system::error_code&) {
}
#endif


WebSocketSession::WebSocketSession(std::shared_ptr<boost::asio::io_context> ioc, WebSocketEndpoint& endpoint, WebSocketSession::socket socket) :
    Session{endpoint, std::make_shared<Strand_Asio>(ioc->get_executor(), "WebSocketSession")},
    endpoint_{std::static_pointer_cast<WebSocketEndpoint>(endpoint.shared_from_this())},
    ioc_{std::move(ioc)},
    stream_{std::move(socket)},
    pingTimer_{*ioc_, std::chrono::steady_clock::time_point::max()}
{
  LOG_LIFECYCLE("%p WebSocketSession +", this);
  LOG_TRACE("WebSocketSession %p", this);
  stream_.binary(true);
}


WebSocketSession::WebSocketSession(std::shared_ptr<boost::asio::io_context> ioc, WebSocketEndpoint& endpoint) :
    Session{endpoint, std::make_shared<Strand_Asio>(ioc->get_executor(), "WebSocketSession")},
    endpoint_{std::static_pointer_cast<WebSocketEndpoint>(endpoint.shared_from_this())},
    ioc_{std::move(ioc)},
    stream_{*ioc_},
    pingTimer_{*ioc_, std::chrono::steady_clock::time_point::max()}
{
  LOG_LIFECYCLE("%p WebSocketSession +", this);
  LOG_TRACE("WebSocketSession %p", this);
  stream_.binary(true);
}


WebSocketSession::~WebSocketSession() {
  LOG_LIFECYCLE("%p WebSocketSession ~", this);
  LOG_TRACE("~WebSocketSession %p", this);
}


Promise<void> WebSocketSession::shutdown_() {
  LOG_LIFECYCLE("%p WebSocketSession Shutdown", this);
  LOG_TRACE("WebSocketSession %p Shutdown", this);

  co_await *strand_;

  error_code ec;
  pingTimer_.cancel(ec);
  WebSocketErrorMonkey(ec);
  if (ec) {
    logError(ec, "cancel");
  }

  auto endpoint = endpoint_.lock();
  LOG_ASSERT(endpoint);
  endpoint->onSessionClosed_safe(*this);
  endpoint->removeConnection_safe(this);
  endpoint_.reset();

  if (stream_.is_open()) {
    Promise<void> deferred{};
    stream_.async_close({}, boost::asio::bind_executor(getAsioStrand(), [strand = getStrand_(), deferred](auto ec) {
      Strand_Asio::SetCurrent current{strand};
      WebSocketErrorMonkey(ec);
      if (ec) {
        logError(ec, "async_close");
      }
      deferred.resolve().done();
    }));
    getStrand().setTimeout([deferred]() {
      if (!deferred.isFulfilled()) {
        deferred.resolve().done();
      }
    }, ShutdownTimeoutMilliseconds);
    co_await deferred;
  }

  if (stream_.next_layer().is_open()) {
    error_code ec;
    stream_.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    WebSocketErrorMonkey(ec);
    if (ec) {
      logError(ec, "shutdown");
    }
    stream_.next_layer().close(ec);
    WebSocketErrorMonkey(ec);
    if (ec) {
      logError(ec, "close");
    }
  }

  co_await Session::shutdown_();
}


void WebSocketSession::onTimer(error_code ec) {
  // LOG_ASSERT(GetStrand().Assert()); ???

  if (ec && (ec != boost::asio::error::operation_aborted || shutdownStarted()))
    return shutdownStarted() ? logError(ec, "timer") : onError(ec, "timer");

  if (std::chrono::steady_clock::now() >= pingTimer_.expiry()) {
    if(stream_.is_open() && pingState_ == 0) {
      pingState_ = 1;
      pingTimer_.expires_after(PingTimeout);

      //LOG_TRACE("WebSocketSession %p async_ping", this);
      stream_.async_ping({}, boost::asio::bind_executor(getAsioStrand(), [weak_ = weak_from_this()](auto ec) {
        if (auto this_ = std::static_pointer_cast<WebSocketSession>(weak_.lock())) {
          Strand_Asio::SetCurrent current{this_->getStrand_()};
          this_->onPing(ec);
        }
      }));
    } else {
      stream_.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
      WebSocketErrorMonkey(ec);
      return logError(ec, "shutdown");
    }
  }

  pingTimer_.async_wait( boost::asio::bind_executor(getAsioStrand(), [weak_ = weak_from_this()](auto ec) {
    if (auto this_ = std::static_pointer_cast<WebSocketSession>(weak_.lock())) {
      Strand_Asio::SetCurrent current{this_->getStrand_()};
      WebSocketErrorMonkey(ec);
      this_->onTimer(ec);
    }
  }));
}


void WebSocketSession::activity() {
  LOG_ASSERT(getStrand().isCurrent());

  pingState_ = 0;
  pingTimer_.expires_after(PingTimeout);
}


void WebSocketSession::onPing(error_code ec) {
  LOG_ASSERT(getStrand().isCurrent());

  if (endpoint_.expired() || ec)
    return onError(ec, "ping");

  if (pingState_ == 1) {
    pingState_ = 2;
  } else {
    // ping_state_ could have been set to 0
    // if an incoming control frame was received
    // at exactly the same time we sent a ping.
    LOG_ASSERT(pingState_ == 0);
  }
}


void WebSocketSession::setControlCallback() {
  // LOG_ASSERT(GetStrand().Assert()); ???

  stream_.control_callback([weak_ = weak_from_this()](boost::beast::websocket::frame_type, boost::beast::string_view) {
    if (auto this_ = std::static_pointer_cast<WebSocketSession>(weak_.lock())) {
      Strand_Asio::SetCurrent current{this_->getStrand_()};
      this_->activity();
    }
  });
}


void WebSocketSession::doResolve(resolver_t& resolver, const char* host, const char* port) {
  // LOG_ASSERT(GetStrand().Assert()); ???

  LOG_TRACE("WebSocketSession %p async_resolve(%s, %s)", this, host, port);
  host_ = host;
  resolver.async_resolve(host, port, boost::asio::bind_executor(getAsioStrand(), [this_ = std::static_pointer_cast<WebSocketSession>(shared_from_this())](auto ec, auto results) {
    Strand_Asio::SetCurrent current{this_->getStrand_()};
    WebSocketErrorMonkey(ec);
    this_->onResolve(ec, results);
  }));
}


void WebSocketSession::onResolve(error_code ec, boost::asio::ip::basic_resolver<boost::asio::ip::tcp>::results_type results) {
  LOG_ASSERT(getStrand().isCurrent());

  if (endpoint_.expired() || ec) {
    return onError(ec, "async_resolve");
  }

  LOG_TRACE("WebSocketSession %p async_connect", this);
  auto handler = boost::asio::bind_executor(getAsioStrand(), [this_ = std::static_pointer_cast<WebSocketSession>(shared_from_this())](auto ec, auto x) {
    Strand_Asio::SetCurrent current{this_->getStrand_()};
    WebSocketErrorMonkey(ec);
    this_->onConnect(ec);
  });
  boost::asio::async_connect(stream_.next_layer(), results.begin(), results.end(), std::move(handler));
}


void WebSocketSession::onConnect(error_code ec) {
  LOG_ASSERT(getStrand().isCurrent());

  if (endpoint_.expired() || ec)
    return onError(ec, "async_connect");

  stream_.next_layer().set_option(boost::asio::ip::tcp::no_delay{true}, ec);
  WebSocketErrorMonkey(ec);
  if (ec) {
    logError(ec, "set_option(no_delay)");
  }

  stream_.set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
    req.insert(boost::beast::http::field::sec_websocket_protocol, "warstage");
  }));

  LOG_TRACE("WebSocketSession %p async_handshake_ex(%s, '/')", this, host_.c_str());
  auto handler = boost::asio::bind_executor(getAsioStrand(), [this_ = std::static_pointer_cast<WebSocketSession>(shared_from_this())](auto ec) {
    Strand_Asio::SetCurrent current{this_->getStrand_()};
    WebSocketErrorMonkey(ec);
    this_->onHandshake(ec);
  });
  stream_.async_handshake(host_, "/", std::move(handler));
}


void WebSocketSession::onHandshake(error_code ec) {
  LOG_ASSERT(getStrand().isCurrent());

  if (endpoint_.expired() || ec) {
    return onError(ec, "async_handshake");
  }

  setControlCallback();
  onTimer({});

  doRead();
}


void WebSocketSession::doAccept() {
  // LOG_ASSERT(GetStrand().Assert()); ???

  LOG_TRACE("WebSocketSession %p doAccept", this);
  setControlCallback();
  onTimer({});
  pingTimer_.expires_after(PingTimeout);

  LOG_TRACE("WebSocketSession %p async_accept", this);
    stream_.set_option(boost::beast::websocket::stream_base::decorator(
            [] (boost::beast::websocket::response_type& res) {
                res.insert(boost::beast::http::field::sec_websocket_protocol, "warstage");
            }
    ));
  auto handler = boost::asio::bind_executor(getAsioStrand(), [this_ = std::static_pointer_cast<WebSocketSession>(shared_from_this())](auto ec) {
    Strand_Asio::SetCurrent current{this_->getStrand_()};
    WebSocketErrorMonkey(ec);
    this_->onAccept(ec);
  });
  stream_.async_accept(std::move(handler));
}


void WebSocketSession::onAccept(error_code ec) {
  LOG_ASSERT(getStrand().isCurrent());

  LOG_TRACE("WebSocketSession %p onAccept", this);

  if (endpoint_.expired() || ec) {
    return onError(ec, "async_accept");
  }

  doRead();

  if (runtime_->getProcessType() == ProcessType::Daemon) {
    LOG_TRACE("WebSocketSession %p SendHandshake", this);
    sendHandshake_strand();
  }
}


void WebSocketSession::doRead() {
  LOG_ASSERT(getStrand().isCurrent());

  //LOG_TRACE("WebSocketSession %p DoRead", this);
  auto handler = boost::asio::bind_executor(getAsioStrand(), [weak_ = weak_from_this()](auto ec, auto n) {
    if (auto this_ = std::static_pointer_cast<WebSocketSession>(weak_.lock())) {
      Strand_Asio::SetCurrent current{this_->getStrand_()};
      WebSocketErrorMonkey(ec);
      this_->onRead(ec, n);
    }
  });
  stream_.async_read(readBuffer_, std::move(handler));
}


void WebSocketSession::onRead(error_code ec, std::size_t bytes_transferred) {
  LOG_ASSERT(getStrand().isCurrent());

  if (endpoint_.expired() || ec) {
    return onError(ec, "async_read");
  }

  activity();

  decodeBuffer_.clear();
  {
    auto i = boost::asio::buffer_sequence_begin(readBuffer_.data());
    auto j = boost::asio::buffer_sequence_end(readBuffer_.data());
    while (i != j) {
      auto p = static_cast<const char*>((*i).data());
      auto q = p + (*i).size();
      decodeBuffer_.insert(decodeBuffer_.end(), p, q);
      ++i;
    }
    readBuffer_.consume(decodeBuffer_.size());
  }

  if (decompressor_.decode(decodeBuffer_.data(), decodeBuffer_.size())) {
    auto data = reinterpret_cast<const char*>(decompressor_.data());
    auto size = decompressor_.size();
    auto buffer = std::make_shared<ValueBuffer>(std::string{data, size});
    receivePacket_strand(Value{buffer});
    doRead();
  } else {
    onError(ec, "decompressor_decode");
  }
}


void WebSocketSession::doWrite(const Value& packet) {
  LOG_ASSERT(getStrand().isCurrent());

  std::lock_guard lock{writeMutex_};

  compressor_.encode(packet);
  // if (auto analytics = runtime_->getAnalytics()) {
  //   analytics->sampleCompressor(packet.size(), compressor_.size());
  // }

  auto p = static_cast<const char*>(compressor_.data());
  auto q = p + compressor_.size();
  writeQueue_.emplace_back(p, q);
  tryWrite();
}


void WebSocketSession::tryWrite() {
  LOG_ASSERT(getStrand().isCurrent());

  // NOTE: requires _writeMutex lock
  if (writeBuffer_.empty() && !writeQueue_.empty()) {
    writeBuffer_ = std::move(writeQueue_.front());
    writeQueue_.erase(writeQueue_.begin());

    auto handler = boost::asio::bind_executor(getAsioStrand(), [weak_ = weak_from_this()](auto ec, auto n) {
      if (auto this_ = std::static_pointer_cast<WebSocketSession>(weak_.lock())) {
        Strand_Asio::SetCurrent current{this_->getStrand_()};
        WebSocketErrorMonkey(ec);
        this_->onWrite(ec, n);
      }
    });
    stream_.async_write(boost::asio::buffer(writeBuffer_), std::move(handler));
  }
}


void WebSocketSession::onWrite(error_code ec, std::size_t bytes_transferred) {
  LOG_ASSERT(getStrand().isCurrent());

  if (endpoint_.expired() || ec) {
    return onError(ec, "async_write");
  }

  std::lock_guard lock{writeMutex_};
  writeBuffer_.clear();
  tryWrite();
}


void WebSocketSession::onError(error_code ec, const char* op) {
  LOG_ASSERT(getStrand().isCurrent());

  if (!shutdownStarted()) {
    if (!endpoint_.expired() && ec != boost::asio::error::operation_aborted) {
      logError(ec, op);
    }

    stream_.next_layer().close(ec);
    WebSocketErrorMonkey(ec);
    if (ec) {
      logError(ec, "close");
    }

    auto this_ = shared_from_this();
    shutdown().onReject<void>([this_](const std::exception_ptr& reason) {
      LOG_REJECTION(reason);
    }).done();
  } else {
    logError(ec, op);
  }
}


void WebSocketSession::logError(WebSocketSession::error_code ec, const char* op) {
  if (isExpectedError(ec)) {
    LOG_D("WebSocketSession %s: %s (%s)", op, ec.message().c_str(), ec.category().name());
  } else {
    LOG_E("WebSocketSession %s: %s (%s)", op, ec.message().c_str(), ec.category().name());
  }
}


bool WebSocketSession::isExpectedError(WebSocketSession::error_code ec) {
  return ec == boost::system::errc::success
      || ec == boost::system::errc::operation_canceled
      || ec == boost::asio::error::eof
      || ec == boost::asio::error::connection_reset
      || ec == boost::beast::websocket::error::closed;
}


/***/


void WebSocketSession::sendPacketImpl_strand(const Value& packet) {
  doWrite(packet);
}
