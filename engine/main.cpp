#include <iostream>

#define BOOST_TEST_MODULE warstage engine
#define BOOST_TEST_NO_MAIN
#include <boost/test/included/unit_test.hpp>


int main(int argc, char *argv[]) {
    for (auto i = 0; i != argc; ++i) {
        std::cout << argv[i] << ((i < argc - 1) ? ' ' : '\n');
    }

    int port = 0;
    for (int i = 1; i != argc; ++i) {
        if (std::strncmp(argv[i], "--port=", 7) == 0) {
            port = std::stoi(argv[i] + 7);
        }
    }

    if (port == 0) {
        return ::boost::unit_test::unit_test_main( &init_unit_test_suite, argc, argv );
    }
}