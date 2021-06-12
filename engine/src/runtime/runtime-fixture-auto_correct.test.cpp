// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "runtime-fixture.h"

namespace {
    void test_ownership_auto_correct_assignment(RuntimeFixture& f) {
        f.federate2->getObjectClass("Foo").publish({"bar"});
        f.strand->execute([&]() {
            auto object1 = f.federate1->getObjectClass("Foo").create();
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            auto object1 = *f.federate1->getObjectClass("Foo").begin();
            auto object2 = *f.federate2->getObjectClass("Foo").begin();
            object1["bar"] = 47;
            object2["bar"] = 47;
            BOOST_CHECK_EQUAL(47, object1["bar"_int]);
            BOOST_CHECK_EQUAL(47, object2["bar"_int]);
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Owned
                + OwnershipStateFlag::NotDivesting
                + OwnershipStateFlag::NotAskedToRelease,
                object1["bar"].getOwnershipState());
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Owned
                + OwnershipStateFlag::NotDivesting
                + OwnershipStateFlag::NotAskedToRelease,
                object2["bar"].getOwnershipState());
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            auto object1 = *f.federate1->getObjectClass("Foo").begin();
            auto object2 = *f.federate2->getObjectClass("Foo").begin();
            BOOST_CHECK_EQUAL(47, object1["bar"_int]);
            BOOST_CHECK_EQUAL(47, object2["bar"_int]);
            bool owned1 = object1["bar"].getOwnershipState() & OwnershipStateFlag::Owned;
            bool owned2 = object2["bar"].getOwnershipState() & OwnershipStateFlag::Owned;
            BOOST_CHECK(owned1 != owned2);
        });
    }

    void test_ownership_auto_correct_divestiture(RuntimeFixture& f) {
        f.federate2->getObjectClass("Foo").publish({"bar"});
        f.strand->execute([&]() {
            auto object1 = f.federate1->getObjectClass("Foo").create();
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            auto object1 = *f.federate1->getObjectClass("Foo").begin();
            auto object2 = *f.federate2->getObjectClass("Foo").begin();
            object1["bar"] = 47;
            object2["bar"] = 47;
            object1["bar"].modifyOwnershipState(OwnershipOperation::NegotiatedOwnershipDivestiture);
            object2["bar"].modifyOwnershipState(OwnershipOperation::NegotiatedOwnershipDivestiture);
            BOOST_CHECK_EQUAL(47, object1["bar"_int]);
            BOOST_CHECK_EQUAL(47, object2["bar"_int]);
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Owned
                + OwnershipStateFlag::Divesting
                + OwnershipStateFlag::NotAskedToRelease,
                object1["bar"].getOwnershipState());
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
            BOOST_CHECK_EQUAL(47, object1["bar"_int]);
            BOOST_CHECK_EQUAL(47, object2["bar"_int]);
            bool owned1 = object1["bar"].getOwnershipState() & OwnershipStateFlag::Owned;
            bool owned2 = object2["bar"].getOwnershipState() & OwnershipStateFlag::Owned;
            BOOST_CHECK(owned1 != owned2);
        });
    }

    void test_ownership_auto_correct_unpublish(RuntimeFixture& f) {
        f.federate2->getObjectClass("Foo").publish({"bar"});
        f.strand->execute([&]() {
            auto object1 = f.federate1->getObjectClass("Foo").create();
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            auto object1 = *f.federate1->getObjectClass("Foo").begin();
            auto object2 = *f.federate2->getObjectClass("Foo").begin();
            object1["bar"] = 47;
            object2["bar"] = 47;
            object1["bar"].modifyOwnershipState(OwnershipOperation::Unpublish);
            object2["bar"].modifyOwnershipState(OwnershipOperation::Unpublish);
            BOOST_CHECK_EQUAL(47, object1["bar"_int]);
            BOOST_CHECK_EQUAL(47, object2["bar"_int]);
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Unowned
                + OwnershipStateFlag::NotAbleToAcquire,
                object1["bar"].getOwnershipState());
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Unowned
                + OwnershipStateFlag::NotAbleToAcquire,
                object2["bar"].getOwnershipState());
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            auto object1 = *f.federate1->getObjectClass("Foo").begin();
            auto object2 = *f.federate2->getObjectClass("Foo").begin();
            bool owned1 = object1["bar"].getOwnershipState() & OwnershipStateFlag::Owned;
            bool owned2 = object2["bar"].getOwnershipState() & OwnershipStateFlag::Owned;
            BOOST_CHECK(!owned1);
            BOOST_CHECK(!owned2);
        });
    }

    void test_ownership_auto_correct_xxxx(RuntimeFixture& f) {
        f.federate2->getObjectClass("Foo").publish({"bar"});
        f.strand->execute([&]() {
            auto object1 = f.federate1->getObjectClass("Foo").create();
            object1["bar"] = 47;
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            auto object1 = *f.federate1->getObjectClass("Foo").begin();
            auto object2 = *f.federate2->getObjectClass("Foo").begin();
            BOOST_CHECK_EQUAL(47, object1["bar"_int]);
            BOOST_CHECK_EQUAL(47, object2["bar"_int]);
            object1["bar"].modifyOwnershipState(OwnershipOperation::NegotiatedOwnershipDivestiture);
            object2["bar"].modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
            BOOST_CHECK_EQUAL(OwnershipState{}
                + OwnershipStateFlag::Owned
                + OwnershipStateFlag::Divesting
                + OwnershipStateFlag::NotAskedToRelease,
                object1["bar"].getOwnershipState());
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
        });
    }
}

BOOST_AUTO_TEST_SUITE(runtime_ownership_auto_correct)

    BOOST_AUTO_TEST_CASE(assignment_local) {
        RemoteFixture f{};
        test_ownership_auto_correct_assignment(f);
    }

    BOOST_AUTO_TEST_CASE(assignment_remote) {
        RemoteFixture f{};
        test_ownership_auto_correct_assignment(f);
    }

    BOOST_AUTO_TEST_CASE(assignment_relay) {
        RelayFixture f{};
        test_ownership_auto_correct_assignment(f);
    }

    /***/

    BOOST_AUTO_TEST_CASE(divestiture_local) {
        RemoteFixture f{};
        test_ownership_auto_correct_divestiture(f);
    }

    BOOST_AUTO_TEST_CASE(divestiture_remote) {
        RemoteFixture f{};
        test_ownership_auto_correct_divestiture(f);
    }

    BOOST_AUTO_TEST_CASE(divestiture_relay) {
        RelayFixture f{};
        test_ownership_auto_correct_divestiture(f);
    }

    /***/

    BOOST_AUTO_TEST_CASE(unpublish_local) {
        RemoteFixture f{};
        test_ownership_auto_correct_unpublish(f);
    }

    BOOST_AUTO_TEST_CASE(unpublish_remote) {
        RemoteFixture f{};
        test_ownership_auto_correct_unpublish(f);
    }

    BOOST_AUTO_TEST_CASE(unpublish_relay) {
        RelayFixture f{};
        test_ownership_auto_correct_unpublish(f);
    }

    /***/

    BOOST_AUTO_TEST_CASE(xxxx_local) {
        RemoteFixture f{};
        test_ownership_auto_correct_xxxx(f);
    }

    BOOST_AUTO_TEST_CASE(xxxx_remote) {
        RemoteFixture f{};
        test_ownership_auto_correct_xxxx(f);
    }

    BOOST_AUTO_TEST_CASE(xxxx_relay) {
        RelayFixture f{};
        test_ownership_auto_correct_xxxx(f);
    }

BOOST_AUTO_TEST_SUITE_END()
