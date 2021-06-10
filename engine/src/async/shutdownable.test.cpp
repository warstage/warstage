// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "async/shutdownable.h"


class ShutdownableMock : public Shutdownable {
public:
    int state = 0;
    Promise<void> promise1 = {};
    Promise<void> promise2 = {};
protected:
    [[nodiscard]] Promise<void> shutdown_() override {
        state = 1;
        co_await promise1;
        state = 2;
        co_await promise2;
        state = 3;
    }
};


BOOST_AUTO_TEST_SUITE(async_shutdownable)

    BOOST_AUTO_TEST_CASE(shutdown_should_be_fulfilled) {
        auto strand = std::make_shared<Strand_Manual>();

        ShutdownableMock shutdownable{};
        BOOST_REQUIRE_EQUAL(shutdownable.state, 0);
        BOOST_REQUIRE(!shutdownable.shutdownStarted());
        BOOST_REQUIRE(!shutdownable.shutdownCompleted());

        Promise<void> promise;
        strand->setImmediate([&]() {
            promise = shutdownable.shutdown();
        });
        strand->runUntilDone();
        BOOST_REQUIRE_EQUAL(shutdownable.state, 1);
        BOOST_REQUIRE(shutdownable.shutdownStarted());
        BOOST_REQUIRE(!shutdownable.shutdownCompleted());
        BOOST_REQUIRE(!promise.isFulfilled());

        strand->setImmediate([&]() {
            shutdownable.promise1.resolve().done();
        });
        strand->runUntilDone();
        BOOST_REQUIRE_EQUAL(shutdownable.state, 2);
        BOOST_REQUIRE(shutdownable.shutdownStarted());
        BOOST_REQUIRE(!shutdownable.shutdownCompleted());
        BOOST_REQUIRE(!promise.isFulfilled());

        strand->setImmediate([&]() {
            shutdownable.promise2.resolve().done();
        });
        strand->runUntilDone();
        BOOST_REQUIRE_EQUAL(shutdownable.state, 3);
        BOOST_REQUIRE(shutdownable.shutdownStarted());
        BOOST_REQUIRE(shutdownable.shutdownCompleted());
        BOOST_REQUIRE(promise.isFulfilled());
    }

BOOST_AUTO_TEST_SUITE_END()
