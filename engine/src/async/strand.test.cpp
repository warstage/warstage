// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "async/promise.h"
#include "async/strand.h"
#include <random>

namespace {

    std::random_device rd{};
    std::default_random_engine gen{0};
    template <typename T>
    auto randint(T a, T b) {
        return std::uniform_int_distribution{a, b}(gen);
    }
}

BOOST_AUTO_TEST_SUITE(async_strand)

    BOOST_AUTO_TEST_CASE(asio_should_run_until_stopped) {
        Strand_Asio::Context context{};
        std::atomic_int counter = 0;
        context.getMain()->setTimeout([&context, &counter]() {
            ++counter;
            context.stop();
        }, 100);
        context.runUntilStopped(1);
        BOOST_CHECK_EQUAL(counter.load(), 1);
    }

    /***/

    BOOST_AUTO_TEST_CASE(asio_should_execute_immediate_on_correct_strand) {
        Strand_Asio::Context context{};
        std::vector<std::shared_ptr<Strand>> strands(10);
        std::generate(strands.begin(), strands.end(), [&context](){ return context.makeStrand(""); });
        std::function<void()> action;
        action = [&strands, &action]() {
            auto strand = strands[randint(0UL, strands.size() - 1)];
            strand->setImmediate([strand, action = std::move(action)]() {
                if (!strand->isCurrent()) {
                    BOOST_CHECK(strand->isCurrent());
                }
                action();
            });
        };
        for (int i = 0; i < 20; ++i) {
            action();
        }
        context.getMain()->setTimeout([&context]() {
            context.stop();
        }, 500);
        context.runUntilStopped(1);
    }

    /***/

    Promise<void> coAwaitOnRandomStrand(const std::vector<std::shared_ptr<Strand>>& strands, std::atomic_int& counter, int count) {
        for (auto i = 0; i != count; ++i) {
            auto strand = strands[randint(0UL, strands.size() - 1)];
            co_await *strand;
            if (!strand->isCurrent()) {
                BOOST_CHECK(strand->isCurrent());
            }
            ++counter;
        }
    }

    BOOST_AUTO_TEST_CASE(co_await_should_continue_on_correct_strand) {
        Strand_Asio::Context context{};
        std::vector<std::shared_ptr<Strand>> strands(4);
        std::generate(strands.begin(), strands.end(), [&context](){ return context.makeStrand(""); });
        std::atomic_int counter = 0;
        context.getMain()->setImmediate([&context, &strands, &counter]() {
            coAwaitOnRandomStrand(strands, counter, 100).then<void>([&context]() {
                context.getMain()->setImmediate([&context]() {
                    context.stop();
                });
            }).done();
        });
        context.runUntilStopped(1);
        BOOST_CHECK_EQUAL(counter.load(), 100);
    }

BOOST_AUTO_TEST_SUITE_END()
