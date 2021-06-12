// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__PLAYER__PLAYER_WINDOW_H
#define WARSTAGE__PLAYER__PLAYER_WINDOW_H

#include "./player-backend.h"
#include "async/strand.h"
#include "graphics/graphics.h"
#include "runtime/web-socket-endpoint.h"
#include "gesture/pointer.h"
#include <memory>

class PlayerFrontend;
class Surface;
class Federate;
class Framebuffer;
class PlayerBackend;
class Renderbuffer;
class PlayerSession;
class Viewport;


class PlayerWindow :
    public Shutdownable,
    public std::enable_shared_from_this<PlayerWindow>
{
  std::unique_ptr<GraphicsApi> graphicsApi_{};
  std::unique_ptr<Graphics> graphics_{};
  std::shared_ptr<Runtime> runtime_{};
  std::shared_ptr<boost::asio::io_context> ioc_{};
  std::shared_ptr<WebSocketEndpoint> endpoint_{};
  std::unique_ptr<std::thread> ioThread_{};

  std::shared_ptr<PlayerBackend> playerBackend_{};
  std::shared_ptr<Viewport> viewport_{};
  std::shared_ptr<Surface> gestureSurface_{};
  std::shared_ptr<PlayerFrontend> playerFrontend_{};
  std::unique_ptr<Pointer> mouse_{};

  PlayerSession* surfaceSession_{};
  bool rendering_{};
  std::vector<Value> buffer_{};

public:
  PlayerWindow();
  ~PlayerWindow() override;

  void startup(std::shared_ptr<boost::asio::io_context> ioc, PlayerSession& surfaceSession);

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

public:
  void setServerUrl(const char* url);

  void renderFrame(int width, int height);
  bool shouldFlushBuffer() const;
  void flushBuffer();

  void mouseUpdate(float x, float y, int buttons, int count, double timestamp);
  void mouseWheel(float x, float y, float dx, float dy);

  void keyDown(char key);
  void keyUp(char key);
};


#endif
