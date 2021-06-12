// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__PLAYER__PLAYER_SESSION_H
#define WARSTAGE__PLAYER__PLAYER_SESSION_H

#include "value/value.h"
#include "runtime/runtime.h"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <array>
#include <thread>

class Session;
class PlayerEndpoint;
class PlayerWindow;


class PlayerSession :
    public Shutdownable,
    public std::enable_shared_from_this<PlayerSession>
{
  friend class PlayerEndpoint;

  using error_code_t = boost::system::error_code;
  using socket_t = boost::asio::ip::tcp::socket;
  using stream_t = boost::beast::websocket::stream<socket_t>;
  using strand_t = boost::asio::strand<stream_t::executor_type>;
  using resolver_t = boost::asio::ip::tcp::resolver;

  // strand thread
  std::weak_ptr<PlayerEndpoint> endpoint_{};
  std::shared_ptr<boost::asio::io_context> ioc_{};
  stream_t stream_;
  std::shared_ptr<Strand_Asio> strand_;
  boost::beast::multi_buffer readBuffer_;
  std::string writeBuffer_{};
  std::vector<std::string> writeQueue_{};
  std::mutex writeQueueMutex_{};
  boost::asio::steady_timer pingTimer_;
  char pingState_{};

  // main thread
  std::unique_ptr<PlayerWindow> surfaceAdapter_{};

  // any thread
  std::vector<Value> messageQueue_{};
  std::mutex messageMutex_{};

public:
  PlayerSession(PlayerEndpoint& endpoint, std::shared_ptr<boost::asio::io_context> ioc, socket_t socket);
  ~PlayerSession() override;

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

public:
  void onTimer(error_code_t ec);
  void activity();
  void onPing(error_code_t ec);
  void setControlCallback();

  void doAccept();
  void onAccept(error_code_t ec);

  void doRead();
  void onRead(error_code_t ec);

  void doWrite(const void* data, std::size_t size);
  void tryWrite();
  void onWrite(error_code_t ec);

  void onError(error_code_t ec, const char* op);
  static void logError(error_code_t ec, const char* op);

protected:
  [[nodiscard]] Promise<void> createSurfaceAdapter();

  void enqueueMessage(const Value& message);
  void processMessageQueue();
};


#endif
