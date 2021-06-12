// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__PLAYER__PLAYER_ENDPOINT_H
#define WARSTAGE__PLAYER__PLAYER_ENDPOINT_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <array>
#include <mutex>
#include <thread>

class PlayerSession;


class PlayerEndpoint :
    public Shutdownable,
    public std::enable_shared_from_this<PlayerEndpoint>
{
  friend class PlayerSession;

  using acceptor_t = boost::asio::ip::tcp::acceptor;
  using socket_t = boost::asio::ip::tcp::socket;

  std::shared_ptr<boost::asio::io_context> ioc_{};
  acceptor_t acceptor_;
  socket_t socket_;

  std::vector<std::shared_ptr<PlayerSession>> sessions_{};
  std::mutex mutex_{};

  std::shared_ptr<IntervalObject> renderInterval_{};

public:
  explicit PlayerEndpoint(std::shared_ptr<boost::asio::io_context> ioc);
  ~PlayerEndpoint() override;

  unsigned short startup(unsigned short port = 0);

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

private:
  void doAccept();
  void onAccept(boost::system::error_code ec);

  void onError(boost::system::error_code ec, const char* op);

  void removeConnection(PlayerSession* session);
};


#endif
