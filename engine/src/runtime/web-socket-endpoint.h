// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__WEB_SOCKET_ENDPOINT_H
#define WARSTAGE__RUNTIME__WEB_SOCKET_ENDPOINT_H

#include "./endpoint.h"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <array>
#include <mutex>
#include <thread>

class Runtime;
class WebSocketSession;


class WebSocketEndpoint : public Endpoint {
  friend class WebSocketSession;

  using acceptor = boost::asio::ip::tcp::acceptor;
  using socket = boost::asio::ip::tcp::socket;
  using resolver = boost::asio::ip::tcp::resolver;

  std::shared_ptr<boost::asio::io_context> ioc_;
  acceptor acceptor_;
  socket socket_;
  resolver resolver_;

  std::vector<std::shared_ptr<WebSocketSession>> sessions_{};
  std::mutex mutex_{};

public:
  WebSocketEndpoint(Runtime& runtime, std::shared_ptr<boost::asio::io_context> ioc);
  ~WebSocketEndpoint() override;

  unsigned short startup_safe(unsigned short port = 0);

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

protected: // Endpoint
  [[nodiscard]] std::shared_ptr<Session> makeSession_safe(const std::string& url) override;

private:
  void doAccept_safe();
  void onAccept_safe(boost::system::error_code ec);

  static void logError(boost::system::error_code ec, const char* op);

  void removeConnection_safe(WebSocketSession* session);
};


#endif
