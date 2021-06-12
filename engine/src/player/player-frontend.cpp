// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./player-frontend.h"

#include "battle-audio/sound-director.h"
#include "battle-gestures/unit-controller.h"
#include "battle-gestures/camera-control.h"
#include "battle-view/battle-view.h"
#include "graphics/graphics.h"


template <typename T>
std::shared_ptr<T> lockOrThrow(std::weak_ptr<T> p) {
  auto r = p.lock();
  if (!r) {
    throw Value{};
  }
  return r;
}


PlayerFrontend::PlayerFrontend(Runtime& runtime, Surface& gestureSurface, Viewport& viewport) :
    runtime_{&runtime},
    gestureSurface_{&gestureSurface},
    viewport_{&viewport}
{
  runtime_->addRuntimeObserver_safe(*this);
}


PlayerFrontend::~PlayerFrontend() {
  LOG_ASSERT(shutdownCompleted());
  LOG_ASSERT(!unitController_ || unitController_->shutdownCompleted());
  LOG_ASSERT(!battleView_ || battleView_->shutdownCompleted());
  LOG_ASSERT(!lobbyFederate_ || lobbyFederate_->shutdownCompleted());
  LOG_ASSERT(!systemFederate_ || systemFederate_->shutdownCompleted());
  runtime_->removeRuntimeObserver_safe(*this);
}


void PlayerFrontend::startup() {
  auto weak_ = weak_from_this();

  lastAnimateSurface_ = std::chrono::system_clock::now();
  backgroundView_ = std::make_unique<BackgroundView>(*viewport_);

  systemFederate_ = std::make_shared<Federate>(*runtime_, "PlayerFrontend", Strand::getMain());

  systemFederate_->getObjectClass("Launcher").observe([weak_](ObjectRef launcher) {
    if (auto this_ = weak_.lock()) {
      if (launcher.justDiscovered()) {
        this_->launcher_ = launcher;
      } else if (launcher.justDestroyed() && launcher == this_->launcher_) {
        this_->launcher_ = ObjectRef{};
      }
      this_->serviceMutex_.lock<void>([this_, launcher]() {
        return this_->handleLauncherChanged(launcher);
      }).done();
    }
  });

  soundDirector_ = std::make_shared<SoundDirector>(systemFederate_);

  systemFederate_->startup(Federation::SystemFederationId);

  //_soundDirector->GetMusicDirector().OnTitleScreen();
}


Promise<void> PlayerFrontend::shutdown_() {
  LOG_ASSERT(Strand::getMain()->isCurrent());

  MutexLock lock = co_await serviceMutex_.lock();

  backgroundView_ = nullptr;

  if (unitController_) {
    auto unitController = std::move(unitController_);
    co_await unitController->shutdown();
  }

  if (battleView_) {
    auto battleView = std::move(battleView_);
    co_await battleView->shutdown();
    LOG_ASSERT(battleView->battleFederate_->shutdownCompleted());
    LOG_ASSERT(battleView.unique());
  }

  if (lobbyFederate_) {
    co_await lobbyFederate_->shutdown();
  }

  if (systemFederate_) {
    co_await systemFederate_->shutdown();
  }
}


CameraState* PlayerFrontend::getUnitControllerCameraState() {
  return unitController_ ? unitController_->GetCameraControl() : nullptr;
}


void PlayerFrontend::animateSurface(bounds2i bounds, float scaling) {
  auto now = std::chrono::system_clock::now();
  float secondsSinceLastUpdate = 0.001f * std::chrono::duration_cast<std::chrono::milliseconds>(now - lastAnimateSurface_).count();
  lastAnimateSurface_ = now;

  if (battleView_) {
      battleView_->animate_(secondsSinceLastUpdate);
  }

  auto weak_ = weak_from_this();
  Strand::getMain()->setImmediate([weak_, bounds, scaling]() {
    if (auto this_ = weak_.lock()) {
      if (auto cameraState = this_->getUnitControllerCameraState())
        cameraState->SetViewportBounds(static_cast<bounds2f>(bounds), scaling);
    }
  });

  if (backgroundView_) {
    backgroundView_->animate(secondsSinceLastUpdate);
  }
}


void PlayerFrontend::renderSurface(Framebuffer* frameBuffer) {
  // if (auto analytics = runtime_->getAnalytics()) {
  //   analytics->sampleRenderFrame();
  // }

    Pipeline{viewport_->getGraphics()->getPipelineInitializer<NullShader>()}
            .clearDepth()
            .clearColor(glm::vec4{159, 155, 147, 255} / 255.0f)
            .render(*viewport_);


  if (battleView_) {
      battleView_->render_(frameBuffer, backgroundView_.get());
  } else {
    if (backgroundView_)
      backgroundView_->render(frameBuffer);
  }
}


void PlayerFrontend::OnTerrainChanged(TerrainFeature terrainFeature, bounds2f bounds) {
  if (battleView_) {
      battleView_->setTerrainDirty(terrainFeature, bounds);
  }
}


void PlayerFrontend::onProcessAuthenticated_main(ObjectId processId, const ProcessAuth& processAuth) {
  if (processId == runtime_->getProcessId()) {
    // if (auto analytics = runtime_->getAnalytics()) {
    //   if (analytics->getViewState() == Analytics::ViewState::None)
    //     analytics->enterLobbyView();
    // }
  }
}


Promise<void> PlayerFrontend::handleLauncherChanged(ObjectRef launcher) {
  co_await tryUpdateCurrentLobby();
  co_await tryUpdateCurrentBattle();
}


Promise<void> PlayerFrontend::handleSessionChanged(ObjectRef session) {
  co_await tryUpdateCurrentBattle();
}


Promise<void> PlayerFrontend::handleMatchChanged(ObjectRef match) {
  if (match.getObjectId() == getLauncherMatchId()) {
    co_await tryUpdateCurrentBattle();
  }
}


ObjectId PlayerFrontend::getLauncherLobbyId() const {
  if (!launcher_) {
    return ObjectId();
  }
  const char* lobbyId = launcher_["lobbyId"_c_str];
  if (!lobbyId || lobbyId[0] == '\0') {
    return ObjectId();
  }
  return ObjectId::parse(lobbyId);
}


ObjectId PlayerFrontend::getLauncherMatchId() const {
  if (battleView_ && battleView_->getPlayerId_() != runtime_->getSubjectId_safe()) {
    return {}; // fulhack: playerId changed => need to reset battle view
  }
  if (launcher_) {
    const char* matchId = launcher_["matchId"_c_str];
    if (matchId && matchId[0] != '\0') {
      return ObjectId::parse(matchId);
    }
  }
  return {};
}


ObjectId PlayerFrontend::getLauncherBattleId() const {
  if (battleView_ && battleView_->getPlayerId_() != runtime_->getSubjectId_safe()) {
    return {}; // fulhack: playerId changed => need to reset battle view
  }
  if (launcher_) {
    const char* battleId = launcher_["battleId"_c_str];
    if (battleId && battleId[0] != '\0') {
      return ObjectId::parse(battleId);
    }
  }
  return {};
}


Promise<void> PlayerFrontend::tryUpdateCurrentLobby() {
  auto currentLobbyId = lobbyFederate_ ? lobbyFederate_->getFederationId() : ObjectId{};
  auto wantedLobbyId = getLauncherLobbyId();
  if (wantedLobbyId != currentLobbyId) {
    co_await joinLobbyFederation(wantedLobbyId);
  }
}


Promise<void> PlayerFrontend::joinLobbyFederation(ObjectId federationId) {
  if (lobbyFederate_) {
    auto federate = std::move(lobbyFederate_);
    co_await federate->shutdown();
  }

  if (federationId) {
    lobbyFederate_ = std::make_shared<Federate>(*runtime_, "PlayerFrontend", Strand::getMain());
    lobbyFederate_->getObjectClass("Session").observe([weak_ = weak_from_this()](ObjectRef session) {
      if (auto this_ = weak_.lock()) {
        this_->serviceMutex_.lock<void>([this_, session]() {
          return this_->handleSessionChanged(session);
        }).done();
      }
    });
    lobbyFederate_->getObjectClass("Match").observe([weak_ = weak_from_this()](ObjectRef match) {
      if (auto this_ = weak_.lock()) {
        this_->serviceMutex_.lock<void>([this_, match]() {
          return this_->handleMatchChanged(match);
        }).done();
      }
    });
    lobbyFederate_->startup(federationId);
  }
}


Promise<void> PlayerFrontend::tryUpdateCurrentBattle() {
  LOG_ASSERT(Strand::getMain()->isCurrent());

  const auto wantedBattleId = getLauncherBattleId();
  const auto currentBattleId = battleView_ ? battleView_->getFederationId() : ObjectId{};
  if (wantedBattleId != currentBattleId) {
    if (unitController_) {
      auto unitController = std::move(unitController_);
      co_await unitController->shutdown();
    }
    if (battleView_) {
      auto battleView = std::move(battleView_);
      co_await battleView->shutdown();
    }

    soundDirector_->StopAll();

    if (wantedBattleId) {
      battleView_ = std::make_shared<BattleView>(*runtime_, *viewport_, soundDirector_);
        battleView_->startup(wantedBattleId, runtime_->getSubjectId_safe());

      unitController_ = std::make_shared<UnitController>(runtime_, gestureSurface_, *viewport_, this, soundDirector_.get());
      unitController_->Startup(wantedBattleId, runtime_->getSubjectId_safe());

      soundDirector_->PlayBackground();
    //  //_soundDirector->GetMusicDirector().OnStartBattle();
    //  // if (auto analytics = runtime_->getAnalytics()) {
    //  //   analytics->enterBattleView();
    //  //   // analytics->enterSetupView();
    //  // }
    // } else {
    //   //_soundDirector->GetMusicDirector().OnTitleScreen();
    //   // if (auto analytics = runtime_->getAnalytics()) {
    //   //   analytics->enterLobbyView();
    //   // }
    }
  }
}