// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "runtime/runtime.h"

namespace {
    struct RuntimeFixture {
        std::shared_ptr<Strand_Manual> strand;
        std::shared_ptr<Runtime> masterRuntime;
        RuntimeFixture() {
            strand = std::make_shared<Strand_Manual>();
            PromiseUtils::strand_ = strand;
            masterRuntime = std::make_shared<Runtime>(ProcessType::Player);
        }
        ~RuntimeFixture() {
            masterRuntime->shutdown().done();
            strand->runUntilDone();
        }
    };

    void shutdown_runtime() {
        RuntimeFixture f{};
    }

    void shutdown_federate() {
        RuntimeFixture f{};
        auto federate1 = std::make_shared<Federate>(*f.masterRuntime, "Federate1", f.strand);
        federate1->shutdown().done();
        f.strand->runUntilDone();
    }
}

BOOST_AUTO_TEST_SUITE(runtime_startup_shutdown)

    BOOST_AUTO_TEST_CASE(should_shutdown_runtime) {
        BOOST_REQUIRE_NO_THROW(shutdown_runtime());
    }

    BOOST_AUTO_TEST_CASE(should_shutdown_federate) {
        BOOST_REQUIRE_NO_THROW(shutdown_federate());
    }

BOOST_AUTO_TEST_SUITE_END()
