// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./battle-animator.h"
#include "battle-model/terrain-map.h"
#include "battle-model/height-map.h"


void BattleAnimator::AddVolleyAndProjectiles(const BattleVM::MissileStats& missileStats, const std::vector<BattleSM::Projectile>& projectiles, float timeToImpact) {
  auto* volley = new BattleVM::Volley{
      .missileStats = missileStats,
      .soundCookie = static_cast<SoundCookieID>(--soundCookieId_)
  };
  viewModel_->volleys.emplace_back(volley);
  float minimumDelay = 5.0f;
  for (const BattleSM::Projectile& projectile : projectiles) {
    if (!missileStats.trajectoryShape.empty()) {
      glm::vec3 p1 = glm::vec3(projectile.position1, viewModel_->terrainMap->getHeightMap().interpolateHeight(projectile.position1));
      glm::vec3 p2 = glm::vec3(projectile.position2, viewModel_->terrainMap->getHeightMap().interpolateHeight(projectile.position2));
      AddProjectile(*volley, p1, p2, projectile.delay, timeToImpact);
    }
    minimumDelay = glm::min(minimumDelay, projectile.delay);
  }
  PlayVolleyReleaseSound(missileStats, minimumDelay, volley->soundCookie);
  if (!missileStats.releaseShape.empty()) {
    float scale = missileStats.trajectoryShape == "cannonball" ? 4 : 1;
    AddSmokeParticles(projectiles, missileStats.releaseShape, scale);
  }
}


void BattleAnimator::AddProjectile(BattleVM::Volley& volley, glm::vec3 position1, glm::vec3 position2, float delay, float duration) {
  auto projectile = BattleVM::Projectile{
      .body = {
          .shape = viewModel_->GetShape(volley.missileStats.trajectoryShape)
      },
      .position1 = position1,
      .position2 = position2,
      .time = -delay,
      .duration = duration
  };
  for (const auto& trajectory : projectile.body.shape->lines) {
    projectile.body.state.lines.push_back({});
  }
  volley.projectiles.push_back(std::move(projectile));
}


void BattleAnimator::PlayVolleyReleaseSound(const BattleVM::MissileStats& missileStats, float delay, SoundCookieID soundCookieId) {
  if (missileStats.trajectoryShape == "bullet") {
    Strand::getMain()->setImmediate([soundDirector = soundDirector_]() {
      soundDirector->PlayMissileMatchlock();
    });
  }
  if (missileStats.trajectoryShape == "arrow") {
    Strand::getMain()->setImmediate([soundDirector = soundDirector_, soundCookie = soundCookieId]() {
      soundDirector->PlayMissileArrows(soundCookie);
    });
  }
  if (missileStats.trajectoryShape == "cannonball") {
    Strand::getMain()->setTimeout([soundDirector = soundDirector_]() {
      soundDirector->PlayMissileCannon();
    }, delay * 1000);
  }
}


void BattleAnimator::AnimateVolleys(float seconds) {
  std::size_t index = 0;
  while (index < viewModel_->volleys.size()) {
    auto& volley = viewModel_->volleys[index];
    if (AnimateVolley(*volley, seconds)) {
      ++index;
    } else {
      viewModel_->volleys.erase(viewModel_->volleys.begin() + index);
    }
  }
}


bool BattleAnimator::AnimateVolley(BattleVM::Volley& volley, float seconds) {
  bool alive = false;
  bool impact = false;

  for (auto& projectile : volley.projectiles) {
    if (projectile.time < 0.0f) {
      projectile.time += seconds;
      alive = true;
    } else if (projectile.time < projectile.duration) {
      projectile.time += seconds;
      if (projectile.time > projectile.duration)
        projectile.time = projectile.duration;
      alive = true;
    } else if (projectile.time == projectile.duration) {
      projectile.time += 1.0f;
      impact = true;
      alive = true;
    }
  }

  if (impact && !volley.impacted) {
    volley.impacted = true;
    if (!volley.missileStats.impactShape.empty()) {
      std::vector<BattleSM::Projectile> projectiles{};
      for (const auto& p : volley.projectiles) {
        const auto p1 = p.position2;
        const auto p2 = 2.0f * p.position2 - p.position1;
        projectiles.emplace_back(p1, p2, p.duration - p.time);
      }
      float scale = volley.missileStats.trajectoryShape == "cannonball" ? 4 : 1;
      AddSmokeParticles(projectiles, volley.missileStats.impactShape, scale);
    }

    if (volley.missileStats.trajectoryShape == "arrow") {
      Strand::getMain()->setImmediate([soundDirector = soundDirector_]() {
        soundDirector->PlayMissileImpact();
      });
    }
  }

  if (!alive && volley.soundCookie != SoundCookieID::None) {
    Strand::getMain()->setImmediate([soundDirector = soundDirector_, soundCookie = volley.soundCookie]() {
      soundDirector->StopSound(SoundChannelID::MissileArrows, soundCookie);
    });
  }

  return alive;
}


void BattleAnimator::AddSmokeParticles(const std::vector<BattleSM::Projectile>& projectiles, const std::string& shape, float scale) {
  for (const auto& projectile : projectiles) {
    const auto p1 = glm::vec3{projectile.position1, viewModel_->terrainMap->getHeightMap().interpolateHeight(projectile.position1)};
    const auto p2 = glm::vec3{projectile.position2, viewModel_->terrainMap->getHeightMap().interpolateHeight(projectile.position2)};
    AddSmokeParticle(p1, p2, projectile.delay, shape, scale);
  }
}


void BattleAnimator::AddSmokeParticle(glm::vec3 position1, glm::vec3 position2, float delay, const std::string& shape, float scale) {
  const auto dir = glm::normalize(position2 - position1);
  viewModel_->particles.push_back({
      .body = {
          .shape = viewModel_->GetShape(shape),
          .state = {
              .position = position1 + glm::vec3(0, 0, 1.5) + 2.0f * dir,
              .velocity = 4.0f * dir,
              .skins = {{}}
          }
      },
      .time = -delay,
      .scale = scale
  });
}


void BattleAnimator::AnimateSmoke(float seconds) {
  float duration = 3;

  for (auto& particle : viewModel_->particles) {
    auto& body = particle.body;
    if (particle.time < 0) {
      particle.time += seconds;
    } else {
      particle.time += seconds / duration;
      body.state.position += seconds * particle.body.state.velocity;
      body.state.velocity *= exp2f(-4 * seconds);
    }

    std::size_t index = 0;
    for (auto& skin : body.shape->skins) {
      if (index >= body.state.skins.size()) {
        break;
      }
      const auto& loop = skin.loops[body.state.skins[index].loop];
      const auto a = loop.angles.empty() ? 1 : loop.angles.size();
      const auto n = static_cast<int>(loop.vertices.size() / a / 4);
      body.state.skins[index].frame = bounds1i{0, n - 1 }.clamp(std::floorf(n * particle.time));
      body.state.skins[index].scale = particle.scale * (1 + 3 * particle.time);
      ++index;
    }
  }

  viewModel_->particles.erase(std::remove_if(viewModel_->particles.begin(), viewModel_->particles.end(), [](const auto& x) {
    return x.time >= 1;
  }), viewModel_->particles.end());
}


void BattleAnimator::UpdateVegetationBody(const bounds2f& bounds) {
  assert(viewModel_->terrainMap);
  const auto& terrainMap = *viewModel_->terrainMap;
  const auto& heightMap = viewModel_->terrainMap->getHeightMap();

  viewModel_->vegetation.erase(std::remove_if(viewModel_->vegetation.begin(), viewModel_->vegetation.end(), [&bounds](const auto& x) {
    return bounds.contains(x.body.state.position.xy());
  }), viewModel_->vegetation.end());

  vegetationRng_.seed(0);

  const auto& treeShapes = viewModel_->GetShapes("tree");
  if (treeShapes.empty()) {
    return;
  }
  auto randomShape = std::uniform_int_distribution<std::uint32_t>(0, treeShapes.size() - 1);
  auto randomDelta = std::uniform_real_distribution<float>(-0.5f, 0.5f);

  const bounds2f mapbounds = terrainMap.getBounds();
  const glm::vec2 center = mapbounds.mid();
  const float radius = mapbounds.x().size() / 2;

  float d = 5 * mapbounds.x().size() / 1024;
  for (float x = mapbounds.min.x; x < mapbounds.max.x; x += d) {
    for (float y = mapbounds.min.y; y < mapbounds.max.y; y += d) {
      float dx = d * randomDelta(vegetationRng_);
      float dy = d * randomDelta(vegetationRng_);
      const auto shapeIndex = randomShape(vegetationRng_);
      glm::vec2 position = glm::vec2(x + dx, y + dy);
      if (bounds.contains(position) && glm::distance(position, center) < radius) {
        if (heightMap.interpolateHeight(position) > 0 && terrainMap.isForest(position)) {
          const auto p = heightMap.getPosition(position, 0.0f);
          viewModel_->vegetation.push_back({
              .body = {
                  .shape = treeShapes[shapeIndex],
                  .state = {
                      .position = p,
                      .skins = {{}}
                  }
              }
          });
        }
      }
    }
  }
}


void BattleAnimator::UpdateElementTrajectory() {
  assert(viewModel_->terrainMap);
  const auto& heightMap = viewModel_->terrainMap->getHeightMap();
  for (auto& unit : viewModel_->units) {
    for (auto& element : unit->elements) {
      auto& body = element.body;
      const auto height = body.shape->size.y * 0.5f;
      assert(body.state.lines.size() == body.shape->lines.size());
      std::size_t index = 0;
      for (auto& trajectory : body.shape->lines) {
        auto& points = body.state.lines[index].points;
        const auto d = vector2_from_angle(body.state.orientation);
        auto p = body.state.position.xy();
        points.clear();
        for (auto delta : trajectory.deltas) {
          p += d * delta;
          points.push_back({heightMap.getPosition(p, height)});
        }
        ++index;
      }
    }
  }
}


void BattleAnimator::UpdateProjectileTrajectory() {
  assert(viewModel_->terrainMap);
  for (auto& volley : viewModel_->volleys) {
    for (auto& projectile : volley->projectiles) {
      auto& body = projectile.body;

      const auto g = 25.0f;
      const auto dp = projectile.position2 - projectile.position1;
      const auto v = dp / projectile.duration;
      const auto vxy = v.xy();
      const auto vz = v.z + 0.5f * g * projectile.duration;
      const auto speed_xy = glm::length(v.xy());

      assert(body.state.lines.size() == body.shape->lines.size());
      std::size_t index = 0;
      for (auto& trajectory : body.shape->lines) {
        auto& points = body.state.lines[index].points;
        points.clear();
        auto time = projectile.time;
        for (auto delta : trajectory.deltas) {
          time += delta / speed_xy;
          const auto t = bounds1f{0.0, projectile.duration}.clamp(time);
          const auto pxy = projectile.position1.xy() + vxy * t;
          const auto pz = projectile.position1.z + vz * t - 0.5f * g * t * t;
          points.emplace_back(pxy, pz);
        }
        ++index;
      }
    }
  }
}
