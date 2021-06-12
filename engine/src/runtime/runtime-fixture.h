// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__RUNTIME_FIXTURE_H
#define WARSTAGE__RUNTIME__RUNTIME_FIXTURE_H

#include "runtime/runtime.h"
#include "runtime/mock-endpoint.h"

namespace {
    struct RuntimeFixture {
        std::shared_ptr<Strand_Manual> strand;
        std::shared_ptr<Federate> federate1;
        std::shared_ptr<Federate> federate2;
        virtual void disconnect() = 0;
        virtual void reconnect() = 0;
    };

    struct LocalFixture : public RuntimeFixture {
        std::unique_ptr <Runtime> runtime1;
        LocalFixture() {
            strand = std::make_shared<Strand_Manual>();
            PromiseUtils::strand_ = strand;
            runtime1 = std::make_unique<Runtime>(ProcessType::Player);
            federate1 = std::make_shared<Federate>(*runtime1, "Federate1", strand);
            federate2 = std::make_shared<Federate>(*runtime1, "Federate2", strand);
            federate1->startup(ObjectId::parse("0011223344556677889900112233"));
            federate2->startup(ObjectId::parse("0011223344556677889900112233"));
        }
        ~LocalFixture() {
            federate1->shutdown().done();
            federate2->shutdown().done();
            runtime1->shutdown().done();
            strand->runUntilDone();
        }
        void disconnect() override {
        }
        void reconnect() override {
        }
    };

    struct RemoteFixture : public RuntimeFixture {
        const ObjectId federationId = ObjectId::parse("0011223344556677889900112233");
        std::unique_ptr <Runtime> runtime1;
        std::unique_ptr <Runtime> runtime2;
        std::shared_ptr <MockEndpoint> endpoint1;
        std::shared_ptr <MockEndpoint> endpoint2;
        RemoteFixture() {
            strand = std::make_shared<Strand_Manual>();
            PromiseUtils::strand_ = strand;
            runtime1 = std::make_unique<Runtime>(ProcessType::Daemon);
            runtime2 = std::make_unique<Runtime>(ProcessType::Daemon);
            endpoint1 = std::make_shared<MockEndpoint>(*runtime1, strand);
            endpoint2 = std::make_shared<MockEndpoint>(*runtime2, strand);
            endpoint1->setMasterEndpoint(*endpoint2);
            runtime1->initiateFederation_safe(federationId, FederationType::Battle);
            runtime2->initiateFederation_safe(federationId, FederationType::Battle);
            federate1 = std::make_shared<Federate>(*runtime1, "Federate1", strand);
            federate2 = std::make_shared<Federate>(*runtime2, "Federate2", strand);
            federate1->startup(federationId);
            federate2->startup(federationId);
        }
        ~RemoteFixture() {
            federate1->shutdown().done();
            federate2->shutdown().done();
            endpoint1->shutdown().done();
            endpoint2->shutdown().done();
            runtime1->shutdown().done();
            runtime2->shutdown().done();
            strand->runUntilDone();
        }
        void disconnect() override {
            endpoint2->disconnect();
        }
        void reconnect() override {
            endpoint2->reconnect();
        }
    };

    struct RelayFixture : public RuntimeFixture {
        const ObjectId federationId = ObjectId::parse("0011223344556677889900112233");
        std::unique_ptr <Runtime> runtime1;
        std::unique_ptr <Runtime> runtime2;
        std::unique_ptr <Runtime> runtime3;
        std::shared_ptr <MockEndpoint> endpoint1;
        std::shared_ptr <MockEndpoint> endpoint2;
        std::shared_ptr <MockEndpoint> endpoint3;
        RelayFixture() {
            strand = std::make_shared<Strand_Manual>();
            PromiseUtils::strand_ = strand;
            runtime1 = std::make_unique<Runtime>(ProcessType::Daemon);
            runtime2 = std::make_unique<Runtime>(ProcessType::Daemon);
            runtime3 = std::make_unique<Runtime>(ProcessType::Daemon);
            endpoint1 = std::make_shared<MockEndpoint>(*runtime1, strand);
            endpoint2 = std::make_shared<MockEndpoint>(*runtime2, strand);
            endpoint3 = std::make_shared<MockEndpoint>(*runtime3, strand);
            endpoint1->setMasterEndpoint(*endpoint3);
            endpoint2->setMasterEndpoint(*endpoint3);
            runtime1->initiateFederation_safe(federationId, FederationType::Battle);
            runtime2->initiateFederation_safe(federationId, FederationType::Battle);
            runtime3->initiateFederation_safe(federationId, FederationType::Battle);
            federate1 = std::make_shared<Federate>(*runtime1, "Federate1", strand);
            federate2 = std::make_shared<Federate>(*runtime2, "Federate2", strand);
            federate1->startup(federationId);
            federate2->startup(federationId);
        }
        ~RelayFixture() {
            federate1->shutdown().done();
            federate2->shutdown().done();
            endpoint1->shutdown().done();
            endpoint2->shutdown().done();
            endpoint3->shutdown().done();
            runtime1->shutdown().done();
            runtime2->shutdown().done();
            runtime3->shutdown().done();
            strand->runUntilDone();
        }
        void disconnect() override {
            endpoint3->disconnect();
        }
        void reconnect() override {
            endpoint3->reconnect();
        }
    };

    int count_objects(ObjectClass& objects) {
        int result = 0;
        for (auto i = objects.begin(); i != objects.end(); ++i) {
            ++result;
        }
        return result;
    }
}

#endif
