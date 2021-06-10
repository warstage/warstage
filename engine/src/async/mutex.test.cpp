// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "async/mutex.h"


BOOST_AUTO_TEST_SUITE(async_mutex)

    static Promise<void> ResolveImmediate(Strand_Asio& strand) {
        auto result = Promise<void>{};
        strand.setImmediate([result]() {
            result.resolve().done();
        });
        return result;
    }

    static Promise<void> DoRequest(Strand_Asio& strand, int n, Mutex& mutex, std::atomic_int& count, std::atomic_int& current, int index) {
        MutexLock lock = co_await mutex.lock();
        const auto old = current.load();
        current = index;
        for (int i = 0; i < n; ++i) {
            co_await ResolveImmediate(strand);
            BOOST_REQUIRE_EQUAL(current.load(), index);
            ++count;
        }
        current = old;
    }

    static Promise<void> should_not_interleave_execution_on_strand_(Mutex& mutex) {
        auto promises = std::vector<Promise<void>>{};
        auto current = std::atomic_int{0};
        auto count = std::atomic_int{0};
        for (int index = 1; index < 5; ++index) {
            promises.emplace_back();
            Strand_Asio::getMain()->setImmediate([promise = promises.back(), &mutex, &count, &current, index]() {
                promise.resolve(DoRequest(*Strand_Asio::getMain(), 5, mutex, count, current, index)).done();
            });
        }
        for (auto& promise : promises) {
            co_await promise;
        }
        BOOST_REQUIRE_EQUAL(count.load(), 4 * 5);
    }

    BOOST_AUTO_TEST_CASE(should_not_interleave_execution_on_strand) {
        auto mutex = Mutex{};
        auto done = false;
        Strand_Asio::getMain()->setImmediate([&mutex, &done]() {
            should_not_interleave_execution_on_strand_(mutex).then<void>([&done]() {
                done = true;
            }).done();
        });
        Strand_Asio::runUntilDone();
        BOOST_REQUIRE_EQUAL(done, true);
    }

BOOST_AUTO_TEST_SUITE_END()
