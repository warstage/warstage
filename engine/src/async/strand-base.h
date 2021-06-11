// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__STRAND__STRAND_BASE_H
#define WARSTAGE__STRAND__STRAND_BASE_H

#include <mutex>
#include <coroutine>


class ImmediateObject;
class IntervalObject;
class Strand_Manual;
class TimeoutObject;


class Strand_base {
    static std::mutex mutex__;
    static std::shared_ptr<Strand_Manual> render__;

protected:
    static thread_local std::shared_ptr<Strand_base> current__;

public:
    struct ClearCurrent {};
    struct SetCurrent {
        std::shared_ptr<Strand_base> previous_;
        std::shared_ptr<Strand_base> strand_;
        explicit SetCurrent(ClearCurrent) : previous_{current__}, strand_{} {
            current__ = nullptr;
        }
        explicit SetCurrent(std::shared_ptr<Strand_base> strand) : previous_{current__}, strand_{std::move(strand)} {
            assert(!current__);
            current__ = strand_;
        }
        ~SetCurrent() {
            assert(current__ == strand_);
            current__ = previous_;
        }
    };

    Strand_base() = default;
    virtual ~Strand_base() = default;

    static std::shared_ptr<Strand_Manual> getRender();

    [[nodiscard]] static std::shared_ptr<Strand_base> getCurrent() {
        return current__;
    }

    [[nodiscard]] bool isCurrent() const {
        return current__.get() == this;
    }

    virtual std::shared_ptr<TimeoutObject> setTimeout(std::function<void()> callback, double delay) = 0;
    virtual std::shared_ptr<IntervalObject> setInterval(std::function<void()> callback, double delay) = 0;
    virtual std::shared_ptr<ImmediateObject> setImmediate(std::function<void()> callback) = 0;

    Strand_base(const Strand_base&) = delete;
    Strand_base& operator=(const Strand_base&) = delete;
    Strand_base(Strand_base&&) = default;
    Strand_base& operator=(Strand_base&&) = default;

    /* co_await */

    [[nodiscard]] bool await_ready() const noexcept {
        return isCurrent();
    }

    template <typename P>
    void await_suspend(std::experimental::coroutine_handle<P> handle) noexcept {
        assert(!isCurrent());
        setImmediate([address = handle.address()]() {
            std::experimental::coroutine_handle<P>::from_address(address).resume();
        });
    }

    void await_resume() {
        assert(isCurrent());
    }

};


#endif
