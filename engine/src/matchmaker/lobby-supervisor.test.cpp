#include <boost/test/unit_test.hpp>
#include "matchmaker/lobby-supervisor.h"
#include "runtime/runtime.h"
#include "runtime/mock-endpoint.h"
#include "runtime/supervision-policy.h"

namespace {
    class SupervisionPolicy_ : public SupervisionPolicy {
    public:
        std::shared_ptr<Shutdownable> makeSupervisor(Runtime& runtime, FederationType federationType, ObjectId federationId) override {
            if (federationType == FederationType::Lobby) {
                auto result = std::make_shared<LobbySupervisor>(runtime, "Supervisor", Strand::getCurrent(), "");
                result->StartupForTest(federationId);
                return result;
            }
            return nullptr;
        }
    };

    struct Node {
        std::unique_ptr<SupervisionPolicy> supervisionPolicy;
        std::shared_ptr<Runtime> runtime;
        std::shared_ptr<LobbySupervisor> lobby;
        std::shared_ptr<MockEndpoint> endpoint;
        void Startup(const std::shared_ptr<Strand_Manual>& strand, ProcessType processType, const char* lobbyId) {
            if (processType == ProcessType::Daemon) {
                supervisionPolicy = std::make_unique<SupervisionPolicy_>();
            }
            runtime = std::make_shared<Runtime>(processType, supervisionPolicy.get());
            runtime->registerProcessAuth_safe(runtime->getProcessId(), {lobbyId, "", "", lobbyId});
            const auto federationId = ObjectId::parse(lobbyId);
            auto federation = runtime->initiateFederation_safe(federationId, FederationType::Lobby);
            endpoint = std::make_shared<MockEndpoint>(*runtime, strand);
            lobby = std::make_shared<LobbySupervisor>(*runtime, "LobbySupervisor", strand, "http://module");
            federation->setSupervisor(lobby);
            lobby->StartupForTest(federationId);
        }
        void shutdown() {
            supervisionPolicy = nullptr;
            lobby->shutdown().done();
            endpoint->shutdown().done();
            runtime->shutdown().done();
        }
    };
    struct RuntimeFixture {
        std::shared_ptr<Strand_Manual> strand;
        Node daemon;
        Node player;
        std::shared_ptr<Federate> federate;
        RuntimeFixture() {
            strand = std::make_shared<Strand_Manual>();
            PromiseUtils::strand_ = strand;
            daemon.Startup(strand, ProcessType::Daemon, "111111111111111111111111");
            player.Startup(strand, ProcessType::Player, "222222222222222222222222");
            player.endpoint->setMasterEndpoint(*daemon.endpoint);
            strand->runUntilDone();
            player.runtime->initiateFederation_safe(daemon.lobby->getFederationId(), FederationType::Lobby);
            strand->runUntilDone();
        }
        ~RuntimeFixture() {
            if (federate) {
                federate->shutdown().done();
            }
            player.shutdown();
            daemon.shutdown();
            strand->runUntilDone();
        }
    };
}

BOOST_AUTO_TEST_SUITE(matchmaker_lobby_services)

    BOOST_AUTO_TEST_CASE(should_create_offline_match) {
        RuntimeFixture f{};

        f.federate = std::make_shared<Federate>(*f.player.runtime, "Federate", f.strand);
        f.federate->startup(f.player.lobby->getFederationId());
        f.strand->runUntilDone();

        ObjectId matchId;
        f.strand->setImmediate([&]() {
            f.federate->getServiceClass("CreateMatch")
                .request(Struct{} << ValueEnd{})
                .then<void>([&](const Value& response) {
                    matchId = response["match"_ObjectId];
                    BOOST_REQUIRE(matchId);
                }, [&](const std::exception_ptr& e) {
                    BOOST_FAIL(ReasonString(e).c_str());
                })
                .done();
        });
        f.strand->runUntilDone();

        f.strand->setImmediate([&]() {
            auto match = f.federate->getObject(matchId);
            BOOST_REQUIRE(match);
            BOOST_REQUIRE(match["online"].getValue().is_boolean());
            BOOST_REQUIRE(!match["online"_bool]);

        });
        f.strand->runUntilDone();
    }

    BOOST_AUTO_TEST_CASE(should_create_online_match) {
        RuntimeFixture f{};

        f.federate = std::make_shared<Federate>(*f.player.runtime, "Federate", f.strand);
        f.federate->startup(f.daemon.lobby->getFederationId());
        f.strand->runUntilDone();

        ObjectId matchId;
        f.strand->setImmediate([&]() {
            f.federate->getServiceClass("CreateMatch")
                    .request(Struct{} << ValueEnd{})
                    .then<void>([&](const Value& response) {
                        matchId = response["match"_ObjectId];
                        BOOST_REQUIRE(matchId);
                    }, [&](const std::exception_ptr& e) {
                        BOOST_FAIL(ReasonString(e).c_str());
                    })
                    .done();
        });
        f.strand->runUntilDone();

        f.strand->setImmediate([&]() {
            auto match = f.federate->getObject(matchId);
            BOOST_REQUIRE(match);
            BOOST_REQUIRE(match["online"].getValue().is_boolean());
            BOOST_REQUIRE(match["online"_bool]);

        });
        f.strand->runUntilDone();
    }

    BOOST_AUTO_TEST_CASE(should_host_match) {
        RuntimeFixture f{};

        f.federate = std::make_shared<Federate>(*f.player.runtime, "Federate", f.strand);
        f.federate->startup(f.player.lobby->getFederationId());
        f.strand->runUntilDone();

        ObjectId matchId;
        f.strand->setImmediate([&]() {
            f.federate->getServiceClass("CreateMatch")
                    .request(Struct{} << ValueEnd{})
                    .then<void>([&](const Value& response) {
                        matchId = response["match"_ObjectId];
                        BOOST_REQUIRE(matchId);
                    }, [&](const std::exception_ptr& e) {
                        BOOST_FAIL(ReasonString(e).c_str());
                    })
                    .done();
        });
        f.strand->runUntilDone();

        f.strand->setImmediate([&]() {
            auto match = f.federate->getObject(matchId);
            BOOST_REQUIRE(match);
            BOOST_REQUIRE(match["online"].getValue().is_boolean());
            BOOST_REQUIRE(!match["online"_bool]);
        });

        f.strand->setImmediate([&]() {
            f.federate->getServiceClass("HostMatch")
                    .request(Struct{} << "match" << matchId << ValueEnd{})
                    .then<void>([&](auto) {}, [&](const std::exception_ptr& e) {
                        BOOST_FAIL(ReasonString(e).c_str());
                    })
                    .done();
        });
        f.strand->runUntilDone();

        f.strand->setImmediate([&]() {
            auto match = f.federate->getObject(matchId);
            BOOST_REQUIRE(match["online"_bool]);
        });
    }

BOOST_AUTO_TEST_SUITE_END()
