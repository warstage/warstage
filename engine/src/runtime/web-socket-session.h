// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__WEB_SOCKET_SESSION_H
#define WARSTAGE__RUNTIME__WEB_SOCKET_SESSION_H

#include "./session.h"
#include "value/compressor.h"
#include "value/decompressor.h"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <array>
#include <thread>

class Session;
class WebSocketEndpoint;


class WebSocketSession : public Session {
  friend class WebSocketEndpoint;

  using error_code = boost::system::error_code;
  using socket = boost::asio::ip::tcp::socket;
  using stream = boost::beast::websocket::stream<socket>;
  using strand = boost::asio::strand<stream::executor_type>;
  using resolver = boost::asio::ip::tcp::resolver;
  using resolver_t = boost::asio::ip::tcp::resolver;

  static constexpr std::chrono::duration PingTimeout = std::chrono::seconds(15);
  static constexpr int ShutdownTimeoutMilliseconds = 2500;

  std::weak_ptr<WebSocketEndpoint> endpoint_{};
  std::shared_ptr<boost::asio::io_context> ioc_{};
  stream stream_;
  boost::beast::multi_buffer readBuffer_{};
  std::string decodeBuffer_{};
  std::string writeBuffer_{};
  std::vector<std::string> writeQueue_{};
  std::mutex writeMutex_{};
  boost::asio::steady_timer pingTimer_;
  char pingState_ = 0;
  std::string host_{};
  ValueCompressor compressor_{};
  ValueDecompressor decompressor_{};

public:
  WebSocketSession(std::shared_ptr<boost::asio::io_context> ioc, WebSocketEndpoint& endpoint, socket socket);
  WebSocketSession(std::shared_ptr<boost::asio::io_context> ioc, WebSocketEndpoint& endpoint);
  ~WebSocketSession() override;

  [[nodiscard]] auto getStrand_() const {
    return dynamic_cast<Strand_Asio&>(getStrand()).shared_from_this();
  }

  [[nodiscard]] auto getAsioStrand() const {
    return dynamic_cast<Strand_Asio&>(getStrand()).strand_;
  }

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

public:
  void onTimer(error_code ec);
  void activity();
  void onPing(error_code ec);
  void setControlCallback();


  void doResolve(resolver_t& resolver, const char* host, const char* port);
  void onResolve(boost::system::error_code ec, resolver_t::results_type results);
  void onConnect(boost::system::error_code ec);
  void onHandshake(boost::system::error_code ec);


  void doAccept();
  void onAccept(error_code ec);

  void doRead();
  void onRead(error_code ec, std::size_t bytes_transferred);

  void doWrite(const Value& packet);
  void tryWrite();
  void onWrite(error_code ec, std::size_t bytes_transferred);

  void onError(error_code ec, const char* op);
  static void logError(error_code ec, const char* op);
  static bool isExpectedError(error_code ec);

protected:
  void sendPacketImpl_strand(const Value& packet) override;
};


#endif
