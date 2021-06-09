// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "./decompressor.h"
#include "./compressor.h"


static std::string hex(const void* data, std::size_t size) {
    std::string r;
    auto hex = "0123456789abcdef";
    auto i = reinterpret_cast<const unsigned char*>(data);
    auto j = i + size;
    while (i != j) {
        r += hex[*i >> 4];
        r += hex[*i & 15];
        ++i;
    }
    return r;
}

void check_compressor(const Value& value, const char* hexval1, const char* hexval2) {
    ValueCompressor c1{};
    c1.encode(value);
    BOOST_CHECK_EQUAL(std::string(hexval1), hex(c1.data(), c1.size()));
    
    ValueDecompressor d{};
    d.decode(c1.data(), c1.size());
    
    ValueCompressor c2{};
    c2.encode(Value{std::make_shared<ValueBuffer>(std::string{reinterpret_cast<const char*>(d.data()), d.size()})});
    BOOST_CHECK_EQUAL(std::string(hexval1), hex(c2.data(), c2.size()));
    
    c1.encode(value);
    BOOST_CHECK_EQUAL(std::string(hexval2), hex(c1.data(), c1.size()));
}


BOOST_AUTO_TEST_SUITE(value_compression)
    
    BOOST_AUTO_TEST_CASE(empty_document) {
        check_compressor(Struct() << ValueEnd(),
            "00",
            "00");
        //  "0500000000"
    }
    
    BOOST_AUTO_TEST_CASE(document_with_primitive_values) {
        check_compressor(Struct()
            << "x" << nullptr
            << "y" << false
            << "z" << true
            << "w" << 3.14f
            << ValueEnd(),
            "010078000200790003007a0006007700c3f5484000",
            "0101020203030604c3f5484000");
        //  ""
    }
    
    BOOST_AUTO_TEST_CASE(document_with_integers) {
        check_compressor(Struct()
                << "a" << 0
                << "b" << 10
                << "c" << 100
                << "d" << 1000
                << "e" << 100000
                << ValueEnd(),
            "200061002a00620038006300643900640003e83a006500000186a000",
            "20012a02380364390403e83a05000186a000");
        //  ""
    }

    BOOST_AUTO_TEST_CASE(document_with_negative_integers) {
        check_compressor(Struct()
                << "a" << -1
                << "b" << -100
                << "c" << -1000
                << "d" << -1000000
                << ValueEnd(),
            "3c006100003c006200633d00630003e73e006400000f423f00",
            "3c01003c02633d0303e73e04000f423f00");
        //  ""
    }

    BOOST_AUTO_TEST_CASE(document_with_object_ids) {
        check_compressor(Struct()
                << "x" << ObjectId::parse("111122223333444455556666")
                << "y" << ObjectId::parse("777788889999aaaabbbbcccc")
                << ValueEnd(),
            "08007800001111222233334444555566660800790000777788889999aaaabbbbcccc00",
            "08010108020200");
        //  "23000000077800111122223333444455556666077900777788889999aaaabbbbcccc00"
    }

    BOOST_AUTO_TEST_CASE(document_with_string_values) {
        check_compressor(Struct()
            << "x" << "A"
            << "y" << "foobar"
            << "z" << std::string("0123456789abcdef0123456789abcdef")
            << ValueEnd(),
            "610078004166007900666f6f62617260007a0030313233343536373839616263646566303132333435363738396162636465660000",
            "6101416602666f6f626172600330313233343536373839616263646566303132333435363738396162636465660000");
        //  "1700000002780002000000410002790002000000610000"
    }
    
    BOOST_AUTO_TEST_CASE(document_with_binary_values) {
        check_compressor(Struct()
            << "x" << Binary("ABC")
            << ValueEnd(),
            "4300780041424300",
            "430141424300");
        //  "10000000057800030000000041424300"
    }

    BOOST_AUTO_TEST_CASE(document_with_array) {
        check_compressor(Struct()
            << "x" << Array()
            << 47 << 62 << "ABC"
            << ValueEnd()
            << ValueEnd(),
            "05007800382f383e634142430000",
            "0501382f383e634142430000");
        //  "260000000478001e0000001030002f0000001031003e00000002320004000000414243000000"
    }

    BOOST_AUTO_TEST_CASE(document_with_array_with_document) {
        check_compressor(Struct()
            << "x" << Array()
            << Struct() << "y" << 47 << ValueEnd()
            << ValueEnd()
            << ValueEnd(),
            "0500780004380079002f000000",
            "05010438022f000000");
        //  "1c000000047800140000000330000c0000001079002f000000000000"
    }

    BOOST_AUTO_TEST_CASE(document_with_array_with_two_documents_2) {
        auto arr = Struct() << "x" << Array();
        arr = std::move(arr) << Struct() << "y" << 47 << ValueEnd();
        arr = std::move(arr) << Struct() << "z" << 62 << ValueEnd();
        auto value = std::move(arr) << ValueEnd() << ValueEnd();
        check_compressor(value,
            "0500780004380079002f000438007a003e000000",
            "05010438022f000438033e000000");
        // "2b000000047800230000000330000c0000001079002f000000000331000c000000107a003e000000000000"
    }

    BOOST_AUTO_TEST_CASE(document_with_array_with_two_documents) {
        auto value = Struct()
                << "x" << Array()
                << Struct() << "y" << 47 << ValueEnd()
                << Struct() << "z" << 62 << ValueEnd()
                << ValueEnd()
                << ValueEnd();
        check_compressor(value,
            "0500780004380079002f000438007a003e000000",
            "05010438022f000438033e000000");
        //  "2b000000047800230000000330000c0000001079002f000000000331000c000000107a003e000000000000"
    }

    BOOST_AUTO_TEST_CASE(document_with_document) {
        auto value = Struct()
                << "x" << Struct()
                << "y" << 47
                << ValueEnd()
                << ValueEnd();
        check_compressor(value,
            "04007800380079002f0000",
            "040138022f0000");
        //  "140000000378000c0000001079002f0000000000"
    }

    BOOST_AUTO_TEST_CASE(document_with_existing_document) {
        auto doc = Struct()
                << "x" << 47
                << ValueEnd();
        auto value = Struct()
                << "X" << doc
                << "y" << 62
                << ValueEnd();
        check_compressor(value,
            "04005800380078002f00380079003e00",
            "040138022f0038033e00");
        //  "1b000000035800130000001078002f0000001079003e0000000000"
    }

    BOOST_AUTO_TEST_CASE(document_with_document_with_document) {
        auto value = Struct()
                << "x" << Struct()
                << "y" << Struct()
                << "z" << 47
                << ValueEnd()
                << ValueEnd()
                << ValueEnd();
        check_compressor(value,
            "040078000400790038007a002f000000",
            "0401040238032f000000");
        //  "1c000000037800140000000379000c000000107a002f000000000000"
    }

BOOST_AUTO_TEST_SUITE_END()
