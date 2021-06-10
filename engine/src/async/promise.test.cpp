// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "async/promise.h"

namespace {

    void TestPromise(const std::function<Promise<const char *>(std::ostream &os)>& f) {
        std::ostringstream os{};
        char const *r{};
        bool done{};
        Strand_Asio::getMain()->setImmediate([&os, &f, &done, &r]() {
            f(os).then<void>([&done, &r](auto v) {
                done = true;
                r = v;
            }).done();
        });
        Strand_Asio::runUntilDone();
        BOOST_REQUIRE_EQUAL(done, true);
        BOOST_REQUIRE_EQUAL(os.str(), r);
    }

}

/***/

BOOST_AUTO_TEST_SUITE(async_promise)

    static Promise<const char *> should_execute_then_on_resolved_promise_(std::ostream &os) {
        Promise<int> promise{};
        promise.resolve(47).done();
        promise.reject("no good").done();
        promise.resolve(62).done();
        promise.then<void>([&os](auto value) {
            os << "then(" << value << ")";
        }, [&os](auto reason) {
            os << "catch()";
        }).done();
        return resolve<const char *>("then(47)");
    }

    BOOST_AUTO_TEST_CASE(should_execute_then_on_resolved_promise) {
        TestPromise([](auto& os) { return should_execute_then_on_resolved_promise_(os); });
    }

    /***/

    static Promise<const char *> should_execute_then_on_resolved_promise_CO_(std::ostream &os) {
        Promise<int> promise{};
        promise.resolve(47).done();
        promise.reject("no good").done();
        promise.resolve(62).done();
        auto value = co_await promise;
        os << "then(" << value << ")";
        co_return "then(47)";
    }

    BOOST_AUTO_TEST_CASE(should_execute_then_on_resolved_promise_CO) {
        TestPromise([](auto& os) { return should_execute_then_on_resolved_promise_CO_(os); });
    }

    /***/

    static Promise<const char *> should_execute_catch_on_rejected_promise_(std::ostream &os) {
        Promise<int> promise{};
        promise.reject(std::string("oh no")).done();
        promise.resolve(47).done();
        promise.reject(std::string("no good")).done();
        promise.resolve(62).done();
        promise.then<void>([&os](auto value) {
            os << "then(" << value << ")";
        }, [&os](auto reason) {
            try {
                std::rethrow_exception(reason);
            } catch (const std::string &e) {
                os << "catch(" << e << ")";
            }
        }).done();
        return resolve<const char *>("catch(oh no)");
    }

    BOOST_AUTO_TEST_CASE(should_execute_catch_on_rejected_promise) {
        TestPromise([](auto& os) { return should_execute_catch_on_rejected_promise_(os); });
    }

    /***/

    static Promise<const char *> should_execute_catch_on_rejected_promise_CO_(std::ostream &os) {
        Promise<int> promise{};
        promise.reject(std::string("oh no")).done();
        promise.resolve(47).done();
        promise.reject(std::string("no good")).done();
        promise.resolve(62).done();

        try {
            int value = co_await promise;
            os << "then(" << value << ")";
        } catch (const std::string &e) {
            os << "catch(" << e << ")";
        }
        co_return "catch(oh no)";
    }

    BOOST_AUTO_TEST_CASE(should_execute_catch_on_rejected_promise_CO) {
        TestPromise([](auto& os) { return should_execute_catch_on_rejected_promise_CO_(os); });
    }

    /***/

    static Promise<const char *> should_execute_then_after_resolving_promise_(std::ostream &os) {
        Promise<int> promise{};
        promise.then<void>([&os](auto value) {
            os << "then(" << value << ")";
        }, [&os](auto reason) {
            os << "catch()";
        }).done();
        Strand_Asio::getMain()->setImmediate([promise]() mutable {
            promise.resolve(47).done();
            promise.reject("no good").done();
            promise.resolve(62).done();
        });
        return resolve<const char *>("then(47)");
    }

    BOOST_AUTO_TEST_CASE(should_execute_then_after_resolving_promise) {
        TestPromise([](auto& os) { return should_execute_then_after_resolving_promise_(os); });
    }

    /***/

    static Promise<const char *> should_execute_then_after_resolving_promise_CO_(std::ostream &os) {
        Promise<int> promise{};
        promise.then<void>([&os](auto value) {
            os << "then(" << value << ")";
        }, [&os](auto reason) {
            os << "catch()";
        }).done();
        Strand_Asio::getMain()->setImmediate([promise]() mutable {
            promise.resolve(47).done();
            promise.reject("no good").done();
            promise.resolve(62).done();
        });
        return resolve<const char *>("then(47)");
    }

    BOOST_AUTO_TEST_CASE(should_execute_then_after_resolving_promise_CO) {
        TestPromise([](auto& os) { return should_execute_then_after_resolving_promise_CO_(os); });
    }

    /***/

    static Promise<void> awaitStrand(Strand_Asio& strand) {
        BOOST_REQUIRE(Strand_Asio::getMain()->isCurrent());
        BOOST_REQUIRE(!strand.isCurrent());
        co_await strand;
        BOOST_REQUIRE(strand.isCurrent());
        BOOST_REQUIRE(!Strand_Asio::getMain()->isCurrent());
    }

    static Promise<const char *> co_await_should_continue_on_original_strand_(std::ostream &os) {
        std::vector<std::shared_ptr<Strand_Asio>> strands(3);
        std::generate(strands.begin(), strands.end(), [](){ return Strand_Asio::makeStrand(""); });

        for (int i = 0; i < 20; ++i) {
            auto& strand = *strands[i % strands.size()];
            BOOST_REQUIRE(Strand_Asio::getMain()->isCurrent());
            BOOST_REQUIRE(!strand.isCurrent());
            co_await awaitStrand(strand);
            BOOST_REQUIRE(Strand_Asio::getMain()->isCurrent());
            BOOST_REQUIRE(!strand.isCurrent());
            co_await strand;
            BOOST_REQUIRE(strand.isCurrent());
            BOOST_REQUIRE(!Strand_Asio::getMain()->isCurrent());
            co_await Promise<void>{}.resolve();
            BOOST_REQUIRE(strand.isCurrent());
            BOOST_REQUIRE(!Strand_Asio::getMain()->isCurrent());
            co_await *Strand_Asio::getMain();
            BOOST_REQUIRE(Strand_Asio::getMain()->isCurrent());
            BOOST_REQUIRE(!strand.isCurrent());
            co_await Promise<void>{}.resolve();
            BOOST_REQUIRE(Strand_Asio::getMain()->isCurrent());
            BOOST_REQUIRE(!strand.isCurrent());
        }

        co_return "";
    }

    BOOST_AUTO_TEST_CASE(co_await_should_continue_on_original_strand) {
        TestPromise([](auto& os) { return co_await_should_continue_on_original_strand_(os); });
    }

    /***/

    struct ShutdownPattern {
        std::shared_ptr<Strand> strand_ = Strand_Asio::makeStrand("");
        mutable std::mutex shutdownMutex_ = {};
        std::unique_ptr<Promise<void>> shutdownPromise_ = {};
        std::atomic_bool shutdownStarted_ = false;
        std::atomic_bool shutdownFinished_ = false;
        std::vector<ShutdownPattern*> others_ = {};
        bool ShutdownStarted() const {
            std::lock_guard shutdownLock{shutdownMutex_};
            if (shutdownPromise_->isFulfilled()) {
                BOOST_REQUIRE(shutdownStarted_);
                BOOST_REQUIRE(shutdownFinished_);
                return true;
            }
            return false;
        }
        Promise<void> Shutdown() {
            std::lock_guard shutdownLock{shutdownMutex_};
            if (!shutdownPromise_) {
                shutdownPromise_ = std::make_unique<Promise<void>>();
                shutdownPromise_->resolve(Shutdown_()).done();
            }
            return *shutdownPromise_;
        }
        Promise<void> Shutdown_() {
            co_await *strand_;
            shutdownStarted_ = true;
            if (!strand_->isCurrent()) {
                BOOST_REQUIRE(strand_->isCurrent());
            }
            for (auto other : others_) {
                co_await other->Shutdown();
                if (!strand_->isCurrent()) {
                    BOOST_REQUIRE(strand_->isCurrent());
                }
            }
            shutdownFinished_ = true;
        }
    };

    static Promise<const char*> shutdown_pattern_should_execute_on_corrent_strand_(std::ostream &os) {
        ShutdownPattern p1, p2, p3, p4;
        p1.others_.push_back(&p2);
        p1.others_.push_back(&p3);
        p2.others_.push_back(&p4);
        p3.others_.push_back(&p4);

        co_await p1.Shutdown();

        BOOST_REQUIRE(p1.ShutdownStarted());
        BOOST_REQUIRE(p2.ShutdownStarted());
        BOOST_REQUIRE(p3.ShutdownStarted());
        BOOST_REQUIRE(p4.ShutdownStarted());

        co_return "";
    }

    BOOST_AUTO_TEST_CASE(shutdown_pattern_should_execute_on_corrent_strand) {
        TestPromise([](auto& os) { return shutdown_pattern_should_execute_on_corrent_strand_(os); });
    }

    /***/

    static Promise<void> FulfillImmediate() {
        auto result = Promise<void>{};
        Strand_Asio::getMain()->setImmediate([result](){ result.resolve().done(); });
        return result;
    }

    static Promise<const char*> should_work_with_lambda_(std::ostream &os) {
        co_await FulfillImmediate();
        auto lambda = [&os]() -> Promise<void> {
            co_await FulfillImmediate();
            os << "ok";
            co_await FulfillImmediate();
        };
        co_await lambda();
        co_return "ok";
    }

    BOOST_AUTO_TEST_CASE(should_work_with_lambda) {
        TestPromise([](auto& os) { return should_work_with_lambda_(os); });
    }


BOOST_AUTO_TEST_SUITE_END()
