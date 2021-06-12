// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "player-window.h"
#include "player-session.h"
#include "player-endpoint.h"
#include "utilities/logging.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio.hpp>

#define LOG_WS(format, ...)          LOG_X(format, ##__VA_ARGS__)

static std::atomic_int debugCounter = 0;


PlayerSession::PlayerSession(PlayerEndpoint& endpoint, std::shared_ptr<boost::asio::io_context> ioc, PlayerSession::socket_t socket) :
        endpoint_{endpoint.weak_from_this()},
        ioc_{std::move(ioc)},
        stream_{std::move(socket)},
        strand_{std::make_shared<Strand_Asio>(stream_.get_executor(), "WebSocketSurfaceSession")},
        pingTimer_{*ioc_, std::chrono::steady_clock::time_point::max()}
{
    LOG_LIFECYCLE("%p PlayerSession + %d", this, ++debugCounter);
    stream_.binary(true);
}


PlayerSession::~PlayerSession() {
    LOG_LIFECYCLE("%p PlayerSession ~ %d", this, --debugCounter);
    LOG_ASSERT(shutdownCompleted());
}


Promise<void> PlayerSession::shutdown_() {
    LOG_LIFECYCLE("%p PlayerSession Shutdown", this);
    LOG_ASSERT(strand_->isCurrent());

    if (!endpoint_.expired()) {
        if (auto endpoint = endpoint_.lock()) {
            endpoint->removeConnection(this);
        }
        endpoint_.reset();
    }

    error_code_t ec;
    pingTimer_.cancel(ec);
    if (ec) {
        logError(ec, "cancel");
    }

    if (stream_.is_open()) {
        Promise<void> deferred{};
        stream_.async_close({}, boost::asio::bind_executor(strand_->strand_, [this_ = shared_from_this(), deferred](auto ec) {
            Strand_Asio::SetCurrent current{this_->strand_};
            if (ec) {
                logError(ec, "async_close");
            }
            deferred.resolve().done();
        }));
        co_await deferred;
    }

    co_await *Strand::getMain();
    LOG_ASSERT(Strand::getMain()->isCurrent());
    if (surfaceAdapter_) {
        co_await surfaceAdapter_->shutdown();
        surfaceAdapter_ = nullptr;
    }
}


void PlayerSession::onTimer(error_code_t ec) {
    LOG_ASSERT(strand_->isCurrent());

    if (ec && (ec != boost::asio::error::operation_aborted || shutdownStarted())) {
        return shutdownStarted() ? logError(ec, "timer") : onError(ec, "timer");
    }

    if (std::chrono::steady_clock::now() >= pingTimer_.expiry()) {
        if(stream_.is_open() && pingState_ == 0) {
            pingState_ = 1;
            pingTimer_.expires_after(std::chrono::seconds(15));

            LOG_WS("PlayerSession %p async_ping", this);
            stream_.async_ping({}, boost::asio::bind_executor(strand_->strand_, [this_ = shared_from_this()](auto ec) {
                Strand_Asio::SetCurrent current{this_->strand_};
                this_->onPing(ec);
            }));
        } else {
            stream_.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            return onError(ec, "shutdown");
        }
    }

    pingTimer_.async_wait(boost::asio::bind_executor(strand_->strand_, [this_ = shared_from_this()](auto ec) {
        Strand_Asio::SetCurrent current{this_->strand_};
        this_->onTimer(ec);
    }));
}


void PlayerSession::activity() {
    LOG_ASSERT(strand_->isCurrent());

    pingState_ = 0;
    pingTimer_.expires_after(std::chrono::seconds(15));
}


void PlayerSession::onPing(error_code_t ec) {
    LOG_ASSERT(strand_->isCurrent());

    if (endpoint_.expired() || ec) {
        return onError(ec, "ping");
    }

    if (pingState_ == 1) {
        pingState_ = 2;
    } else {
        // ping_state_ could have been set to 0
        // if an incoming control frame was received
        // at exactly the same time we sent a ping.
        LOG_ASSERT(pingState_ == 0);
    }
}


void PlayerSession::setControlCallback() {
    LOG_ASSERT(strand_->isCurrent());

    auto weak_ = weak_from_this();
    stream_.control_callback([weak_](boost::beast::websocket::frame_type, boost::beast::string_view) {
        if (auto this_ = weak_.lock()) {
            Strand_Asio::SetCurrent current{this_->strand_};
            this_->activity();
        }
    });
}


void PlayerSession::doAccept() {
    LOG_ASSERT(strand_->isCurrent());

    setControlCallback();
    onTimer({});
    pingTimer_.expires_after(std::chrono::seconds(15));

    LOG_WS("PlayerSession %p async_accept", this);

    stream_.set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::response_type& hdr) {
        hdr.set( boost::beast::http::field::sec_websocket_protocol, "warstage-player");
    }));

    // stream_.set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::response_type& req) {
    //     req.insert(boost::beast::http::field::sec_websocket_protocol, "warstage-player");
    // }));

    stream_.async_accept(boost::asio::bind_executor(strand_->strand_, [this_ = shared_from_this()](auto ec) {
        Strand_Asio::SetCurrent current{this_->strand_};
        this_->onAccept(ec);
    }));
}


void PlayerSession::onAccept(error_code_t ec) {
    LOG_ASSERT(strand_->isCurrent());

    if (endpoint_.expired() || ec) {
        return onError(ec, "async_accept");
    }

    auto this_ = shared_from_this();
    createSurfaceAdapter().onResolve<void>(strand_, [this_]() {
        this_->doRead();
    }).done();
}


void PlayerSession::doRead() {
    LOG_ASSERT(strand_->isCurrent());

    std::lock_guard lock{writeQueueMutex_};
    stream_.async_read(readBuffer_, boost::asio::bind_executor(strand_->strand_, [this_ = shared_from_this()](auto ec, auto) {
        Strand_Asio::SetCurrent current{this_->strand_};
        this_->onRead(ec);
    }));
}


void PlayerSession::onRead(error_code_t ec) {
    LOG_ASSERT(strand_->isCurrent());

    if (endpoint_.expired() || ec) {
        return onError(ec, "async_read");
    }

    activity();

    auto buffer = std::make_shared<ValueBuffer>();
    std::unique_lock lock{writeQueueMutex_};
    const auto d = readBuffer_.data();
    auto i = boost::asio::buffer_sequence_begin(d);
    auto j = boost::asio::buffer_sequence_end(d);
    while (i != j) {
        auto p = static_cast<const char*>((*i).data());
        auto q = p + (*i).size();
        buffer->value_.insert(buffer->value_.end(), p, q);
        ++i;
    }
    readBuffer_.consume(buffer->value_.size());
    lock.unlock();

    enqueueMessage(Value{std::move(buffer)});
    doRead();
}


void PlayerSession::doWrite(const void* data, std::size_t size) {
    {
        std::lock_guard lock{writeQueueMutex_};
        auto p = static_cast<const char*>(data);
        auto q = p + size;
        writeQueue_.emplace_back(p, q);
    }
    tryWrite();
}


void PlayerSession::tryWrite() {
    std::lock_guard lock{writeQueueMutex_};
    if (writeBuffer_.empty() && !writeQueue_.empty()) {
        writeBuffer_ = std::move(writeQueue_.front());
        writeQueue_.erase(writeQueue_.begin());

        stream_.async_write(boost::asio::buffer(writeBuffer_), boost::asio::bind_executor(strand_->strand_, [this_ = shared_from_this()](auto ec, auto) {
            Strand_Asio::SetCurrent current{this_->strand_};
            this_->onWrite(ec);
        }));
    }
}


void PlayerSession::onWrite(error_code_t ec) {
    LOG_ASSERT(strand_->isCurrent());

    if (endpoint_.expired() || ec) {
        return onError(ec, "async_write");
    }

    std::unique_lock lock{writeQueueMutex_};
    writeBuffer_.clear();
    lock.unlock();

    tryWrite();
}


void PlayerSession::onError(error_code_t ec, const char* op) {
    LOG_ASSERT(strand_->isCurrent());

    if (!shutdownStarted()) {
        if (ec != boost::asio::error::operation_aborted) {
            logError(ec, op);
        }

        stream_.next_layer().close(ec);
        if (ec) {
            logError(ec, "close");
        }

        auto this_ = shared_from_this();
        shutdown().onReject<void>([capture_until_done = this_](const std::exception_ptr& reason) {
            LOG_REJECTION(reason);
        }).done();
    }
}


void PlayerSession::logError(PlayerSession::error_code_t ec, const char* op) {
    LOG_E("PlayerSession %s: %s (%s)", op, ec.message().c_str(), ec.category().name());
}


/***/


Promise<void> PlayerSession::createSurfaceAdapter() {
    Promise<void> deferred{};
    Strand::getMain()->setImmediate([this_ = shared_from_this(), deferred]() {
        this_->surfaceAdapter_ = std::make_unique<PlayerWindow>();
        this_->surfaceAdapter_->startup(this_->ioc_, *this_);
        deferred.resolve().done();
    });
    return deferred;
}


void PlayerSession::enqueueMessage(const Value& message) {
    std::lock_guard lock{messageMutex_};
    messageQueue_.push_back(message);
    Strand::getMain()->setImmediate([this_ = shared_from_this()]() {
        this_->processMessageQueue();
    });
}


void PlayerSession::processMessageQueue() {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    if (!surfaceAdapter_) {
        return;
    }
    std::vector<Value> messages{};
    std::unique_lock lock{messageMutex_};
    std::swap(messageQueue_, messages);
    lock.unlock();

    for (const auto& message : messages) {
        switch (message["op"_int]) {
            case 1: { // set master url
                surfaceAdapter_->setServerUrl(
                        message["url"_c_str]
                );
                break;
            }
            case 2: { // render frame
                surfaceAdapter_->renderFrame(
                        message["width"_int],
                        message["height"_int]);
                break;
            }
            case 3: { // mouse update
                surfaceAdapter_->mouseUpdate(
                        message["x"_float],
                        message["y"_float],
                        message["buttons"_int],
                        message["count"_int],
                        message["timestamp"_double]);
                break;
            }
            case 4: { // mouse wheel
                surfaceAdapter_->mouseWheel(
                        message["x"_float],
                        message["y"_float],
                        message["dx"_float],
                        message["dy"_float]);
                break;
            }
            case 5: { // key down
                surfaceAdapter_->keyDown(message["keyCode"_int]);
                break;
            }
            case 6: { // key up
                surfaceAdapter_->keyUp(message["keyCode"_int]);
                break;
            }
            default:
                break;
        }
    }
}
