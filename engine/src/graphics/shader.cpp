// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./shader.h"


namespace {
    std::string trimBrackets(const std::string& s) {
        return s.size() >= 2 && s[0] == '{' && s[s.size() - 1] == '}'
                ? s.substr(1, s.size() - 2)
                : s;
    }
}


Shader::Shader(GraphicsApi& graphicsApi, GLenum type, const std::string& source) :
        graphicsApi_{graphicsApi},
        shader_{graphicsApi_.createShader(type)} {
    graphicsApi_.shaderSource(shader_, trimBrackets(source).c_str());
    graphicsApi_.compileShader(shader_);
}


Shader::Shader(Shader&& other) noexcept : graphicsApi_{other.graphicsApi_} {
    shader_ = other.shader_;
    other.shader_ = 0;
}


Shader::~Shader() {
    if (shader_) {
        graphicsApi_.deleteShader(shader_);
    }
}


Shader& Shader::operator=(Shader&& other) noexcept {
    assert(&graphicsApi_ == &other.graphicsApi_);
    std::swap(shader_, other.shader_);
    return *this;
}
