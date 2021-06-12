// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./player-window.h"
#include "./player-session.h"
#include "./player-backend.h"
#include "./player-frontend.h"

#include "async/strand.h"
#include "gesture/surface.h"
#include "gesture/gesture.h"
#include "graphics/framebuffer.h"
#include "graphics/graphics.h"
#include "graphics/renderbuffer.h"
#include "graphics/viewport.h"
#include "runtime/web-socket-endpoint.h"
#include "runtime/session.h"
#include "value/value.h"

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

static std::atomic_int debugCounter = 0;


PlayerWindow::PlayerWindow() {
    LOG_LIFECYCLE("%p WorkshopSurfaceAdapter + %d", this, ++debugCounter);
}


PlayerWindow::~PlayerWindow() {
    LOG_LIFECYCLE("%p WorkshopSurfaceAdapter ~ %d", this, --debugCounter);
    LOG_ASSERT(shutdownCompleted());
}


void PlayerWindow::startup(std::shared_ptr<boost::asio::io_context> ioc, PlayerSession& surfaceSession) {
    runtime_ = std::make_shared<Runtime>(ProcessType::Player);
    runtime_->registerProcessAuth_safe(runtime_->getProcessId(), {"_"});

    runtime_->registerProcess_safe(ObjectId{}, ProcessType::Headup, nullptr);
    runtime_->registerProcessAuth_safe(ObjectId{}, ProcessAuth{"_"});

    endpoint_ = std::make_shared<WebSocketEndpoint>(*runtime_, ioc);
    endpoint_->setSessionClosedHandler([](const auto& session) -> void {
        LOG_W("Session %s closed", str(session.getProcessType()));
    });
    int port = endpoint_->startup_safe(0);

    playerBackend_ = std::make_shared<PlayerBackend>(*runtime_);
    playerBackend_->startup();

    surfaceSession_ = &surfaceSession;
    graphicsApi_ = std::make_unique<GraphicsApi>([this](const Value& message) {
        buffer_.push_back(message);
        if (shouldFlushBuffer())
            flushBuffer();
    });

    graphics_ = std::make_unique<Graphics>(*graphicsApi_);

    viewport_ = std::make_shared<Viewport>(graphics_.get(), 1);
    gestureSurface_ = std::make_shared<Surface>(*viewport_);
    playerFrontend_ = std::make_shared<PlayerFrontend>(*runtime_, *gestureSurface_, *viewport_);
    playerFrontend_->startup();

    buffer_.push_back(build_array() << "Startup" << port << ValueEnd{});
    flushBuffer();
}


Promise<void> PlayerWindow::shutdown_() {
    LOG_LIFECYCLE("%p WorkshopSurfaceAdapter Shutdown", this);

    if (playerFrontend_) {
        co_await playerFrontend_->shutdown();
    }

    gestureSurface_ = nullptr;
    viewport_ = nullptr;
    graphics_ = nullptr;
    graphicsApi_ = nullptr;

    co_await playerBackend_->shutdown();
    co_await endpoint_->shutdown();
    co_await runtime_->shutdown();
}


void PlayerWindow::setServerUrl(const char* url) {
    endpoint_->setMasterUrl_safe(url);
}


void PlayerWindow::renderFrame(int width, int height) {
    Strand::getRender()->run();
    Strand::getRender()->setImmediate([this, width, height]() {
        rendering_ = true;
        graphics_->getGraphicsApi().beginFrame(0);
        viewport_->setViewportBounds(bounds2i{0, 0, width, height});
        playerFrontend_->animateSurface(bounds2i{0, 0, width, height}, viewport_->getScaling());
        playerFrontend_->renderSurface(nullptr);
        graphics_->getGraphicsApi().endFrame();
        rendering_ = false;
        if (shouldFlushBuffer())
            flushBuffer();
    });
    Strand::getRender()->run();
    Strand::getMain()->setImmediate([gestures_weak = std::weak_ptr<Surface>(gestureSurface_)]() {
        if (auto gestures = gestures_weak.lock())
            for (auto gestureRecognizer : gestures->GetGestures())
                gestureRecognizer->Animate();
    });
}


bool PlayerWindow::shouldFlushBuffer() const {
    if (buffer_.empty())
        return false;
    if (rendering_) {
        std::size_t size = 0;
        for (auto& value : buffer_)
            size += value.size();
        return size > 4096;
    }

    return true;
}

void PlayerWindow::flushBuffer() {
    auto builder = Struct{} << "_" << Array{};
    for (auto& value : buffer_)
        builder = std::move(builder) << value;
    auto message = std::move(builder) << ValueEnd{} << ValueEnd{};
    surfaceSession_->doWrite(message.data(), message.size());
    buffer_.clear();
}


void PlayerWindow::mouseUpdate(float x, float y, int buttons, int count, double timestamp) {
    if (!mouse_ && buttons == 0)
        return;

    const int clickCount = 1;
    glm::vec2 position{x, y};
    MouseButtons mouseButtons;
    mouseButtons.left = (buttons & 1) != 0;
    mouseButtons.right = (buttons & 2) != 0;
    mouseButtons.other = (buttons & 4) != 0;

    if (!mouse_) {
        mouse_ = std::make_unique<Pointer>(PointerType::Mouse, count, position, 0.001 * timestamp, mouseButtons);
        for (auto gesture : gestureSurface_->GetGestures()) {
            gesture->PointerWillBegin(mouse_.get());
        }
        mouse_->TouchBegan();
    } else if (!mouseButtons.Any()) {
        mouse_->Update(position, 0.001 * timestamp, mouseButtons);
        mouse_->TouchEnded();
        mouse_ = nullptr;
    } else {
        double oldTimestamp = mouse_->GetTimestamp();
        mouse_->Update(position, 0.001 * timestamp, mouseButtons);
        if (mouse_->GetTimestamp() != oldTimestamp)
            mouse_->TouchMoved();
    }
}


void PlayerWindow::mouseWheel(float x, float y, float dx, float dy) {
    glm::vec2 position{x, y};
    glm::vec2 delta = 0.01f * glm::vec2{dx, dy};

    for (auto gestureRecognizer : gestureSurface_->GetGestures())
        gestureRecognizer->ScrollWheel(position, delta);
}


void PlayerWindow::keyDown(char key) {
    for (auto gestureRecognizer : gestureSurface_->GetGestures())
        gestureRecognizer->KeyDown(key);
}


void PlayerWindow::keyUp(char key) {
    for (auto gestureRecognizer : gestureSurface_->GetGestures())
        gestureRecognizer->KeyUp(key);
}

