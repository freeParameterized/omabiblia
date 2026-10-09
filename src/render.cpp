// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
#include "render.h"
#include <cstdio>
#include <fstream>
#include <iterator>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "imstb_truetype.h"

uint32_t utf8Next(const char*& s, const char* end) {
    unsigned char c = (unsigned char)*s++;
    if (c < 0x80) return c;
    int n = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
    uint32_t cp = c & (0x3F >> n);
    for (int i = 0; i < n && s < end; i++) cp = (cp << 6) | ((unsigned char)*s++ & 0x3F);
    return cp;
}

bool Font::bake(const std::string& path, float px, bool pixelArt) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::fprintf(stderr, "omabiblia: font not found %s\n", path.c_str()); return false; }
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)), {});
    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, data.data(), stbtt_GetFontOffsetForIndex(data.data(), 0))) return false;
    pixel = pixelArt;
    bakeSize = px;
    const int W = 2048, H = 2048;
    std::vector<unsigned char> alpha(W * H, 0);
    stbtt_pack_context pc;
    stbtt_PackBegin(&pc, alpha.data(), W, H, 0, 2, nullptr);
    if (!pixelArt) stbtt_PackSetOversampling(&pc, 2, 2);
    // Basic Latin + Latin-1 + general punctuation (curly quotes, dashes, ellipsis) + a few symbols
    struct R { int first, count; };
    const R ranges[] = {{32, 95}, {160, 96}, {0x2010, 24}, {0x2190, 4}, {0x25A0, 2}, {0x2588, 1}};
    std::vector<stbtt_packedchar> pcs[6];
    std::vector<stbtt_pack_range> pr;
    for (int i = 0; i < 6; i++) {
        pcs[i].resize(ranges[i].count);
        stbtt_pack_range r{};
        r.font_size = px; r.first_unicode_codepoint_in_range = ranges[i].first;
        r.num_chars = ranges[i].count; r.chardata_for_range = pcs[i].data();
        pr.push_back(r);
    }
    stbtt_PackFontRanges(&pc, data.data(), 0, pr.data(), (int)pr.size());
    stbtt_PackEnd(&pc);
    int a, d, g;
    stbtt_GetFontVMetrics(&info, &a, &d, &g);
    float sc = stbtt_ScaleForPixelHeight(&info, px);
    ascent = a * sc; descent = d * sc; lineGap = g * sc;
    for (int i = 0; i < 6; i++)
        for (int k = 0; k < ranges[i].count; k++) {
            const auto& q = pcs[i][k];
            if (q.x1 == q.x0 && q.xadvance == 0) continue;
            Glyph gl;
            gl.x0 = q.xoff; gl.y0 = -q.yoff2; gl.x1 = q.xoff2; gl.y1 = -q.yoff;   // y up
            gl.u0 = q.x0 / (float)W; gl.v0 = q.y1 / (float)H; gl.u1 = q.x1 / (float)W; gl.v1 = q.y0 / (float)H;
            gl.advance = q.xadvance;
            glyphs_[ranges[i].first + k] = gl;
        }
    std::vector<unsigned char> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) { rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255; rgba[i * 4 + 3] = alpha[i]; }
    tex = glxTexture(W, H, rgba.data(), !pixelArt, !pixelArt);
    ok = true;
    return true;
}

const Glyph* Font::glyph(uint32_t cp) const {
    auto it = glyphs_.find(cp);
    if (it != glyphs_.end()) return &it->second;
    // typographic fallbacks
    switch (cp) {
        case 0x2018: case 0x2019: case 0x201B: cp = '\''; break;
        case 0x201C: case 0x201D: case 0x201F: cp = '"'; break;
        case 0x2013: case 0x2014: case 0x2015: cp = '-'; break;
        case 0x2026: cp = '.'; break;
        default: cp = '?';
    }
    it = glyphs_.find(cp);
    return it == glyphs_.end() ? nullptr : &it->second;
}

float Font::measure(const std::string& s, float size) const {
    float w = 0, k = size / bakeSize;
    const char* p = s.data(); const char* e = p + s.size();
    while (p < e) { if (auto g = glyph(utf8Next(p, e))) w += g->advance * k; }
    return w;
}

std::vector<std::string> Font::wrap(const std::string& s, float size, float maxW) const {
    std::vector<std::string> lines;
    std::string cur, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string trial = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && measure(trial, size) > maxW) { lines.push_back(cur); cur = word; }
        else cur = trial;
        word.clear();
    };
    for (char c : s) {
        if (c == ' ') flushWord();
        else if (c == '\n') { flushWord(); lines.push_back(cur); cur.clear(); }
        else word += c;
    }
    flushWord();
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

// ---------------------------------------------------------------------------------------------
static const char* kTextVS = R"(
layout(location=0) in vec3 aPos; layout(location=1) in vec2 aUV; layout(location=2) in vec4 aCol;
uniform mat4 uVP; out vec2 vUV; out vec4 vCol;
void main(){ vUV=aUV; vCol=aCol; gl_Position=uVP*vec4(aPos,1); })";
static const char* kTextFS = R"(
in vec2 vUV; in vec4 vCol; uniform sampler2D uTex; out vec4 o;
void main(){ float a=texture(uTex,vUV).a; if(a<0.02) discard; o=vec4(vCol.rgb, vCol.a*a); })";
static const char* kColVS = R"(
layout(location=0) in vec3 aPos; layout(location=1) in vec4 aCol; layout(location=2) in float aSize;
uniform mat4 uVP; out vec4 vCol;
void main(){ vCol=aCol; gl_Position=uVP*vec4(aPos,1); gl_PointSize=aSize/max(gl_Position.w,0.05); })";
static const char* kColFS = R"(
in vec4 vCol; out vec4 o; void main(){ o=vCol; })";
static const char* kPointFS = R"(
in vec4 vCol; out vec4 o;
void main(){ vec2 d=gl_PointCoord*2.0-1.0; float r=dot(d,d); if(r>1.0) discard;
  float core=exp(-r*6.0); o=vec4(vCol.rgb*(0.4+1.6*core), vCol.a*(1.0-r)); })";

void Renderer::init() {
    textProg_ = glxProgram("text", kTextVS, kTextFS);
    colProg_ = glxProgram("col", kColVS, kColFS);
    pointProg_ = glxProgram("point", kColVS, kPointFS);
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
}

void Renderer::begin(const M4& vp) { vp_ = vp; }

void Renderer::text(const Font& f, const std::string& s, const M4& model, float x, float y, float size, V4 c) {
    if (!f.ok) return;
    auto& v = text_[f.tex];
    textPixel_[f.tex] = f.pixel;
    float k = size / f.bakeSize;
    const char* p = s.data(); const char* e = p + s.size();
    float pen = x;
    while (p < e) {
        const Glyph* g = f.glyph(utf8Next(p, e));
        if (!g) continue;
        if (g->x1 > g->x0) {
            float x0 = pen + g->x0 * k, x1 = pen + g->x1 * k, y0 = y + g->y0 * k, y1 = y + g->y1 * k;
            V3 q[4] = {(model * V4(x0, y0, 0, 1)).xyz(), (model * V4(x1, y0, 0, 1)).xyz(),
                       (model * V4(x1, y1, 0, 1)).xyz(), (model * V4(x0, y1, 0, 1)).xyz()};
            float uv[4][2] = {{g->u0, g->v0}, {g->u1, g->v0}, {g->u1, g->v1}, {g->u0, g->v1}};
            int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int i : idx) v.push_back({q[i].x, q[i].y, q[i].z, uv[i][0], uv[i][1], c.x, c.y, c.z, c.w});
        }
        pen += g->advance * k;
    }
}

void Renderer::textCentered(const Font& f, const std::string& s, const M4& model, float y, float size, V4 c) {
    text(f, s, model, -f.measure(s, size) / 2, y, size, c);
}

void Renderer::line(V3 a, V3 b, V4 ca, V4 cb) {
    lines_.push_back({a.x, a.y, a.z, ca.x, ca.y, ca.z, ca.w, 1});
    lines_.push_back({b.x, b.y, b.z, cb.x, cb.y, cb.z, cb.w, 1});
}
void Renderer::tri(V3 a, V3 b, V3 c, V4 k) {
    for (V3 p : {a, b, c}) tris_.push_back({p.x, p.y, p.z, k.x, k.y, k.z, k.w, 1});
}
void Renderer::point(V3 p, V4 c, float size) { points_.push_back({p.x, p.y, p.z, c.x, c.y, c.z, c.w, size}); }
void Renderer::circle(V3 o, V3 u, V3 v, float r, V4 c, int seg) {
    for (int i = 0; i < seg; i++) {
        float a0 = 2 * PI * i / seg, a1 = 2 * PI * (i + 1) / seg;
        line(o + u * (r * std::cos(a0)) + v * (r * std::sin(a0)), o + u * (r * std::cos(a1)) + v * (r * std::sin(a1)), c);
    }
}

static void uploadCol(GLuint vao, GLuint vbo, const std::vector<ColVert>& v) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(ColVert), v.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ColVert), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ColVert), (void*)12);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ColVert), (void*)28);
}

void Renderer::flush() {
    glEnable(GL_BLEND);
    if (additive_) glBlendFunc(GL_SRC_ALPHA, GL_ONE); else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!tris_.empty()) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);   // panels/planets are solid-ish
        glUseProgram(colProg_);
        glUniformMatrix4fv(glGetUniformLocation(colProg_, "uVP"), 1, GL_FALSE, vp_.m);
        uploadCol(vao_, vbo_, tris_);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)tris_.size());
        tris_.clear();
        if (additive_) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }
    if (!lines_.empty()) {
        glUseProgram(colProg_);
        glUniformMatrix4fv(glGetUniformLocation(colProg_, "uVP"), 1, GL_FALSE, vp_.m);
        uploadCol(vao_, vbo_, lines_);
        glDrawArrays(GL_LINES, 0, (GLsizei)lines_.size());
        lines_.clear();
    }
    if (!points_.empty()) {
        glEnable(GL_PROGRAM_POINT_SIZE);
        glUseProgram(pointProg_);
        glUniformMatrix4fv(glGetUniformLocation(pointProg_, "uVP"), 1, GL_FALSE, vp_.m);
        uploadCol(vao_, vbo_, points_);
        glDrawArrays(GL_POINTS, 0, (GLsizei)points_.size());
        points_.clear();
    }
    flushText();
}

void Renderer::flushText() {
    if (text_.empty()) return;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(textProg_);
    glUniformMatrix4fv(glGetUniformLocation(textProg_, "uVP"), 1, GL_FALSE, vp_.m);
    glUniform1i(glGetUniformLocation(textProg_, "uTex"), 0);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    for (auto& [tex, v] : text_) {
        if (v.empty()) continue;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(TextVert), v.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TextVert), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVert), (void*)12);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TextVert), (void*)20);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)v.size());
        v.clear();
    }
    if (additive_) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}
