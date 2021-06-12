// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "runtime-fixture.h"

namespace {
    void should_synchronize_new_objects(RuntimeFixture& f) {
        f.strand->execute([&]() {
            auto foo = f.federate1->getObjectClass("Foo").create();
            foo["bar"] = 47;
            foo["nope"_int];
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            BOOST_CHECK_EQUAL(1, count_objects(f.federate2->getObjectClass("Foo")));
            auto foo = f.federate2->getObjectClass("Foo").find([](auto) { return true; });
            foo["nope"] = 62;
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            auto foo = f.federate1->getObjectClass("Foo").find([](auto) { return true; });
            BOOST_CHECK_EQUAL(62, foo["nope"_int]);
        });
    }
}

BOOST_AUTO_TEST_SUITE(runtime_sync_object)

    BOOST_AUTO_TEST_CASE(should_synchronize_new_objects_local) {
        LocalFixture f{};
        should_synchronize_new_objects(f);
    }

    BOOST_AUTO_TEST_CASE(should_synchronize_new_objects_remote) {
        RemoteFixture f{};
        should_synchronize_new_objects(f);
    }

    BOOST_AUTO_TEST_CASE(should_synchronize_new_objects_relay) {
        RelayFixture f{};
        should_synchronize_new_objects(f);
    }

BOOST_AUTO_TEST_SUITE_END()
