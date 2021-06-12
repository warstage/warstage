// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__PLAYER__PLAYER_FRONTEND_H
#define WARSTAGE__PLAYER__PLAYER_FRONTEND_H

#include "runtime/runtime.h"
#include "battle-gestures/editor-model.h"
#include "gesture/surface.h"
#include "async/mutex.h"

#include "battle-view/render-background.h"

class Analytics;
class BattleView;
class CameraState;
class SoundDirector;
class UnitController;


class PlayerFrontend :
    public EditorObserver,
    public RuntimeObserver,
    public Shutdownable,
    public std::enable_shared_from_this<PlayerFrontend>
{
  Runtime* runtime_{};
  Surface* gestureSurface_{};
  Viewport* viewport_{};

  std::chrono::system_clock::time_point lastAnimateSurface_{};
  std::unique_ptr<BackgroundView> backgroundView_{};
  std::shared_ptr<BattleView> battleView_{};
  std::shared_ptr<UnitController> unitController_{};

  std::shared_ptr<SoundDirector> soundDirector_{};

public: std::shared_ptr<Federate> systemFederate_; private: // android
  ObjectRef launcher_{};
  std::shared_ptr<Federate> lobbyFederate_{};

  Mutex serviceMutex_;

public:
  PlayerFrontend(Runtime& runtime, Surface& gestureSurface, Viewport& viewport);
  ~PlayerFrontend() override;

  void startup();

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

  /* EditorObserver */
  void OnTerrainChanged(TerrainFeature terrainFeature, bounds2f bounds) override;

public:
  // Runtime& getRuntime() { return *runtime_; }
  // SoundDirector* getSoundDirector() { return soundDirector_.get(); }

  CameraState* getUnitControllerCameraState();

  /* SurfaceRenderer */
  void animateSurface(bounds2i bounds, float scaling); // render thread
  void renderSurface(Framebuffer* frameBuffer); // render thread

protected:
  void onProcessAuthenticated_main(ObjectId processId, const ProcessAuth& processAuth) override;

private:
  Promise<void> handleLauncherChanged(ObjectRef launcher);
  Promise<void> handleSessionChanged(ObjectRef session);
  Promise<void> handleMatchChanged(ObjectRef match);

  ObjectId getLauncherLobbyId() const;
  ObjectId getLauncherMatchId() const;
  ObjectId getLauncherBattleId() const;

  Promise<void> tryUpdateCurrentLobby();
  Promise<void> joinLobbyFederation(ObjectId federationId);

  Promise<void> tryUpdateCurrentBattle();
};


#endif
