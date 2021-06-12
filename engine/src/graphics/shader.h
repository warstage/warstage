// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GRAPHICS__SHADER_H
#define WARSTAGE__GRAPHICS__SHADER_H

#include "./graphics-api.h"
#include <string>

#define SHADER_SOURCE(source) (#source)


class Shader {
    friend class Program;
    GraphicsApi& graphicsApi_;
    GLuint shader_ = 0;

public:
    Shader(GraphicsApi& graphicsApi, GLenum type, const std::string& source);
    Shader(Shader&&) noexcept;
    Shader(const Shader&) = delete;
    ~Shader();

    Shader& operator=(Shader&&) noexcept;
    Shader& operator=(const Shader&) = delete;

    [[nodiscard]] GraphicsApi& getGraphicsApi() const noexcept { return graphicsApi_; }
};


#endif
