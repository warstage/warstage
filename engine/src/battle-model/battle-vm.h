// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_MODEL__BATTLE_VM_H
#define WARSTAGE__BATTLE_MODEL__BATTLE_VM_H

#include "runtime/runtime.h"
#include "battle-audio/sound-director.h"
#include "battle-simulator/battle-objects.h"
#include <unordered_map>

class TerrainMap;
class BattleView;

namespace BattleVM {

  class Unit;

  struct Line {
    std::vector<float> deltas = {};
    std::vector<glm::vec4> colors = {};
  };

  struct LineState {
    std::vector<glm::vec3> points = {};
  };

  // enum class BoneType {
  //   None,
  //   WholeBody
  // };

  // struct Bone {
  //   BoneType type = {};
  // };

  enum class LoopType {
    None = 0,
    Dead      = 1u << 0u,
    Friendly  = 1u << 1u,
    Hostile   = 1u << 2u,
    // NeutralAffiliation = 1u << 3u,
    // UnknownAffiliation
    // Aiming    = 1u << 0u,
    // Dying     = 1u << 0u,
    // Fighting  = 1u << 0u,
    // Firing    = 1u << 0u,
    // Loading   = 1u << 0u,
    // Moving    = 1u << 0u,
    // Parrying  = 1u << 0u,
    // Striking  = 1u << 0u
  };

  struct Loop {
    LoopType type = {};
    int texture = 0;
    std::vector<float> angles = {};
    std::vector<float> vertices = {};
    float duration = 0.0f;
    bool repeat = false;

    static int findLoop(const std::vector<Loop>& loops, LoopType type);
  };

  enum class SkinType {
    None,
    Point,
    Line,
    Billboard, // vertices: { tex.u1, tex.v1, tex.u2, tex.v2 }
    Mesh
  };

  struct Skin {
    int bone = 0;
    SkinType type = {};
    std::vector<Loop> loops = {};
    float adjust = 0.5f - 2.0f / 64.0f; // place texture 2 texels below ground, TODO: should this be here?
  };

  struct SkinState {
    int loop = 0;
    float frame = 0.0f;
    float scale = 1.0f; // TODO: should this be here?
  };

  struct Shape {
    std::string name = {};
    float mass = 0.0f;
    glm::vec3 size = {}; // width, height, depth
    std::vector<Skin> skins = {};
    std::vector<Line> lines = {};
    // std::vector<Bone> bones = {};
  };

  struct BodyState {
    glm::vec3 position = {};
    glm::vec3 velocity = {};
    float orientation = 0.0f;
    std::vector<LineState> lines = {};
    std::vector<SkinState> skins = {};
    bool invisible = false;
  };

  struct Body {
    BackPtr<Shape> shape;
    BodyState state = {};
  };


  struct Element {
    BackPtr<Unit> unit;
    Body body;
  };

  enum class MarkerColor {
    None,
    Alliance,
    Commander
  };

  enum class MarkerState {
    None      = 0,
    Allied    = 1u << 1u,
    Command   = 1u << 2u,
    Dragged   = 1u << 3u,
    Friendly  = 1u << 4u,
    Hovered   = 1u << 5u,
    Hostile   = 1u << 6u,
    Routed    = 1u << 7u,
    Selected  = 1u << 8u
  };

  inline MarkerState operator|(MarkerState s1, MarkerState s2) {
    return static_cast<MarkerState>(static_cast<unsigned int>(s1) | static_cast<unsigned int>(s2));
  }

  inline MarkerState operator&(MarkerState s1, MarkerState s2) {
    return static_cast<MarkerState>(static_cast<unsigned int>(s1) & static_cast<unsigned int>(s2));
  }

  struct MarkerLayer {
    std::array<glm::vec2, 2> vertices = {};
    MarkerColor color = MarkerColor::None;
    MarkerState mask = MarkerState::None;
    MarkerState match = MarkerState::None;

    void SetStateMatch(const MarkerState state, const Value& value) {
      if (value.is_defined()) {
        mask = mask | state;
        if (value._bool()) {
          match = match | state;
        }
      }
    }

    bool IsMatch(const MarkerState state) const {
      return mask == MarkerState::None ? true : (state & mask) == match;
    }
  };

  struct Marker {
    int texture = 0;
    std::vector<MarkerLayer> layers = {};
  };


  struct MissileStats {
    int id = 0;
    std::string trajectoryShape = {};
    std::string releaseShape = {};
    std::string impactShape = {};
  };

  struct Weapon {
    std::vector<MissileStats> missileStats = {};
  };


  class Unit {
  public:
    ObjectRef object = {};
    ObjectId unitId = {};
    ObjectId allianceId = {};

    Marker marker = {};
    std::vector<Element> elements = {};
    std::vector<Weapon> weapons = {};

    ObjectRef unitGestureMarker = {};
    float routingTimer = 0.0f;

    const MissileStats* FindMissileStats(int missileType) const;
    float GetRoutingBlinkTime() const;
    void Animate(float seconds);
  };


  struct MissileRange {
    float angleStart = {};
    float angleLength = {};
    float minimumRange = {};
    float maximumRange = {};
    std::array<float, 25> actualRanges = {};
  };


  struct Vegetation {
    Body body;
  };

  /*struct Particle {
    Body body;
    float timer = 0.0f;
    float duration = 0.0f;
    glm::vec3 origin = {};
    glm::vec3 velocity = {};
    glm::vec3 acceleration = {};
  };*/

  struct SmokeParticle {
    Body body;
    float time = 0.0f;
    float scale = 0.0f;
  };

  struct Projectile {
    Body body;
    glm::vec3 position1 = {};
    glm::vec3 position2 = {};
    float time = 0.0f;
    float duration = 0.0f;
  };

  struct Volley {
    MissileStats missileStats = {};
    SoundCookieID soundCookie = {};
    std::vector<Projectile> projectiles = {};
    bool impacted = false;
  };

  struct Casualty {
    Body body;
    glm::vec3 color = {};
    float time = 0.0f;
  };

  /***/

  struct Model {
    TerrainMap* terrainMap = nullptr;
    std::vector<RootPtr<Unit>> units = {};
    std::unordered_map<std::string, std::vector<RootPtr<Shape>>> shapes = {};
    std::vector<RootPtr<Shape>> defaultShape = {};
    std::vector<Vegetation> vegetation = {};
    std::vector<SmokeParticle> particles = {};
    std::vector<Casualty> casualties;
    std::vector<RootPtr<Volley>> volleys{};

    Model() {
      defaultShape.emplace_back(new Shape{});
    }

    [[nodiscard]] WeakPtr<Unit> FindUnit(ObjectId unitId) const;
    [[nodiscard]] const std::vector<RootPtr<Shape>>& GetShapes(const std::string& name) const;
    [[nodiscard]] BackPtr<Shape> GetShape(const std::string& name) const;
  };

} // namespace BattleVM

#endif
