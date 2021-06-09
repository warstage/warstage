// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "./value.h"


static std::string hex(const Value& value) {
    std::string r;
    auto hex = "0123456789abcdef";
    auto i = reinterpret_cast<const unsigned char*>(value.data());
    auto j = i + value.size();
    while (i != j) {
        r += hex[*i >> 4];
        r += hex[*i & 15];
        ++i;
    }
    return r;
}

BOOST_AUTO_TEST_SUITE(value_builder)

    BOOST_AUTO_TEST_CASE(empty_document) {
        auto value = Struct() << ValueEnd();
        BOOST_CHECK_EQUAL("0500000000", hex(value));
    }

    BOOST_AUTO_TEST_CASE(document_with_primitive_values) {
        auto value = Struct()
            << "x" << nullptr
            << "y" << false
            << "z" << true
            << "w" << 3.14f
            << ValueEnd();
        BOOST_CHECK_EQUAL("1b0000000a780008790000087a000101770000000060b81e094000", hex(value));
    }

    BOOST_AUTO_TEST_CASE(document_with_object_ids) {
        auto value = Struct()
                << "x" << ObjectId::parse("111122223333444455556666")
                << "y" << ObjectId::parse("777788889999aaaabbbbcccc")
                << ValueEnd();
        BOOST_CHECK_EQUAL("23000000077800111122223333444455556666077900777788889999aaaabbbbcccc00", hex(value));
    }

    BOOST_AUTO_TEST_CASE(document_with_string_values) {
        auto value = Struct()
                << "x" << "A"
                << "y" << std::string("a")
                << ValueEnd();
        BOOST_CHECK_EQUAL("1700000002780002000000410002790002000000610000", hex(value));
        // 17000000 02 7800 02000000 4100 02 7900 02000000 6100 00
    }

    BOOST_AUTO_TEST_CASE(document_with_binary_values) {
        auto value = Struct()
                << "x" << Binary("ABC")
                << ValueEnd();
        BOOST_CHECK_EQUAL("10000000057800030000000041424300", hex(value));
    }

    BOOST_AUTO_TEST_CASE(document_with_array) {
        auto value = Struct()
                << "x" << Array()
                << 47 << 62 << "ABC"
                << ValueEnd()
                << ValueEnd();
        BOOST_CHECK_EQUAL("260000000478001e0000001030002f0000001031003e00000002320004000000414243000000", hex(value));
    }

    BOOST_AUTO_TEST_CASE(document_with_array_with_document) {
        auto value = Struct()
                << "x" << Array()
                << Struct() << "y" << 47 << ValueEnd()
                << ValueEnd()
                << ValueEnd();
        BOOST_CHECK_EQUAL("1c000000047800140000000330000c0000001079002f000000000000", hex(value));
    }

    BOOST_AUTO_TEST_CASE(document_with_array_with_two_documents_2) {
        auto arr = Struct() << "x" << Array();
        arr = std::move(arr) << Struct() << "y" << 47 << ValueEnd();
        arr = std::move(arr) << Struct() << "z" << 62 << ValueEnd();
        auto value = std::move(arr) << ValueEnd() << ValueEnd();
        BOOST_CHECK_EQUAL("2b000000047800230000000330000c0000001079002f000000000331000c000000107a003e000000000000", hex(value));
    }

    BOOST_AUTO_TEST_CASE(document_with_array_with_two_documents) {
        auto value = Struct()
                << "x" << Array()
                << Struct() << "y" << 47 << ValueEnd()
                << Struct() << "z" << 62 << ValueEnd()
                << ValueEnd()
                << ValueEnd();
        BOOST_CHECK_EQUAL("2b000000047800230000000330000c0000001079002f000000000331000c000000107a003e000000000000", hex(value));
    }

    BOOST_AUTO_TEST_CASE(document_with_document) {
        auto value = Struct()
                << "x" << Struct()
                << "y" << 47
                << ValueEnd()
                << ValueEnd();
        BOOST_CHECK_EQUAL("140000000378000c0000001079002f0000000000", hex(value));
    }

    BOOST_AUTO_TEST_CASE(document_with_existing_document) {
        auto doc = Struct()
                << "x" << 47
                << ValueEnd();
        auto value = Struct()
                << "X" << doc
                << "y" << 62
                << ValueEnd();
        //BOOST_CHECK_EQUAL("1b000000035800130000001078002f0000001079003e0000000000", hex(value));
    }

    BOOST_AUTO_TEST_CASE(document_with_document_with_document) {
        auto value = Struct()
                << "x" << Struct()
                << "y" << Struct()
                << "z" << 47
                << ValueEnd()
                << ValueEnd()
                << ValueEnd();
        BOOST_CHECK_EQUAL("1c000000037800140000000379000c000000107a002f000000000000", hex(value));
    }

BOOST_AUTO_TEST_SUITE_END()
