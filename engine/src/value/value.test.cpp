// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "./value.h"


BOOST_AUTO_TEST_SUITE(value_struct)

	BOOST_AUTO_TEST_CASE(undefined_document)
	{
		Value doc{};

		BOOST_CHECK_EQUAL(true, doc.is_undefined());
	}

	BOOST_AUTO_TEST_CASE(empty_document)
	{
		auto doc = Struct() << ValueEnd();

		BOOST_CHECK_EQUAL(false, doc.is_undefined());
	}

	BOOST_AUTO_TEST_CASE(document_with_basic_values)
	{
		auto doc = Struct()
				<< "x" << 47
				<< "y" << 62
				<< ValueEnd();

		BOOST_CHECK_EQUAL(47, doc["x"_int]);
		BOOST_CHECK_EQUAL(62, doc["y"_int]);
		BOOST_CHECK_EQUAL(true, doc["z"].is_undefined());
	}

	BOOST_AUTO_TEST_CASE(document_with_string_values)
	{
		auto doc = Struct()
				<< "x" << "A"
				<< "y" << std::string("a")
				<< ValueEnd();

		BOOST_CHECK_EQUAL(0, std::strcmp(doc["x"_c_str], "A"));
		BOOST_CHECK_EQUAL(0, std::strcmp(doc["y"_c_str], "a"));
		BOOST_CHECK_EQUAL(true, doc["z"].is_undefined());
	}

	BOOST_AUTO_TEST_CASE(document_with_binary_values)
	{
		auto doc = Struct()
				<< "x" << Binary("ABC")
				<< ValueEnd();

		BOOST_CHECK_EQUAL('A', *reinterpret_cast<const char*>(doc["x"_binary].data));
		BOOST_CHECK_EQUAL(3, doc["x"_binary].size);
	}

	BOOST_AUTO_TEST_CASE(document_with_array)
	{
		auto doc = Struct()
				<< "x" << Array() << 47 << 62 << "ABC" << ValueEnd()
				<< ValueEnd();

		BOOST_CHECK_EQUAL(47, doc["x"]["0"_int]);
		BOOST_CHECK_EQUAL(62, doc["x"]["1"_int]);
		BOOST_CHECK_EQUAL(true, doc["x"]["2"].is_string());
		BOOST_CHECK_EQUAL(true, doc["x"]["x"].is_undefined());
	}

	BOOST_AUTO_TEST_CASE(document_with_document)
	{
		auto doc = Struct()
				<< "x" << Struct()
				<< "y" << 47
				<< ValueEnd()
				<< ValueEnd();

		BOOST_CHECK_EQUAL(47, doc["x"]["y"_int]);
	}

	BOOST_AUTO_TEST_CASE(document_with_document_with_document)
	{
		auto doc = Struct()
				<< "x" << Struct()
				<< "y" << Struct()
				<< "z" << 47
				<< ValueEnd()
				<< ValueEnd()
				<< ValueEnd();

		BOOST_CHECK_EQUAL(47, doc["x"]["y"]["z"_int]);
	}

BOOST_AUTO_TEST_SUITE_END()
