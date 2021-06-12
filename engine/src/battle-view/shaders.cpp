// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./shaders.h"
#include "graphics/graphics.h"


NullShader::NullShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            void main() {
                gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
            }
        }),
        SHADER_SOURCE
        ({
            void main() {
                gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
            }
        })) {
    blendSrcFactor = GL_SRC_ALPHA;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


GradientShader_2f::GradientShader_2f(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            attribute vec3 position;
            attribute vec4 color;
            uniform mat4 transform;
            uniform float point_size;
            varying vec4 v_color;
            void main() {
                vec4 p = transform * vec4(position.x, position.y, position.z, 1.0);
                gl_Position = p;
                gl_PointSize = point_size;
                v_color = color;
            }
        }),
        SHADER_SOURCE
        ({
            varying vec4 v_color;
            void main() {
                gl_FragColor = v_color;
            }
        })) {
    blendSrcFactor = GL_SRC_ALPHA;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


GradientShader_3f::GradientShader_3f(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            attribute vec3 position;
            attribute vec4 color;
            uniform mat4 transform;
            uniform float point_size;
            varying vec4 v_color;
            void main() {
                vec4 p = transform * vec4(position.x, position.y, position.z, 1.0);
                gl_Position = p;
                gl_PointSize = point_size;
                v_color = color;
            }
        }),
        SHADER_SOURCE
        ({
            varying vec4 v_color;
            void main() {
                gl_FragColor = v_color;
            }
        })) {
    blendSrcFactor = GL_SRC_ALPHA;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


PlainShader_2f::PlainShader_2f(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            attribute vec2 position;
            uniform mat4 transform;
            uniform float point_size;
            void main() {
                vec4 p = transform * vec4(position.x, position.y, 0.0, 1.0);
                gl_Position = p;
                gl_PointSize = point_size;
            }
        }),
        SHADER_SOURCE
        ({
            uniform vec4 color;
            void main() {
                gl_FragColor = color;
            }
        })) {
    blendSrcFactor = GL_SRC_ALPHA;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


PlainShader_3f::PlainShader_3f(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            attribute vec3 position;
            uniform mat4 transform;
            uniform float point_size;
            void main() {
                vec4 p = transform * vec4(position.x, position.y, position.z, 1.0);
                gl_Position = p;
                gl_PointSize = point_size;
            }
        }),
        SHADER_SOURCE
        ({
            uniform vec4 color;
            void main() {
                gl_FragColor = color;
            }
        })) {
    blendSrcFactor = GL_SRC_ALPHA;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


TextureShader_2f::TextureShader_2f(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            attribute vec2 position;
            attribute vec2 texcoord;
            varying vec2 _texcoord;
            void main() {
                vec4 p = transform * vec4(position.x, position.y, 0.0, 1.0);
                _texcoord = texcoord;
                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;
            varying vec2 _texcoord;

            void main() {
                gl_FragColor = texture2D(texture, _texcoord);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


TextureShader_3f::TextureShader_3f(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            attribute vec3 position;
            attribute vec2 texcoord;
            varying vec2 _texcoord;
            void main() {
                vec4 p = transform * vec4(position.x, position.y, position.z, 1.0);

                _texcoord = texcoord;

                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;
            varying vec2 _texcoord;
            void main() {
                gl_FragColor = texture2D(texture, _texcoord);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


OpaqueTextureShader_2f::OpaqueTextureShader_2f(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            attribute vec2 position;
            attribute vec2 texcoord;
            varying vec2 _texcoord;

            void main() {
                vec4 p = transform * vec4(position.x, position.y, 0.0, 1.0);
                _texcoord = texcoord;
                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;
            varying vec2 _texcoord;
            void main() {
                gl_FragColor = texture2D(texture, _texcoord);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ZERO;
}


AlphaTextureShader_2f::AlphaTextureShader_2f(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            attribute vec2 position;
            attribute vec2 texcoord;
            varying vec2 _texcoord;
            void main() {
                vec4 p = transform * vec4(position.x, position.y, 0.0, 1.0);
                _texcoord = texcoord;
                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;
            uniform float alpha;
            varying vec2 _texcoord;
            void main() {
                vec4 c = texture2D(texture, _texcoord) * alpha;
                gl_FragColor = c;
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


BillboardColorShader::BillboardColorShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform
            mat4 transform;
            uniform
            vec3 upvector;
            uniform float viewport_height;
            attribute
            vec3 position;
            attribute
            vec4 color;
            attribute float height;
            varying
            vec4 _color;

            void main() {
                float scale = 0.5 * height * viewport_height;
                vec3 position2 = position + scale * upvector;
                vec4 p = transform * vec4(position, 1);
                vec4 q = transform * vec4(position2, 1);
                float s = abs(q.y / q.w - p.y / p.w);

                float a = color.a;
                if (s < 1.0) {
                    a = a * s;
                    s = 1.0;
                }

                _color = vec4(color.rgb, a);
                gl_Position = p;
                gl_PointSize = s;
            }
        }),
        SHADER_SOURCE
        ({
            varying
            vec4 _color;

            void main() {
                gl_FragColor = _color;
            }
        })) {
    blendSrcFactor = GL_SRC_ALPHA;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


BillboardMarkerShader::BillboardMarkerShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE({
            uniform mat4 transform;
            uniform vec3 upvector;
            uniform float viewport_height;
            uniform float min_point_size;
            uniform float max_point_size;
            attribute vec3 position;
            attribute float height;
            attribute vec2 texcoord;
            attribute vec2 texsize;
            varying vec2 _texcoord;
            varying vec2 _texsize;

            void main() {
                vec3 position2 = position + height * 0.5 * viewport_height * upvector;
                vec4 p = transform * vec4(position, 1.0);
                vec4 q = transform * vec4(position2, 1.0);
                float s = clamp(abs(q.y / q.w - p.y / p.w), min_point_size, max_point_size);

                _texcoord = texcoord;
                _texsize = texsize;

                gl_Position = p;
                gl_PointSize = s;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;
            varying vec2 _texcoord;
            varying vec2 _texsize;

            void main() {
                vec4 color = texture2D(texture, _texcoord + gl_PointCoord * _texsize);

                gl_FragColor = color;
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


BillboardMarkerColorShader::BillboardMarkerColorShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE({
            uniform mat4 transform;
            uniform vec3 upvector;
            uniform float viewport_height;
            uniform float min_point_size;
            uniform float max_point_size;
            attribute vec3 position;
            attribute float height;
            attribute vec2 texcoord;
            attribute vec2 texsize;
            attribute vec4 color;
            varying vec2 _texcoord;
            varying vec2 _texsize;
            varying vec4 _color;

            void main() {
                vec3 position2 = position + height * 0.5 * viewport_height * upvector;
                vec4 p = transform * vec4(position, 1.0);
                vec4 q = transform * vec4(position2, 1.0);
                float s = clamp(abs(q.y / q.w - p.y / p.w), min_point_size, max_point_size);

                _texcoord = texcoord;
                _texsize = texsize;
                _color = color;

                gl_Position = p;
                gl_PointSize = s;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;
            varying vec2 _texcoord;
            varying vec2 _texsize;
            varying vec4 _color;

            void main() {
                vec4 color = _color * texture2D(texture, _texcoord + gl_PointCoord * _texsize);
                gl_FragColor = color;
            }
        })) {
    blendSrcFactor = GL_SRC_ALPHA;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}

BillboardTextureShader::BillboardTextureShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            uniform vec3 upvector;
            uniform float viewport_height;
            attribute vec3 position;
            attribute float height;
            attribute vec2 texcoord;
            attribute vec2 texsize;
            varying vec2 _texcoord;
            varying vec2 _texsize;

            void main() {
                vec3 position2 = position + height * 0.5 * viewport_height * upvector;
                vec4 p = transform * vec4(position, 1.0);
                vec4 q = transform * vec4(position2, 1.0);
                float s = abs(q.y / q.w - p.y / p.w);

                _texcoord = texcoord;
                _texsize = texsize;

                gl_Position = p;
                gl_PointSize = s;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;
            varying vec2 _texcoord;
            varying vec2 _texsize;

            void main() {
                vec4 color = texture2D(texture, _texcoord + gl_PointCoord * _texsize);
                if (color.a <= 0.01) {
                    color.a = 0.0;
                    discard;
                }
                gl_FragColor = color;
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


// BillboardSmokeShader::BillboardSmokeShader(Graphics* graphics) : ShaderProgram(graphics->Api(),
//     SHADER_SOURCE
//     ({
//         uniform mat4 transform;
//         uniform vec3 upvector;
//         uniform float viewport_height;
//         attribute vec3 position;
//         attribute float height;
//         attribute vec2 texcoord;
//         attribute vec2 texsize;
//         varying vec2 _texcoord;
//         varying vec2 _texsize;
//
//         void main() {
//             vec3 position2 = position + 0.5 * viewport_height * upvector;
//             vec4 p = transform * vec4(position, 1.0);
//             vec4 q = transform * vec4(position2, 1.0);
//             float s = height * abs(q.y / q.w - p.y / p.w);
//
//             _texcoord = texcoord;
//             _texsize = texsize;
//
//             gl_Position = p;
//             gl_PointSize = s;
//         }
//     }),
//     SHADER_SOURCE
//     ({
//         uniform sampler2D texture;
//         varying vec2 _texcoord;
//         varying vec2 _texsize;
//
//         void main() {
//             vec4 color = texture2D(texture, _texcoord + gl_PointCoord * _texsize);
//             gl_FragColor = color;
//         }
//     })) {
//     blend_sfactor_ = GL_ONE;
//     blend_dfactor_ = GL_ONE_MINUS_SRC_ALPHA;
// }


TerrainInsideShader::TerrainInsideShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            uniform vec4 map_bounds;
            uniform vec3 light_normal;

            attribute vec3 position;
            attribute vec3 normal;

            varying vec3 _position;
            varying vec2 _colorcoord;
            varying vec2 _splatcoord;
            varying float _brightness;

            void main() {
                vec4 p = transform * vec4(position, 1);

                float brightness = -dot(light_normal, normal);

                _position = position;
                _colorcoord = vec2(brightness, 1.0 - (2.5 + position.z) / 128.0);
                _splatcoord = (position.xy - map_bounds.xy) / map_bounds.zw;
                _brightness = brightness;

                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D colormap;
            uniform sampler2D splatmap;

            varying vec3 _position;
            varying vec2 _colorcoord;
            varying vec2 _splatcoord;
            varying float _brightness;

            void main() {
                vec3 color = texture2D(colormap, _colorcoord).rgb;
                vec3 splat = texture2D(splatmap, _splatcoord).rgb;

                color = mix(color, vec3(0.45), 0.4 * step(0.5, splat.r));
                float f = step(0.0, _position.z) * smoothstep(0.475, 0.525, splat.g);
                color = mix(color, vec3(0.2196, 0.3608, 0.1922), 0.25 * f);
                color = mix(color, vec3(0), 0.03 * step(0.5, 1.0 - _brightness));

                gl_FragColor = vec4(color, 1.0);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ZERO;
}


TerrainBorderShader::TerrainBorderShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            uniform vec4 map_bounds;
            uniform vec3 light_normal;

            attribute vec3 position;
            attribute vec3 normal;

            varying vec3 _position;
            varying vec2 _colorcoord;
            varying vec2 _splatcoord;
            varying float _brightness;

            void main() {
                vec4 p = transform * vec4(position, 1);

                float brightness = -dot(light_normal, normal);

                _position = position;
                _colorcoord = vec2(brightness, 1.0 - (2.5 + position.z) / 128.0);
                _splatcoord = (position.xy - map_bounds.xy) / map_bounds.zw;
                _brightness = brightness;

                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D colormap;
            uniform sampler2D splatmap;

            varying vec3 _position;
            varying vec2 _colorcoord;
            varying vec2 _splatcoord;
            varying float _brightness;

            void main() {
                if (distance(_splatcoord, vec2(0.5, 0.5)) > 0.5)
                    discard;

                vec3 color = texture2D(colormap, _colorcoord).rgb;
                vec3 splat = texture2D(splatmap, _splatcoord).rgb;

                color = mix(color, vec3(0.45), 0.4 * step(0.5, splat.r));
                float f = step(0.0, _position.z) * smoothstep(0.475, 0.525, splat.g);
                color = mix(color, vec3(0.2196, 0.3608, 0.1922), 0.3 * f);
                color = mix(color, vec3(0), 0.03 * step(0.5, 1.0 - _brightness));

                gl_FragColor = vec4(color, 1.0);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ZERO;
}


TerrainSkirtShader::TerrainSkirtShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            attribute vec3 position;
            attribute float height;

            uniform mat4 transform;

            varying
            vec2 _colorcoord;
            varying float _height;

            void main() {
                vec4 p = transform * vec4(position, 1);

                _colorcoord = vec2(0.2, 1.0 - (2.5 + height) / 128.0);
                _height = 0.85 * (position.z + 2.5) / (height + 2.5);

                gl_Position = p;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;

            varying vec2 _colorcoord;
            varying float _height;

            void main() {
                vec3 color = texture2D(texture, _colorcoord).rgb;
                color = mix(vec3(0.15), color, _height);

                gl_FragColor = vec4(color, 1);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ZERO;
}


DepthInsideShader::DepthInsideShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;

            attribute vec3 position;
            attribute vec3 normal;

            void main() {
                vec4 p = transform * vec4(position, 1);
                gl_Position = p;
            }
        }),
        SHADER_SOURCE
        ({
            void main() {
                gl_FragColor = vec4(1, 1, 1, 1);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ZERO;
}


DepthBorderShader::DepthBorderShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            uniform vec4 map_bounds;

            attribute vec3 position;
            attribute vec3 normal;

            varying vec2 _terraincoord;

            void main() {
                _terraincoord = (position.xy - map_bounds.xy) / map_bounds.zw;
                vec4 p = transform * vec4(position, 1);
                gl_Position = p;
            }
        }),
        SHADER_SOURCE
        ({
            varying vec2 _terraincoord;

            void main() {
                if (distance(_terraincoord, vec2(0.5, 0.5)) > 0.5)
                    discard;

                gl_FragColor = vec4(1);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ZERO;
}


DepthSkirtShader::DepthSkirtShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;

            attribute vec3 position;
            attribute float height;

            void main() {
                vec4 p = transform * vec4(position, 1);

                gl_Position = p;
            }
        }),
        SHADER_SOURCE
        ({
            void main() {
                gl_FragColor = vec4(1);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ZERO;
}


SobelFilterShader::SobelFilterShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;

            attribute vec2 position;
            attribute vec2 texcoord;

            varying vec2 coord11;
            //varying vec2 coord12;
            varying vec2 coord13;
            //varying vec2 coord21;
            //varying vec2 coord23;
            varying vec2 coord31;
            //varying vec2 coord32;
            varying vec2 coord33;

            void main() {
                const float dx = 1.0 / 2.0 / 1024.0;
                const float dy = 1.0 / 2.0 / 768.0;

                vec4 p = transform * vec4(position, 0, 1);

                gl_Position = p;

                coord11 = texcoord + vec2(-dx, dy);
                //coord12 = texcoord + vec2(0.0,  dy);
                coord13 = texcoord + vec2(dx, dy);
                //coord21 = texcoord + vec2(-dx, 0.0);
                //coord23 = texcoord + vec2( dx, 0.0);
                coord31 = texcoord + vec2(-dx, -dy);
                //coord32 = texcoord + vec2(0.0, -dy);
                coord33 = texcoord + vec2(dx, -dy);
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D depth;

            varying vec2 coord11;
            //varying vec2 coord12;
            varying vec2 coord13;
            //varying vec2 coord21;
            //varying vec2 coord23;
            varying vec2 coord31;
            //varying vec2 coord32;
            varying vec2 coord33;

            void main() {
                float value11 = texture2D(depth, coord11).r;
                //float value12 = texture2D(depth, coord12).r;
                float value13 = texture2D(depth, coord13).r;
                //float value21 = texture2D(depth, coord21).r;
                //float value23 = texture2D(depth, coord23).r;
                float value31 = texture2D(depth, coord31).r;
                //float value32 = texture2D(depth, coord32).r;
                float value33 = texture2D(depth, coord33).r;

                float h = value11 - value33;
                float v = value31 - value13;

                //float h = -value11 - 2.0 * value12 - value13 + value31 + 2.0 * value32 + value33;
                //float v = -value31 - 2.0 * value21 - value11 + value33 + 2.0 * value23 + value13;

                float k = clamp(5.0 * length(vec2(h, v)), 0.0, 0.6);

                //gl_FragColor = vec4(0.145, 0.302, 0.255, k);
                gl_FragColor = vec4(0.0725, 0.151, 0.1275, k);
            }
        })) {
    blendSrcFactor = GL_SRC_ALPHA;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


GroundShadowShader::GroundShadowShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            uniform vec4 map_bounds;

            attribute vec2 position;

            varying vec2 _groundpos;

            void main() {
                vec4 p = transform * vec4(position, -2.5, 1);

                _groundpos = (position - map_bounds.xy) / map_bounds.zw;

                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            varying vec2 _groundpos;

            void main() {
                float d = distance(_groundpos, vec2(0.5, 0.5)) - 0.5;
                float a = clamp(0.3 - d * 24.0, 0.0, 0.3);

                gl_FragColor = vec4(0, 0, 0, a);
            }
        })) {
    blendSrcFactor = GL_SRC_ALPHA;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


HatchingsMasterShader::HatchingsMasterShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            attribute vec2 position;
            attribute vec2 texcoord;
            varying vec2 _texcoord;

            void main() {
                vec4 p = transform * vec4(position.x, position.y, 0, 1);

                _texcoord = texcoord;

                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;
            varying vec2 _texcoord;

            void main() {
                gl_FragColor = texture2D(texture, _texcoord);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ONE;
}


HatchingsInsideShader::HatchingsInsideShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            uniform vec4 map_bounds;
            attribute vec3 position;
            varying vec2 _texcoord;

            void main() {
                vec4 p = transform * vec4(position, 1);

                _texcoord = (position.xy - map_bounds.xy) / map_bounds.zw;

                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;
            varying vec2 _texcoord;

            void main() {
                gl_FragColor = texture2D(texture, _texcoord);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


HatchingsBorderShader::HatchingsBorderShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform mat4 transform;
            uniform vec4 map_bounds;
            attribute vec3 position;

            varying vec2 _texcoord;

            void main() {
                vec4 p = transform * vec4(position, 1);

                _texcoord = (position.xy - map_bounds.xy) / map_bounds.zw;

                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;
            varying vec2 _texcoord;

            void main() {
                if (distance(_texcoord, vec2(0.5, 0.5)) > 0.5)
                    discard;

                gl_FragColor = texture2D(texture, _texcoord);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


HatchingsResultShader::HatchingsResultShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            attribute vec2 position;
            attribute vec2 texcoord;
            uniform mat4 transform;
            varying vec2 _texcoord;

            void main() {
                vec4 p = transform * vec4(position.x, position.y, 0, 1);

                _texcoord = texcoord;

                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            uniform sampler2D texture;
            uniform sampler2D hatch_r;
            uniform sampler2D hatch_g;
            uniform sampler2D hatch_b;
            uniform float hatch_scale;
            varying vec2 _texcoord;

            vec4 mix_hatch(vec4 c1, vec4 c2) {
                return c1 + c2 * (1.0 - c1.a);
            }

            void main() {
                vec2 hatchcoord = gl_FragCoord.xy / hatch_scale;

                vec4 k = texture2D(texture, _texcoord);
                vec4 r = texture2D(hatch_r, hatchcoord) * step(0.5, k.r);
                vec4 g = texture2D(hatch_g, hatchcoord) * step(0.5, k.g);
                vec4 b = texture2D(hatch_b, hatchcoord) * step(0.5, k.b);

                gl_FragColor = mix_hatch(mix_hatch(b, r), g);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


WaterInsideShader::WaterInsideShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),

        SHADER_SOURCE
        ({
            uniform
            mat4 transform;
            uniform
            vec4 map_bounds;
            attribute
            vec2 position;

            void main() {
                vec4 p = transform * vec4(position, 0, 1);

                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            void main() {
                gl_FragColor = vec4(0.44 * 0.5, 0.72 * 0.5, 0.91 * 0.5, 0.5);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}


WaterBorderShader::WaterBorderShader(Graphics* graphics) : PipelineInitializer(graphics->getGraphicsApi(),
        SHADER_SOURCE
        ({
            uniform
            mat4 transform;
            uniform
            vec4 map_bounds;
            attribute
            vec2 position;
            varying
            vec2 _groundpos;

            void main() {
                vec4 p = transform * vec4(position, 0, 1);

                _groundpos = (position - map_bounds.xy) / map_bounds.zw;

                gl_Position = p;
                gl_PointSize = 1.0;
            }
        }),
        SHADER_SOURCE
        ({
            varying
            vec2 _groundpos;

            void main() {
                if (distance(_groundpos, vec2(0.5, 0.5)) > 0.5)
                    discard;

                gl_FragColor = vec4(0.44 * 0.5, 0.72 * 0.5, 0.91 * 0.5, 0.5);
            }
        })) {
    blendSrcFactor = GL_ONE;
    blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
}
