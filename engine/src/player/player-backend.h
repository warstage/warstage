// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__PLAYER__PLAYER_BACKEND_H
#define WARSTAGE__PLAYER__PLAYER_BACKEND_H

#include "runtime/runtime.h"

class BattleSupervisor;
class BattleSimulator;
class LobbySupervisor;


class PlayerBackend :
    public Shutdownable,
    public std::enable_shared_from_this<PlayerBackend>
{
  Runtime* runtime_{};
  std::shared_ptr<Federate> systemFederate_{};
  ObjectRef launcher_{};

  std::shared_ptr<LobbySupervisor> lobbySupervisor_{};
  std::shared_ptr<BattleSupervisor> battleSupervisor_{};
  std::shared_ptr<BattleSimulator> battleSimulator_{};
  ObjectId currentBattleId_{};

public:
  explicit PlayerBackend(Runtime& runtime);
  ~PlayerBackend() override;

  void startup();

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

private:
  Promise<Value> processCreateLobby(const Value& params);
  Promise<Value> processCreateBattle(const Value& params);

  void launcherChanged(ObjectRef launcher);
  void tryUpdateCurrentBattle();

  ObjectId getLauncherLobbyId() const;
  ObjectId getLauncherBattleId() const;
};


#endif
