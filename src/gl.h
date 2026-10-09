// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// Omabiblia - minimal OpenGL 3.3 core loader (via glfwGetProcAddress) and GPU helpers.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef char GLchar;
typedef std::ptrdiff_t GLsizeiptr;
typedef std::ptrdiff_t GLintptr;
typedef unsigned char GLubyte;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_LINES 0x0001
#define GL_POINTS 0x0000
#define GL_TRIANGLE_STRIP 0x0005
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_CULL_FACE 0x0B44
#define GL_SCISSOR_TEST 0x0C11
#define GL_PROGRAM_POINT_SIZE 0x8642
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_ZERO 0
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_INT 0x1405
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_REPEAT 0x2901
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_RGBA16F 0x881A
#define GL_RED 0x1903
#define GL_R8 0x8229
#define GL_HALF_FLOAT 0x140B
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_LEQUAL 0x0203
#define GL_LESS 0x0201
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_LINE_SMOOTH 0x0B20

#define OMA_GL_FUNCS(X) \
    X(void, Viewport, GLint, GLint, GLsizei, GLsizei) \
    X(void, ClearColor, GLfloat, GLfloat, GLfloat, GLfloat) \
    X(void, Clear, GLbitfield) \
    X(void, Enable, GLenum) \
    X(void, Disable, GLenum) \
    X(void, BlendFunc, GLenum, GLenum) \
    X(void, DepthMask, GLboolean) \
    X(void, DepthFunc, GLenum) \
    X(void, LineWidth, GLfloat) \
    X(const GLubyte*, GetString, GLenum) \
    X(void, GenTextures, GLsizei, GLuint*) \
    X(void, DeleteTextures, GLsizei, const GLuint*) \
    X(void, BindTexture, GLenum, GLuint) \
    X(void, ActiveTexture, GLenum) \
    X(void, TexParameteri, GLenum, GLenum, GLint) \
    X(void, TexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) \
    X(void, GenerateMipmap, GLenum) \
    X(void, PixelStorei, GLenum, GLint) \
    X(void, GenBuffers, GLsizei, GLuint*) \
    X(void, BindBuffer, GLenum, GLuint) \
    X(void, BufferData, GLenum, GLsizeiptr, const void*, GLenum) \
    X(void, GenVertexArrays, GLsizei, GLuint*) \
    X(void, BindVertexArray, GLuint) \
    X(void, EnableVertexAttribArray, GLuint) \
    X(void, VertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) \
    X(void, DrawArrays, GLenum, GLint, GLsizei) \
    X(GLuint, CreateShader, GLenum) \
    X(void, ShaderSource, GLuint, GLsizei, const GLchar* const*, const GLint*) \
    X(void, CompileShader, GLuint) \
    X(void, GetShaderiv, GLuint, GLenum, GLint*) \
    X(void, GetShaderInfoLog, GLuint, GLsizei, GLsizei*, GLchar*) \
    X(void, DeleteShader, GLuint) \
    X(GLuint, CreateProgram, void) \
    X(void, AttachShader, GLuint, GLuint) \
    X(void, LinkProgram, GLuint) \
    X(void, GetProgramiv, GLuint, GLenum, GLint*) \
    X(void, GetProgramInfoLog, GLuint, GLsizei, GLsizei*, GLchar*) \
    X(void, UseProgram, GLuint) \
    X(GLint, GetUniformLocation, GLuint, const GLchar*) \
    X(void, Uniform1i, GLint, GLint) \
    X(void, Uniform1f, GLint, GLfloat) \
    X(void, Uniform2f, GLint, GLfloat, GLfloat) \
    X(void, Uniform3f, GLint, GLfloat, GLfloat, GLfloat) \
    X(void, Uniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat) \
    X(void, UniformMatrix4fv, GLint, GLsizei, GLboolean, const GLfloat*) \
    X(void, GenFramebuffers, GLsizei, GLuint*) \
    X(void, BindFramebuffer, GLenum, GLuint) \
    X(void, FramebufferTexture2D, GLenum, GLenum, GLenum, GLuint, GLint) \
    X(GLenum, CheckFramebufferStatus, GLenum) \
    X(void, DeleteFramebuffers, GLsizei, const GLuint*) \
    X(void, GenRenderbuffers, GLsizei, GLuint*) \
    X(void, BindRenderbuffer, GLenum, GLuint) \
    X(void, RenderbufferStorage, GLenum, GLenum, GLsizei, GLsizei) \
    X(void, FramebufferRenderbuffer, GLenum, GLenum, GLenum, GLuint) \
    X(void, DeleteRenderbuffers, GLsizei, const GLuint*) \
    X(void, ReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)

#define OMA_GL_DECLARE(ret, name, ...) typedef ret (*PFN_gl##name)(__VA_ARGS__); extern PFN_gl##name gl##name;
OMA_GL_FUNCS(OMA_GL_DECLARE)
#undef OMA_GL_DECLARE

bool omaLoadGL(void* (*getProc)(const char*));

// ---- helpers ----
GLuint glxProgram(const char* name, const char* vs, const char* fs);
GLuint glxTexture(int w, int h, const void* rgba, bool linear, bool mipmap);

struct RenderTarget {
    GLuint fbo = 0, tex = 0, depth = 0;
    int w = 0, h = 0;
    bool hdr = false;
    void ensure(int width, int height, bool withDepth, bool linear = true, bool hdrFormat = true);
    void bind() const;
};
void glxBindDefault(int w, int h);
extern GLuint g_defaultFbo;   // where the final frame goes (0 = window; an FBO for --shot)

// Fullscreen triangle (no VBO needed; vertex id driven) - call with a program bound.
void glxFullscreen();
