// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "runtime-fixture.h"

BOOST_AUTO_TEST_SUITE(runtime_ownership_unpublish)

    BOOST_AUTO_TEST_CASE(should_work_local) {
        LocalFixture f{};
        f.strand->execute([&]() {
            auto object1 = f.federate1->getObjectClass("Foo").create();
            object1["bar"] = 47;
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
            f.federate1->shutdown().done();
        });
        f.strand->runUntilDone();
        f.strand->execute([&]() {
            BOOST_CHECK_EQUAL(0,count_objects( f.federate2->getObjectClass("Foo")));
        });
    }

BOOST_AUTO_TEST_SUITE_END()
