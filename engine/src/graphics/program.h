// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GRAPHICS__PROGRAM_H
#define WARSTAGE__GRAPHICS__PROGRAM_H

#include "./graphics-api.h"
#include <string>

class Shader;


class Program {
    friend class Pipeline;

    GraphicsApi& graphicsApi_;
    GLuint program_ = 0;
    std::vector<std::shared_ptr<Shader>> shaders_ = {};

public:
    explicit Program(GraphicsApi& graphicsApi);
    Program(Program&&) noexcept;
    Program(const Program&) = delete;
    Program(GraphicsApi& graphicsApi, const char* vertexShader, const char* fragmentShader);
    virtual ~Program();

    Program& operator=(Program&&) noexcept;
    Program& operator=(const Program&) = delete;

    [[nodiscard]] GraphicsApi& getGraphicsApi() const noexcept { return graphicsApi_; }

    Program& addVertexShader(const std::string& source);
    Program& addFragmentShader(const std::string& source);
    Program& addShader(std::shared_ptr<Shader> shader);
    Program& linkProgram();
};


#endif
