// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GRAPHICS__GRAPHICS_H
#define WARSTAGE__GRAPHICS__GRAPHICS_H

#include "./graphics-api.h"
#include "./pipeline.h"
#include "./shader.h"
#include "async/strand.h"
#include "geometry/bounds.h"
#include <map>
#include <mutex>
#include <string>

#ifdef ENABLE_GL_TRACE
template <class T>
struct GlTrace_ {
    T& v_;
    explicit GlTrace_(T& v) : v_{v} {}
    ~GlTrace_() { v_.trace(nullptr, 0); }
    decltype(auto) trace_(const char* file, int line) { return v_.trace(file, line); }
};
#define GL_TRACE(x) (GlTrace_((x)).trace_(__FILE__, __LINE__))
#else
#define GL_TRACE(x) (x)
#endif


class Framebuffer;
class Program;
class Texture;


class Graphics {
    friend class Pipeline;

    GraphicsApi& graphicsApi_;
    std::map<std::string, std::unique_ptr<PipelineInitializer>> programs_ = {};

public:
    explicit Graphics(GraphicsApi& graphicsApi);
    Graphics(Graphics&&) = delete;
    Graphics(const Graphics&) = delete;
    Graphics& operator=(Graphics&&) = delete;
    Graphics& operator=(const Graphics&) = delete;

    #ifdef ENABLE_GL_TRACE
    Graphics& trace(const char* file, int line) {
        graphicsApi_.trace(file, line);
        return *this;
    }
    #endif

    [[nodiscard]] GraphicsApi& getGraphicsApi() const { return graphicsApi_; }

    template<class PipelineInitializerT> const PipelineInitializer& getPipelineInitializer() {
        std::string name = typeid(PipelineInitializerT).name();
        auto i = programs_.find(name);
        if (i != programs_.end()) {
            return *i->second;
        }
        auto result = new PipelineInitializerT(this);
        programs_[name] =  std::unique_ptr<PipelineInitializer>(result);
        return *result;
    }
};

#endif
