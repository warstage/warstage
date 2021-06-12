#include "async/strand.h"
#include "matchmaker/lobby-supervisor.h"
#include "player/player-window.h"
#include "player/player-endpoint.h"
#include "utilities/logging.h"
#include <iostream>

#define BOOST_TEST_MODULE warstage engine
#define BOOST_TEST_NO_MAIN
#include <boost/test/included/unit_test.hpp>


int main(int argc, char *argv[]) {
    int port = 0;
    for (int i = 1; i != argc; ++i) {
        if (std::strncmp(argv[i], "--port=", 7) == 0) {
            port = std::stoi(argv[i] + 7);
        }
    }

    if (port == 0) {
        return ::boost::unit_test::unit_test_main( &init_unit_test_suite, argc, argv );
    }

    auto playerEndpoint = std::make_shared<PlayerEndpoint>(Strand::io_context());
    playerEndpoint->startup(port);

    boost::asio::signal_set signals{*Strand::io_context()};
    signals.add(SIGHUP);
    signals.add(SIGINT);
    signals.add(SIGQUIT);
    signals.add(SIGTERM);
    signals.async_wait([&playerEndpoint](const boost::system::error_code& error, int signal_number) {
        std::cout << "signal " << signal_number << ", stopping surface endpoint\n";
        auto promise = Promise<void>{}.resolve();
        if (playerEndpoint) {
            promise = promise.onResolve<Promise<void>>([&playerEndpoint]() {
                return playerEndpoint->shutdown();
            }).onResolve<Promise<void>>([&playerEndpoint]() {
                playerEndpoint = nullptr;
                return Promise<void>{}.resolve();
            });
        }
        promise.onResolve<Promise<void>>([]() {
            Strand::stop();
            return Promise<void>{}.resolve();
        }).onReject<void>([](auto& reason) {
            LOG_REJECTION(reason);
        }).done();
    });

    Strand::runUntilStopped(1);
    std::cout << "done\n";

    return 0;
}