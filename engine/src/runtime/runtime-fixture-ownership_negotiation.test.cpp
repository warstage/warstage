// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "runtime-fixture.h"

namespace {
    void test_ownership_negotiation(RuntimeFixture& f) {
        f.strand->execute([&]() {
            auto object = f.federate1->getObjectClass("Foo").create();
            object["bar"] = 47;
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            auto object1 = *f.federate1->getObjectClass("Foo").begin();
            auto object2 = *f.federate2->getObjectClass("Foo").begin();
            BOOST_CHECK_EQUAL(47, object1["bar"_int]);
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Owned
                + OwnershipStateFlag::NotDivesting
                + OwnershipStateFlag::NotAskedToRelease,
                object1["bar"].getOwnershipState());
            BOOST_CHECK_EQUAL(47, object2["bar"_int]);
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Unowned
                + OwnershipStateFlag::NotAbleToAcquire,
                object2["bar"].getOwnershipState());
            object2["bar"].modifyOwnershipState(OwnershipOperation::Publish);
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Unowned
                + OwnershipStateFlag::AbleToAcquire
                + OwnershipStateFlag::NotAcquiring
                + OwnershipStateFlag::NotTryingToAcquire,
                object2["bar"].getOwnershipState());
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            auto object1 = *f.federate1->getObjectClass("Foo").begin();
            auto object2 = *f.federate2->getObjectClass("Foo").begin();
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
            object2["bar"].modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Unowned
                + OwnershipStateFlag::AbleToAcquire
                + OwnershipStateFlag::AcquisitionPending
                + OwnershipStateFlag::Acquiring
                + OwnershipStateFlag::NotTryingToAcquire,
                object2["bar"].getOwnershipState());
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            auto object1 = *f.federate1->getObjectClass("Foo").begin();
            auto object2 = *f.federate2->getObjectClass("Foo").begin();
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Unowned
                + OwnershipStateFlag::AbleToAcquire
                + OwnershipStateFlag::NotAcquiring
                + OwnershipStateFlag::NotTryingToAcquire,
                object1["bar"].getOwnershipState());
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Owned
                + OwnershipStateFlag::NotDivesting
                + OwnershipStateFlag::NotAskedToRelease,
                object2["bar"].getOwnershipState());
            object2["bar"].modifyOwnershipState(OwnershipOperation::NegotiatedOwnershipDivestiture);
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Owned
                + OwnershipStateFlag::Divesting
                + OwnershipStateFlag::NotAskedToRelease,
                object2["bar"].getOwnershipState());
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            auto object1 = *f.federate1->getObjectClass("Foo").begin();
            auto object2 = *f.federate2->getObjectClass("Foo").begin();
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

BOOST_AUTO_TEST_SUITE(runtime_ownership_negotiation)

    BOOST_AUTO_TEST_CASE(should_modify_ownership_local) {
        LocalFixture f{};
        test_ownership_negotiation(f);
    }

    BOOST_AUTO_TEST_CASE(should_modify_ownership_remote) {
        RemoteFixture f{};
        test_ownership_negotiation(f);
    }

    BOOST_AUTO_TEST_CASE(should_modify_ownership_relay) {
        RelayFixture f{};
        test_ownership_negotiation(f);
    }

BOOST_AUTO_TEST_SUITE_END()
