// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "./memory.h"


struct DestructorCallback {
    std::function<void()> callback;
    ~DestructorCallback() { callback(); }
};

BOOST_AUTO_TEST_SUITE(battlemodel_pointer)


    BOOST_AUTO_TEST_CASE(should_work_with_root_ptr) {
        auto done = false;
        auto root = RootPtr<DestructorCallback>{ new DestructorCallback{ [&done]() { done = true; } } };
        root.reset(nullptr);
        BOOST_REQUIRE_EQUAL(root.get(), nullptr);
        BOOST_REQUIRE_EQUAL(done, true);
    }

    BOOST_AUTO_TEST_CASE(should_work_with_back_ptr) {
        auto root = RootPtr<int>{ new int{ 1 } };
        auto back = BackPtr<int>{ root };
        BOOST_REQUIRE_EQUAL(root.get(), back.get());
    }

    BOOST_AUTO_TEST_CASE(should_work_with_weak_ptr) {
        auto root = RootPtr<int>{ new int{ 1 } };
        auto weak = WeakPtr<int>{ root };
        root.reset(nullptr);
        BOOST_REQUIRE_EQUAL(weak.get(), nullptr);
    }

BOOST_AUTO_TEST_SUITE_END()
