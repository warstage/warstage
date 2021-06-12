// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__SHADERS_H
#define WARSTAGE__BATTLE_VIEW__SHADERS_H

#include "graphics/program.h"
#include "graphics/pipeline.h"

class Graphics;


class NullShader : public PipelineInitializer {
    friend class Graphics;
    explicit NullShader(Graphics* graphics);
};


class GradientShader_2f : public PipelineInitializer {
    friend class Graphics;
    explicit GradientShader_2f(Graphics* graphics);
    // attribute vec3 position;
    // attribute vec4 color;
    // uniform mat4 transform;
    // uniform float point_size;
};


class GradientShader_3f : public PipelineInitializer {
    friend class Graphics;
    explicit GradientShader_3f(Graphics* graphics);
    // attribute vec3 position;
    // attribute vec4 color;
    // uniform mat4 transform;
    // uniform float point_size;
};


class PlainShader_2f : public PipelineInitializer {
    friend class Graphics;
    explicit PlainShader_2f(Graphics* graphics);
    // attribute vec2 position;
    // uniform mat4 transform;
    // uniform float point_size;
    // uniform vec4 color;
};


class PlainShader_3f : public PipelineInitializer {
    friend class Graphics;
    explicit PlainShader_3f(Graphics* graphics);
    // attribute vec2 position;
    // uniform mat4 transform;
    // uniform float point_size;
    // uniform vec4 color;
};


class TextureShader_2f : public PipelineInitializer {
    friend class Graphics;
    explicit TextureShader_2f(Graphics* graphics);
    // attribute vec2 position;
    // attribute vec2 texcoord;
    // uniform mat4 transform;
    // uniform sampler2D texture;
};


class TextureShader_3f : public PipelineInitializer {
    friend class Graphics;
    explicit TextureShader_3f(Graphics* graphics);
    // attribute vec3 position;
    // attribute vec2 texcoord;
    // uniform mat4 transform;
    // uniform sampler2D texture;
};


class OpaqueTextureShader_2f : public PipelineInitializer {
    friend class Graphics;
    explicit OpaqueTextureShader_2f(Graphics* graphics);
    // attribute vec2 position;
    // attribute vec2 texcoord;
    // uniform mat4 transform;
    // uniform sampler2D texture;
};


class AlphaTextureShader_2f : public PipelineInitializer {
    friend class Graphics;
    explicit AlphaTextureShader_2f(Graphics* graphics);
    // attribute vec2 position;
    // attribute vec2 texcoord;
    // uniform mat4 transform;
    // uniform sampler2D texture;
    // uniform float alpha;
};


class BillboardColorShader : public PipelineInitializer {
    friend class Graphics;
    explicit BillboardColorShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute vec4 color;
    // attribute float height;
    // uniform mat4 transform;
    // uniform vec3 upvector;
    // uniform float viewport_height;
};


class BillboardMarkerShader : public PipelineInitializer {
    friend class Graphics;
    explicit BillboardMarkerShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute float height;
    // attribute vec2 texcoord;
    // attribute vec2 texsize;
    // uniform mat4 transform;
    // uniform vec3 upvector;
    // uniform float viewport_height;
    // uniform float min_point_size;
    // uniform float max_point_size;
};

class BillboardMarkerColorShader : public PipelineInitializer {
    friend class Graphics;
    explicit BillboardMarkerColorShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute float height;
    // attribute vec2 texcoord;
    // attribute vec2 texsize;
    // uniform mat4 transform;
    // uniform vec3 upvector;
    // uniform float viewport_height;
    // uniform float min_point_size;
    // uniform float max_point_size;
};


class BillboardTextureShader : public PipelineInitializer {
    friend class Graphics;
    explicit BillboardTextureShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute float height;
    // attribute vec2 texcoord;
    // attribute vec2 texsize;
    // uniform mat4 transform;
    // uniform vec3 upvector;
    // uniform float viewport_height;
};

// class BillboardSmokeShader : public ShaderProgram {
//     friend class Graphics;
//     BillboardSmokeShader(Graphics* graphics);
//     // attribute vec3 position;
//     // attribute float height;
//     // attribute vec2 texcoord;
//     // attribute vec2 texsize;
//
//     // uniform mat4 transform;
//     // uniform vec3 upvector;
//     // uniform float viewport_height;
// };


class TerrainInsideShader : public PipelineInitializer {
    friend class Graphics;
    explicit TerrainInsideShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute vec3 normal;
    // uniform mat4 transform;
    // uniform vec4 map_bounds;
    // uniform vec3 light_normal;
};


class TerrainBorderShader : public PipelineInitializer {
    friend class Graphics;
    explicit TerrainBorderShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute vec3 normal;
    // uniform mat4 transform;
    // uniform vec4 map_bounds;
    // uniform vec3 light_normal;
};


class TerrainSkirtShader : public PipelineInitializer {
    friend class Graphics;
    explicit TerrainSkirtShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute float height;
    // uniform mat4 transform;
};


class DepthInsideShader : public PipelineInitializer {
    friend class Graphics;
    explicit DepthInsideShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute vec3 normal;
    // uniform mat4 transform;
};


class DepthBorderShader : public PipelineInitializer {
    friend class Graphics;
    explicit DepthBorderShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute vec3 normal;
    // uniform mat4 transform;
    // uniform vec4 map_bounds;
};


class DepthSkirtShader : public PipelineInitializer {
    friend class Graphics;
    explicit DepthSkirtShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute float height;
    // uniform mat4 transform;
};


class SobelFilterShader : public PipelineInitializer {
    friend class Graphics;
    explicit SobelFilterShader(Graphics* graphics);
    // attribute vec2 position;
    // attribute vec2 texcoord;
    // uniform mat4 transform;
};


class GroundShadowShader : public PipelineInitializer {
    friend class Graphics;
    explicit GroundShadowShader(Graphics* graphics);
    // attribute vec2 position;
    // uniform mat4 transform;
    // uniform vec4 map_bounds;
};


class HatchingsMasterShader : public PipelineInitializer {
    friend class Graphics;
    explicit HatchingsMasterShader(Graphics* graphics);
    // attribute vec2 position;
    // attribute vec2 texcoord;
    // uniform mat4 transform;
    // uniform sampler2D texture;
};


class HatchingsInsideShader : public PipelineInitializer {
    friend class Graphics;
    explicit HatchingsInsideShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute vec3 normal;
    // uniform mat4 transform;
    // uniform vec4 map_bounds;
};


class HatchingsBorderShader : public PipelineInitializer {
    friend class Graphics;
    explicit HatchingsBorderShader(Graphics* graphics);
    // attribute vec3 position;
    // attribute vec3 normal;
    // uniform mat4 transform;
    // uniform vec4 map_bounds;
};


class HatchingsResultShader : public PipelineInitializer {
    friend class Graphics;
    explicit HatchingsResultShader(Graphics* graphics);
    // attribute vec2 position;
    // attribute vec2 texcoord;
    // uniform mat4 transform;
    // uniform sampler2D texture;
    // uniform sampler2D hatch_r;
    // uniform sampler2D hatch_g;
    // uniform sampler2D hatch_b;
    // uniform float hatch_scale;
};


class WaterInsideShader : public PipelineInitializer {
    friend class Graphics;
    explicit WaterInsideShader(Graphics* graphics);
    // attribute vec2 position;
    // uniform mat4 transform;
    // uniform vec4 map_bounds;
};


class WaterBorderShader : public PipelineInitializer {
    friend class Graphics;
    explicit WaterBorderShader(Graphics* graphics);
    // attribute vec2 position;
    // uniform mat4 transform;
    // uniform vec4 map_bounds;
};


#endif
