// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
#include "gl.h"
#include <cstdio>

#define OMA_GL_DEFINE(ret, name, ...) PFN_gl##name gl##name = nullptr;
OMA_GL_FUNCS(OMA_GL_DEFINE)
#undef OMA_GL_DEFINE

bool omaLoadGL(void* (*getProc)(const char*)) {
    bool ok = true;
#define OMA_GL_LOAD(ret, name, ...) \
    gl##name = reinterpret_cast<PFN_gl##name>(getProc("gl" #name)); \
    if (!gl##name) { std::fprintf(stderr, "omabiblia: missing GL function gl%s\n", #name); ok = false; }
    OMA_GL_FUNCS(OMA_GL_LOAD)
#undef OMA_GL_LOAD
    return ok;
}

static GLuint compile(GLenum type, const char* name, const char* src) {
    GLuint s = glCreateShader(type);
    const char* parts[2] = {"#version 330 core\n", src};
    glShaderSource(s, 2, parts, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "omabiblia: shader %s (%s) failed:\n%s\n", name, type == GL_VERTEX_SHADER ? "vs" : "fs", log);
    }
    return s;
}

GLuint glxProgram(const char* name, const char* vs, const char* fs) {
    GLuint p = glCreateProgram();
    GLuint a = compile(GL_VERTEX_SHADER, name, vs), b = compile(GL_FRAGMENT_SHADER, name, fs);
    glAttachShader(p, a);
    glAttachShader(p, b);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        std::fprintf(stderr, "omabiblia: program %s link failed:\n%s\n", name, log);
    }
    glDeleteShader(a);
    glDeleteShader(b);
    return p;
}

GLuint glxTexture(int w, int h, const void* rgba, bool linear, bool mipmap) {
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? GL_LINEAR_MIPMAP_LINEAR : (linear ? GL_LINEAR : GL_NEAREST));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (mipmap) glGenerateMipmap(GL_TEXTURE_2D);
    return t;
}

void RenderTarget::ensure(int width, int height, bool withDepth, bool linear, bool hdrFormat) {
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    if (fbo && width == w && height == h) return;
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        if (depth) glDeleteRenderbuffers(1, &depth);
        depth = 0;
    }
    w = width; h = height; hdr = hdrFormat;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    if (hdrFormat) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    else glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (withDepth) {
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    }
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "omabiblia: framebuffer %dx%d incomplete\n", w, h);
}

void RenderTarget::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
}

GLuint g_defaultFbo = 0;
void glxBindDefault(int w, int h) {
    glBindFramebuffer(GL_FRAMEBUFFER, g_defaultFbo);
    glViewport(0, 0, w, h);
}

void glxFullscreen() {
    static GLuint vao = 0;
    if (!vao) glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
