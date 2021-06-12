// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "runtime-fixture.h"

namespace {
    void init_fixture(RuntimeFixture& f) {
        auto federation1 = f.federate1->getRuntime().acquireFederation_safe(f.federate1->getFederationId());
        federation1->setOwnershipPolicy([&f](const Federate& federate, const std::string&) {
            return &federate == f.federate1.get();
        });
        f.federate1->getRuntime().releaseFederation_safe(federation1);
        f.federate1->getObjectClass("Foo").publish({"bar"});
        f.federate2->getObjectClass("Foo").publish({"bar"});
    }

    void should_not_sync_spurious_object(RuntimeFixture& f) {
        init_fixture(f);
        f.strand->execute([&]() {
            auto object1 = f.federate1->getObjectClass("Foo").create();
            auto object2 = f.federate2->getObjectClass("Foo").create();
            object1["bar"] = 47;
            object2["bar"] = 62;
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            BOOST_CHECK_EQUAL(1, count_objects(f.federate1->getObjectClass("Foo")));
            BOOST_CHECK_EQUAL(2, count_objects(f.federate2->getObjectClass("Foo")));
            auto object2 = f.federate2->getObjectClass("Foo").find([](auto const& x) { return x["bar"_int] == 62; });
            object2.Delete();
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            BOOST_CHECK_EQUAL(1, count_objects(f.federate1->getObjectClass("Foo")));
            BOOST_CHECK_EQUAL(1, count_objects(f.federate2->getObjectClass("Foo")));
        });
        f.federate2->shutdown().done();
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            BOOST_CHECK_EQUAL(1, count_objects(f.federate1->getObjectClass("Foo")));
        });
    }

    void should_override_spurious_object(RuntimeFixture& f) {
        init_fixture(f);
        f.strand->execute([&]() {
            auto object2 = f.federate2->getObjectClass("Foo").create();
            object2["bar"] = 62;
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            BOOST_CHECK_EQUAL(0, count_objects(f.federate1->getObjectClass("Foo")));
            BOOST_CHECK_EQUAL(1, count_objects(f.federate2->getObjectClass("Foo")));

            auto object2 = f.federate2->getObjectClass("Foo").find([](auto) { return true; });
            auto object1 = f.federate1->getObjectClass("Foo").create(object2.getObjectId());
            object1["bar"] = 47;
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            BOOST_CHECK_EQUAL(1, count_objects(f.federate1->getObjectClass("Foo")));
            BOOST_CHECK_EQUAL(1, count_objects(f.federate2->getObjectClass("Foo")));
            auto object1 = f.federate1->getObjectClass("Foo").find([](auto){ return true; });
            auto object2 = f.federate2->getObjectClass("Foo").find([](auto){ return true; });
            BOOST_CHECK_EQUAL(47, object1["bar"_int]);
            BOOST_CHECK_EQUAL(47, object2["bar"_int]);
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Owned
                + OwnershipStateFlag::NotDivesting
                + OwnershipStateFlag::NotAskedToRelease,
                object1["bar"].getOwnershipState());
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Unowned
                + OwnershipStateFlag::AbleToAcquire
                + OwnershipStateFlag::NotAcquiring
                + OwnershipStateFlag::NotTryingToAcquire,
                object2["bar"].getOwnershipState());
        });
    }
}

BOOST_AUTO_TEST_SUITE(runtime_ownership_policy)

    BOOST_AUTO_TEST_CASE(should_not_sync_spurious_object_local) {
        LocalFixture f{};
        should_not_sync_spurious_object(f);
    }

    BOOST_AUTO_TEST_CASE(should_not_sync_spurious_object_remote) {
        RemoteFixture f{};
        should_not_sync_spurious_object(f);
    }

    BOOST_AUTO_TEST_CASE(should_not_sync_spurious_object_relay) {
        RelayFixture f{};
        should_not_sync_spurious_object(f);
    }

    /***/

    BOOST_AUTO_TEST_CASE(should_override_spurious_object_local) {
        LocalFixture f{};
        should_override_spurious_object(f);
    }

    BOOST_AUTO_TEST_CASE(should_override_spurious_object_remote) {
        RemoteFixture f{};
        should_override_spurious_object(f);
    }

    BOOST_AUTO_TEST_CASE(should_override_spurious_object_relay) {
        RelayFixture f{};
        should_override_spurious_object(f);
    }

BOOST_AUTO_TEST_SUITE_END()
