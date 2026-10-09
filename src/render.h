// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// Omabiblia - immediate-style batched renderer: 3D text (baked TrueType atlases),
// lines, triangles and point sprites. Colours may exceed 1.0: the bloom pass makes them glow.
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "gl.h"
#include "math.h"

struct Glyph { float x0, y0, x1, y1, u0, v0, u1, v1, advance; };

class Font {
public:
    bool bake(const std::string& path, float pixelHeight, bool pixelArt = false);
    const Glyph* glyph(uint32_t cp) const;
    float measure(const std::string& utf8, float size) const;        // width in "size" units
    std::vector<std::string> wrap(const std::string& utf8, float size, float maxWidth) const;
    float ascent = 0, descent = 0, lineGap = 0, bakeSize = 1;          // in baked pixels
    GLuint tex = 0;
    bool ok = false;
    bool pixel = false;
private:
    std::unordered_map<uint32_t, Glyph> glyphs_;
};

uint32_t utf8Next(const char*& s, const char* end);

struct TextVert { float x, y, z, u, v, r, g, b, a; };
struct ColVert { float x, y, z, r, g, b, a, size; };

class Renderer {
public:
    void init();
    void begin(const M4& viewProj);      // starts a new frame/pass with this camera
    // text on a plane: model maps local (x right, y up, z out) to world; size = cap height-ish in local units
    void text(const Font& f, const std::string& s, const M4& model, float x, float y, float size, V4 color);
    void textCentered(const Font& f, const std::string& s, const M4& model, float y, float size, V4 color);
    void line(V3 a, V3 b, V4 ca, V4 cb);
    void line(V3 a, V3 b, V4 c) { line(a, b, c, c); }
    void tri(V3 a, V3 b, V3 c, V4 col);
    void quad(V3 a, V3 b, V3 c, V3 d, V4 col) { tri(a, b, c, col); tri(a, c, d, col); }
    void point(V3 p, V4 c, float size);
    void circle(V3 center, V3 axisU, V3 axisV, float r, V4 c, int seg = 64);
    void flush();                         // draws everything queued (prims first, then text)
    void setAdditive(bool on) { additive_ = on; }
private:
    void flushText();
    M4 vp_;
    GLuint textProg_ = 0, colProg_ = 0, pointProg_ = 0;
    GLuint vao_ = 0, vbo_ = 0;
    std::vector<ColVert> lines_, tris_, points_;
    std::unordered_map<GLuint, std::vector<TextVert>> text_;
    std::unordered_map<GLuint, bool> textPixel_;
    bool additive_ = true;
};
