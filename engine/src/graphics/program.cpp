// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./program.h"
#include "./shader.h"


namespace {
    std::string trimBrackets(const std::string& s) {
        return s.size() >= 2 && s[0] == '{' && s[s.size() - 1] == '}'
                ? s.substr(1, s.size() - 2)
                : s;
    }
}


Program::Program(GraphicsApi& graphicsApi) :
        graphicsApi_{graphicsApi},
        program_{graphicsApi.createProgram()} {
}


Program::Program(Program&& other) noexcept : graphicsApi_{other.graphicsApi_} {
    program_ = other.program_;
    shaders_ = std::move(other.shaders_);
    other.program_ = 0;
    other.shaders_.clear();
}


Program::Program(GraphicsApi& graphicsApi, const char* vertexShader, const char* fragmentShader) :
        graphicsApi_{graphicsApi},
        program_{graphicsApi.createProgram()} {
    addVertexShader(vertexShader);
    addFragmentShader(fragmentShader);
    linkProgram();
}


Program::~Program() {
    if (program_) {
        graphicsApi_.deleteProgram(program_);
    }
}


Program& Program::operator=(Program&& other) noexcept {
    assert(&graphicsApi_ == &other.graphicsApi_);
    std::swap(program_, other.program_);
    std::swap(shaders_, other.shaders_);
    return *this;
}


Program& Program::addVertexShader(const std::string& source) {
    addShader(std::make_shared<Shader>(graphicsApi_, GL_VERTEX_SHADER, source));
    return *this;
}


Program& Program::addFragmentShader(const std::string& source) {
    addShader(std::make_shared<Shader>(graphicsApi_, GL_FRAGMENT_SHADER, source));
    return *this;
}


Program& Program::addShader(std::shared_ptr<Shader> shader) {
    shaders_.push_back(std::move(shader));
    return *this;
}


Program& Program::linkProgram() {
    assert(program_);
    for (auto& shader : shaders_) {
        graphicsApi_.attachShader(program_, shader->shader_);
    }
    graphicsApi_.linkProgram(program_);
    for (auto& shader : shaders_) {
        graphicsApi_.detachShader(program_, shader->shader_);
    }
    shaders_.clear();
    return *this;
}
