// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW_BATTLE_ANIMATOR_H
#define WARSTAGE__BATTLE_VIEW_BATTLE_ANIMATOR_H

#include "geometry/bounds.h"
#include "battle-model/battle-vm.h"
#include <random>

class TerrainMap;
class HeightMap;


class BattleAnimator {
  BattleVM::Model* viewModel_;
  std::shared_ptr<SoundDirector> soundDirector_;

  int soundCookieId_ = 0;
  std::ranlux24_base vegetationRng_{};

public:
  BattleAnimator(BattleVM::Model& model, std::shared_ptr<SoundDirector> soundDirector) :
      viewModel_{&model},
      soundDirector_{std::move(soundDirector)} {
  }

  void AddVolleyAndProjectiles(const BattleVM::MissileStats& missileStats, const std::vector<BattleSM::Projectile>& projectiles, float timeToImpact);
  void AddProjectile(BattleVM::Volley& volley, glm::vec3 position1, glm::vec3 position2, float delay, float duration);
  void PlayVolleyReleaseSound(const BattleVM::MissileStats& missileStats, float delay, SoundCookieID soundCookieId);

  void AnimateVolleys(float seconds);
  bool AnimateVolley(BattleVM::Volley& volley, float seconds);

  void AddSmokeParticles(const std::vector<BattleSM::Projectile>& projectiles, const std::string& shape, float scale);
  void AddSmokeParticle(glm::vec3 position1, glm::vec3 position2, float delay, const std::string& shape, float scale);
  void AnimateSmoke(float seconds);

  void UpdateVegetationBody(const bounds2f& bounds);
  void UpdateElementTrajectory();
  void UpdateProjectileTrajectory();
};


#endif
