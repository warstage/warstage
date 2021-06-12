// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./player-backend.h"

#include "utilities/logging.h"
#include "battle-simulator/battle-simulator.h"
#include "matchmaker/battle-supervisor.h"
#include "matchmaker/lobby-supervisor.h"


PlayerBackend::PlayerBackend(Runtime &runtime) : runtime_{&runtime} {
}


PlayerBackend::~PlayerBackend() {
  LOG_ASSERT(!systemFederate_ || systemFederate_->shutdownCompleted());
  LOG_ASSERT(!battleSupervisor_ || battleSupervisor_->shutdownCompleted());
  LOG_ASSERT(!battleSimulator_ || battleSimulator_->shutdownCompleted());
  LOG_ASSERT(!lobbySupervisor_ || lobbySupervisor_->shutdownCompleted());
}


void PlayerBackend::startup() {
  auto weak_ = weak_from_this();

  systemFederate_ = std::make_shared<Federate>(*runtime_, "PlayerBackend", Strand::getMain());

  systemFederate_->getObjectClass("Launcher").observe([weak_](ObjectRef launcher) {
    if (auto this_ = weak_.lock()) {
      this_->launcherChanged(launcher);
    }
  });

  systemFederate_->getServiceClass("CreateLobby").define([weak_ = weak_from_this()](const auto& params, const auto& subjectId) {
    auto this_ = weak_.lock();
    return this_ ? this_->processCreateLobby(params) : Promise<Value>{}.reject<Value>(Value{});
  });

  systemFederate_->getServiceClass("CreateBattle").define([weak_ = weak_from_this()](const auto& params, const auto& subjectId) {
    auto this_ = weak_.lock();
    return this_ ? this_->processCreateBattle(params) : Promise<Value>{}.reject<Value>(Value{});
  });

  systemFederate_->startup(Federation::SystemFederationId);
}


Promise<void> PlayerBackend::shutdown_() {
  auto systemFederate = std::move(systemFederate_);
  auto battleSupervisor = std::move(battleSupervisor_);
  auto battleSimulator = std::move(battleSimulator_);
  auto lobbySupervisor = std::move(lobbySupervisor_);

  if (systemFederate) {
    co_await systemFederate->shutdown();
  }

  if (battleSupervisor) {
    co_await battleSupervisor->shutdown();
  }

  if (battleSimulator) {
    co_await battleSimulator->shutdown();
  }

  if (lobbySupervisor) {
    co_await lobbySupervisor->shutdown();
  }
}


Promise<Value> PlayerBackend::processCreateLobby(const Value& params) {
  const char* moduleUrl = params["moduleUrl"_c_str];
  if (!moduleUrl || moduleUrl[0] == '\0') {
    return Promise<Value>{}.reject<Value>(REASON(401, "CreateLobby: missing moduleUrl"));
  }
  if (lobbySupervisor_) {
    auto capture_until_done = lobbySupervisor_->shared_from_this();
    lobbySupervisor_->shutdown().onResolve<void>([capture_until_done] {}).done();
    lobbySupervisor_ = nullptr;
  }
  auto federationId = ObjectId::create();
  runtime_->initiateFederation_safe(federationId, FederationType::Lobby);

  lobbySupervisor_ = std::make_shared<LobbySupervisor>(*runtime_,
      "LobbyServices",
      Strand::getMain(),
      moduleUrl);
  lobbySupervisor_->Startup(federationId);

  return resolve(Struct{}
      << "lobbyId" << federationId.str()
      << ValueEnd{});
}


Promise<Value> PlayerBackend::processCreateBattle(const Value& params) {
  auto federationId = ObjectId::create();
  runtime_->initiateFederation_safe(federationId, FederationType::Battle);

  return resolve(Struct{}
      << "battleId" << federationId.str()
      << ValueEnd{});
}


void PlayerBackend::launcherChanged(ObjectRef launcher) {
  if (launcher.justDiscovered()) {
    launcher_ = launcher;
  } else if (launcher.justDestroyed() && launcher == launcher_) {
    launcher_ = ObjectRef{};
  }
  tryUpdateCurrentBattle();
}


void PlayerBackend::tryUpdateCurrentBattle() {
  const auto launcherBattleId = getLauncherBattleId();
  if (launcherBattleId != currentBattleId_) {
    if (battleSupervisor_) {
      auto battleServices = std::move(battleSupervisor_);
      battleServices->shutdown().onResolve<void>([capture_until_done = battleServices] {}).done();
    }

    if (battleSimulator_) {
      auto battleSimulator = std::move(battleSimulator_);
      battleSimulator->shutdown().onResolve<void>([capture_until_done = battleSimulator] {}).done();
    }

    currentBattleId_ = launcherBattleId;

    if (currentBattleId_) {
      battleSimulator_ = std::make_shared<BattleSimulator>(*runtime_);
      battleSimulator_->Startup(currentBattleId_);

      if (const auto lobbyId = getLauncherLobbyId()) {
        battleSupervisor_ = std::make_shared<BattleSupervisor>(*runtime_, "BattleSupervisor", Strand::getMain());
        battleSupervisor_->Startup(lobbyId, currentBattleId_);
      }
    }
  }
}


ObjectId PlayerBackend::getLauncherLobbyId() const {
  if (launcher_) {
    const char* battleId = launcher_["lobbyId"_c_str];
    if (battleId && battleId[0] != '\0') {
      return ObjectId::parse(battleId);
    }
  }
  return {};
}


ObjectId PlayerBackend::getLauncherBattleId() const {
  if (launcher_) {
    const char* battleId = launcher_["battleId"_c_str];
    if (battleId && battleId[0] != '\0') {
      return ObjectId::parse(battleId);
    }
  }
  return {};
}
