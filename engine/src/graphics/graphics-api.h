// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GRAPHICS__GRAPHICS_API_H
#define WARSTAGE__GRAPHICS__GRAPHICS_API_H

#include "value/value.h"
#include <functional>
#include <map>
#include <glm/glm.hpp>
#include <GLES3/gl3.h>

// #define ENABLE_GL_TRACE


class GraphicsApi {
    GLuint lastObjectId_{};
    GLint lastLocation_{};
    using location_info_t = std::pair<GLuint, std::string>;
    std::map<location_info_t, GLint> locationName_{};
    std::map<GLint, location_info_t> locationInfo_{};
    std::function<void(const Value&)> sendMessage_;
    const char* traceFile_ = nullptr;
    int traceLine_ = 0;

public:
    explicit GraphicsApi(std::function<void(const Value& value)> sendMessage) : sendMessage_{std::move(sendMessage)} {
    }

    GraphicsApi& trace(const char* file, int line) {
        traceFile_ = file;
        traceLine_ = line;
        return* this;
    }

    /***/

    void beginFrame(GLuint framebuffer) {
        sendMessage_(build_array() << "BeginFrame" << traceFile_ << traceLine_
                << static_cast<int>(framebuffer)
                << ValueEnd{});
    }

    void endFrame() {
        sendMessage_(build_array() << "EndFrame" << traceFile_ << traceLine_
                << ValueEnd{});
    }

    /***/

    void activeTexture(GLenum texture) {
        //glActiveTexture(texture);
        sendMessage_(build_array() << "ActiveTexture" << traceFile_ << traceLine_
                << static_cast<int>(texture)
                << ValueEnd{});
    }

    void attachShader(GLuint program, GLuint shader) {
        //glAttachShader(program, shader);
        sendMessage_(build_array() << "AttachShader" << traceFile_ << traceLine_
                << static_cast<int>(program)
                << static_cast<int>(shader)
                << ValueEnd{});
    }

    void bindBuffer(GLenum target, GLuint buffer) {
        //glBindBuffer(target, buffer);
        sendMessage_(build_array() << "BindBuffer" << traceFile_ << traceLine_
                << static_cast<int>(target)
                << static_cast<int>(buffer)
                << ValueEnd{});
    }

    void bindFramebuffer(GLenum target, GLuint framebuffer) {
        //glBindFramebuffer(target, framebuffer);
        sendMessage_(build_array() << "BindFramebuffer" << traceFile_ << traceLine_
                << static_cast<int>(target)
                << static_cast<int>(framebuffer)
                << ValueEnd{});
    }

    void bindRenderbuffer(GLenum target, GLuint renderbuffer) {
        //glBindRenderbuffer(target, renderbuffer);
        sendMessage_(build_array() << "BindRenderbuffer" << traceFile_ << traceLine_
                << static_cast<int>(target)
                << static_cast<int>(renderbuffer)
                << ValueEnd{});
    }

    void bindTexture(GLenum target, GLuint texture) {
        //glBindTexture(target, texture);
        sendMessage_(build_array() << "BindTexture" << traceFile_ << traceLine_
                << static_cast<int>(target)
                << static_cast<int>(texture)
                << ValueEnd{});
    }

    void blendFunc(GLenum sfactor, GLenum dfactor) {
        //glBlendFunc(sfactor, dfactor);
        sendMessage_(build_array() << "BlendFunc" << traceFile_ << traceLine_
                << static_cast<int>(sfactor)
                << static_cast<int>(dfactor)
                << ValueEnd{});
    }

    void bufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage) {
        //glBufferData(target, size, data, usage);
        sendMessage_(build_array() << "BufferData" << traceFile_ << traceLine_
                << static_cast<int>(target)
                << Binary{data, static_cast<std::size_t>(size)}
                << static_cast<int>(usage)
                << ValueEnd{});
    }

    void clear(GLbitfield mask) {
        //glClear(mask);
        sendMessage_(build_array() << "Clear" << traceFile_ << traceLine_
                << static_cast<int>(mask)
                << ValueEnd{});
    }

    void clearColor(const glm::vec4& value) {
        //glClear(mask);
        sendMessage_(build_array() << "ClearColor" << traceFile_ << traceLine_
                << static_cast<float>(value.r)
                << static_cast<float>(value.g)
                << static_cast<float>(value.b)
                << static_cast<float>(value.a)
                << ValueEnd{});
    }

    void compileShader(GLuint shader) {
        //glCompileShader(shader);
        sendMessage_(build_array() << "CompileShader" << traceFile_ << traceLine_
                << static_cast<int>(shader)
                << ValueEnd{});
    }

    GLuint createBuffer() {
        //glGenBuffers(1, &id);
        GLuint id = ++lastObjectId_;
        sendMessage_(build_array() << "CreateBuffer" << traceFile_ << traceLine_
                << static_cast<int>(id)
                << ValueEnd{});
        return id;
    }

    GLuint createFramebuffer() {
        //glGenFramebuffers(1, &id);
        GLuint id = ++lastObjectId_;
        sendMessage_(build_array() << "CreateFramebuffer" << traceFile_ << traceLine_
                << static_cast<int>(id)
                << ValueEnd{});
        return id;
    }

    GLuint createProgram() {
        //glCreateProgram();
        auto id = ++lastObjectId_;
        sendMessage_(build_array() << "CreateProgram" << traceFile_ << traceLine_
                << static_cast<int>(id)
                << ValueEnd{});
        return id;
    }

    GLuint createRenderbuffer() {
        //glGenRenderbuffers(1, &id);
        GLuint id = ++lastObjectId_;
        sendMessage_(build_array() << "CreateRenderbuffer" << traceFile_ << traceLine_
                << static_cast<int>(id)
                << ValueEnd{});
        return id;
    }

    GLuint createShader(GLenum type) {
        //glCreateShader(type);
        auto id = ++lastObjectId_;
        sendMessage_(build_array() << "CreateShader" << traceFile_ << traceLine_
                << static_cast<int>(id)
                << static_cast<int>(type)
                << ValueEnd{});
        return id;
    }

    GLuint createTexture() {
        //glGenTextures(1, &id);
        GLuint id = ++lastObjectId_;
        sendMessage_(build_array() << "CreateTexture" << traceFile_ << traceLine_
                << static_cast<int>(id)
                << ValueEnd{});
        return id;
    }

    void deleteBuffer(GLuint id) {
        //glDeleteBuffers(1, &id);
        sendMessage_(build_array() << "DeleteBuffer" << traceFile_ << traceLine_
                << static_cast<int>(id)
                << ValueEnd{});
    }

    void deleteFramebuffer(GLuint id) {
        //glDeleteFramebuffers(1, &id);
        sendMessage_(build_array() << "DeleteFramebuffer" << traceFile_ << traceLine_
                << static_cast<int>(id)
                << ValueEnd{});
    }

    void deleteProgram(GLuint program) {
        //glDeleteProgram(program);
        sendMessage_(build_array() << "DeleteProgram" << traceFile_ << traceLine_
                << static_cast<int>(program)
                << ValueEnd{});
    }

    void deleteRenderbuffer(GLuint id) {
        //glDeleteRenderbuffers(1, &id);
        sendMessage_(build_array() << "DeleteRenderbuffer" << traceFile_ << traceLine_
                << static_cast<int>(id)
                << ValueEnd{});
    }

    void deleteTexture(GLuint id) {
        //glDeleteTextures(1, &id);
        sendMessage_(build_array() << "DeleteTexture" << traceFile_ << traceLine_
                << static_cast<int>(id)
                << ValueEnd{});
    }

    void deleteShader(GLuint shader) {
        //glDeleteShader(shader);
        sendMessage_(build_array() << "DeleteShader" << traceFile_ << traceLine_
                << static_cast<int>(shader)
                << ValueEnd{});
    }

    void depthMask(GLboolean flag) {
        //glDepthMask(flag);
        sendMessage_(build_array() << "DepthMask" << traceFile_ << traceLine_
                << static_cast<int>(flag)
                << ValueEnd{});
    }

    void detachShader(GLuint program, GLuint shader) {
        //glDetachShader(program, shader);
        sendMessage_(build_array() << "DetachShader" << traceFile_ << traceLine_
                << static_cast<int>(program)
                << static_cast<int>(shader)
                << ValueEnd{});
    }

    void disable(GLenum cap) {
        //glDisable(cap);
        sendMessage_(build_array() << "Disable" << traceFile_ << traceLine_
                << static_cast<int>(cap)
                << ValueEnd{});
    }

    void disableVertexAttribArray(GLuint index) {
        //glDisableVertexAttribArray(index);
        auto info = getLocationInfo(index);
        sendMessage_(build_array() << "DisableVertexAttribArray" << traceFile_ << traceLine_
                << static_cast<int>(info.first)
                << info.second
                << ValueEnd{});
    }

    void drawArrays(GLenum mode, GLint first, GLsizei count) {
        //glDrawArrays(mode, first, count);
        sendMessage_(build_array() << "DrawArrays" << traceFile_ << traceLine_
                << static_cast<int>(mode)
                << static_cast<int>(first)
                << static_cast<int>(count)
                << ValueEnd{});
    }

    void drawBuffer(GLenum mode) {
        //glDrawBuffer(mode);
        sendMessage_(build_array() << "DrawBuffer" << traceFile_ << traceLine_
                << static_cast<int>(mode)
                << ValueEnd{});
    }

    void enable(GLenum cap) {
        //glEnable(cap);
        sendMessage_(build_array() << "Enable" << traceFile_ << traceLine_
                << static_cast<int>(cap)
                << ValueEnd{});
    }

    void enableVertexAttribArray(GLuint index) {
        //glEnableVertexAttribArray(index);
        auto info = getLocationInfo(index);
        sendMessage_(build_array() << "EnableVertexAttribArray" << traceFile_ << traceLine_
                << static_cast<int>(info.first)
                << info.second
                << ValueEnd{});
    }

    void framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
        //glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
        sendMessage_(build_array() << "FramebufferRenderbuffer" << traceFile_ << traceLine_
                << static_cast<int>(target)
                << static_cast<int>(attachment)
                << static_cast<int>(renderbuffertarget)
                << static_cast<int>(renderbuffer)
                << ValueEnd{});
    }

    void framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
        //glFramebufferTexture2D(target, attachment, textarget, texture, level);
        sendMessage_(build_array() << "FramebufferTexture2D" << traceFile_ << traceLine_
                << static_cast<int>(target)
                << static_cast<int>(attachment)
                << static_cast<int>(textarget)
                << static_cast<int>(texture)
                << static_cast<int>(level)
                << ValueEnd{});
    }

    void generateMipmap(GLenum target) {
        //glGenerateMipmap(target);
        sendMessage_(build_array() << "GenerateMipmap" << traceFile_ << traceLine_
                << static_cast<int>(target)
                << ValueEnd{});
    }

    GLint getAttribLocation(GLuint program, const GLchar* name) {
        return getLocation(program, name);
    }

    GLint getUniformLocation(GLuint program, const GLchar* name) {
        return getLocation(program, name);
    }

    void lineWidth(GLfloat width) {
        //glLineWidth(width);
        sendMessage_(build_array() << "LineWidth" << traceFile_ << traceLine_
                << static_cast<float>(width)
                << ValueEnd{});
    }

    void linkProgram(GLuint program) {
        //glLinkProgram(program);
        sendMessage_(build_array() << "LinkProgram" << traceFile_ << traceLine_
                << static_cast<int>(program)
                << ValueEnd{});
    }

    void renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
        //glRenderbufferStorage(target, internalformat, width, height);
        sendMessage_(build_array() << "RenderbufferStorage" << traceFile_ << traceLine_
                << static_cast<int>(target)
                << static_cast<int>(internalformat)
                << static_cast<int>(width)
                << static_cast<int>(height)
                << ValueEnd{});
    }

    void shaderSource(GLuint shader, const GLchar* source) {
        std::string str(source);
        str.insert(0, "precision highp float; precision lowp int; ");
        //glShaderSource(shader, count, string, length);
        sendMessage_(build_array() << "ShaderSource" << traceFile_ << traceLine_
                << static_cast<int>(shader)
                << str
                << ValueEnd{});
    }

    void texImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels) {
        //glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
        if (pixels) {
            sendMessage_(build_array() << "TexImage2D" << traceFile_ << traceLine_
                    << static_cast<int>(target)
                    << static_cast<int>(level)
                    << static_cast<int>(internalformat)
                    << static_cast<int>(width)
                    << static_cast<int>(height)
                    << static_cast<int>(border)
                    << static_cast<int>(format)
                    << static_cast<int>(type)
                    << Binary{pixels, texImageSize(width, height, format, type)}
                    << ValueEnd{});
        } else {
            sendMessage_(build_array() << "TexImage2D" << traceFile_ << traceLine_
                    << static_cast<int>(target)
                    << static_cast<int>(level)
                    << static_cast<int>(internalformat)
                    << static_cast<int>(width)
                    << static_cast<int>(height)
                    << static_cast<int>(border)
                    << static_cast<int>(format)
                    << static_cast<int>(type)
                    << nullptr
                    << ValueEnd{});
        }
    }

    void texParameteri(GLenum target, GLenum pname, GLint param) {
        //glTexParameteri(target, pname, param);
        sendMessage_(build_array() << "TexParameteri" << traceFile_ << traceLine_
                << static_cast<int>(target)
                << static_cast<int>(pname)
                << static_cast<int>(param)
                << ValueEnd{});
    }

    void uniform1i(GLint location, int value) {
        //glUniform1iv(location, 1, (const GLint*)&value);
        auto info = getLocationInfo(location);
        sendMessage_(build_array() << "Uniform1i" << traceFile_ << traceLine_
                << static_cast<int>(info.first)
                << info.second
                << value
                << ValueEnd{});
    }

    void uniform1f(GLint location, float value) {
        //glUniform1fv(location, 1, (const GLfloat*)&value);
        auto info = getLocationInfo(location);
        sendMessage_(build_array() << "Uniform1f" << traceFile_ << traceLine_
                << static_cast<int>(info.first)
                << info.second
                << value
                << ValueEnd{});
    }

    void uniform2f(GLint location, const glm::vec2& value) {
        //glUniform2fv(location, 1, reinterpret_cast<const GLfloat*>(&value));
        auto info = getLocationInfo(location);
        sendMessage_(build_array() << "Uniform2f" << traceFile_ << traceLine_
                << static_cast<int>(info.first)
                << info.second
                << value.x
                << value.y
                << ValueEnd{});
    }

    void uniform3f(GLint location, const glm::vec3& value) {
        //glUniform3fv(location, 1, reinterpret_cast<const GLfloat*>(&value));
        auto info = getLocationInfo(location);
        sendMessage_(build_array() << "Uniform3f" << traceFile_ << traceLine_
                << static_cast<int>(info.first)
                << info.second
                << value.x
                << value.y
                << value.z
                << ValueEnd{});
    }

    void uniform4f(GLint location, const glm::vec4& value) {
        //glUniform4fv(location, 1, reinterpret_cast<const GLfloat*>(&value));
        auto info = getLocationInfo(location);
        sendMessage_(build_array() << "Uniform4f" << traceFile_ << traceLine_
                << static_cast<int>(info.first)
                << info.second
                << value.x
                << value.y
                << value.z
                << value.w
                << ValueEnd{});
    }

    void uniformMatrix2f(GLint location, const glm::mat2& value) {
        //glUniformMatrix2fv(location, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(&value));
        auto info = getLocationInfo(location);
        sendMessage_(build_array() << "UniformMatrix2fv" << traceFile_ << traceLine_
                << static_cast<int>(info.first)
                << info.second
                << Binary{&value, sizeof(value)}
                << ValueEnd{});
    }

    void uniformMatrix3f(GLint location, const glm::mat3& value) {
        //glUniformMatrix3fv(location, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(&value));
        auto info = getLocationInfo(location);
        sendMessage_(build_array() << "UniformMatrix3fv" << traceFile_ << traceLine_
                << static_cast<int>(info.first)
                << info.second
                << Binary{&value, sizeof(value)}
                << ValueEnd{});
    }

    void uniformMatrix4f(GLint location, const glm::mat4& value) {
        //glUniformMatrix4fv(location, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(&value));
        auto info = getLocationInfo(location);
        sendMessage_(build_array() << "UniformMatrix4fv" << traceFile_ << traceLine_
                << static_cast<int>(info.first)
                << info.second
                << Binary{&value, sizeof(value)}
                << ValueEnd{});
    }

    void useProgram(GLuint program) {
        //glUseProgram(program);
        sendMessage_(build_array() << "UseProgram" << traceFile_ << traceLine_
                << static_cast<int>(program)
                << ValueEnd{});
    }

    void vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* offset) {
        //glVertexAttribPointer(index, size, type, normalized, stride, pointer);
        auto info = getLocationInfo(index);
        sendMessage_(build_array() << "VertexAttribPointer" << traceFile_ << traceLine_
                << static_cast<int>(info.first)
                << info.second
                << static_cast<int>(size)
                << static_cast<int>(type)
                << static_cast<int>(normalized)
                << static_cast<int>(stride)
                << static_cast<int>(reinterpret_cast<std::size_t>(offset))
                << ValueEnd{});
    }

    void viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
        //glViewport(x, y, width, height);
        sendMessage_(build_array() << "Viewport" << traceFile_ << traceLine_
                << static_cast<int>(x)
                << static_cast<int>(y)
                << static_cast<int>(width)
                << static_cast<int>(height)
                << ValueEnd{});
    }

    /***/

    GLint getLocation(GLuint program, const GLchar* name) {
        if (!name) {
            return -1;
        }
        location_info_t info{program, name};
        auto i = locationName_.find(info);
        if (i != locationName_.end())
            return i->second;

        GLint location = ++lastLocation_;
        locationName_[info] = location;
        locationInfo_[location] = info;
        return location;
    }

    location_info_t getLocationInfo(GLint location) {
        auto i = locationInfo_.find(location);
        if (i != locationInfo_.end())
            return i->second;
        return location_info_t{0, ""};
    }

    static std::size_t texImageSize(GLsizei width, GLsizei height, GLenum format, GLenum type) {
        auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if (format == GL_RGBA)
            size *= 4;
        if (type == GL_UNSIGNED_SHORT)
            size *= 2;
        return size;
    }
};


#endif
