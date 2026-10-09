// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// Omabiblia - tiny vector / matrix math (column-major, OpenGL conventions).
#pragma once
#include <cmath>
#include <cstdint>

struct V2 { float x = 0, y = 0; };
struct V3 {
    float x = 0, y = 0, z = 0;
    V3() = default;
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
    V3 operator+(V3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    V3 operator-(V3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    V3 operator*(float s) const { return {x * s, y * s, z * s}; }
    V3 operator-() const { return {-x, -y, -z}; }
};
struct V4 {
    float x = 0, y = 0, z = 0, w = 0;
    V4() = default;
    V4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
    V4(V3 v, float d) : x(v.x), y(v.y), z(v.z), w(d) {}
    V3 xyz() const { return {x, y, z}; }
};

inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 normalize(V3 a) { float l = length(a); return l > 0 ? a * (1.0f / l) : a; }
inline float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
inline float mixf(float a, float b, float t) { return a + (b - a) * t; }
inline V4 mix4(V4 a, V4 b, float t) { return {mixf(a.x, b.x, t), mixf(a.y, b.y, t), mixf(a.z, b.z, t), mixf(a.w, b.w, t)}; }
inline V4 withAlpha(V4 c, float a) { return {c.x, c.y, c.z, a}; }
inline V4 scaleRGB(V4 c, float s) { return {c.x * s, c.y * s, c.z * s, c.w}; }
inline float smoothstepf(float e0, float e1, float x) { float t = clampf((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t); }
constexpr float PI = 3.14159265358979f;

struct M4 {
    float m[16];   // column-major
    static M4 identity() { M4 r{}; for (int i = 0; i < 16; i++) r.m[i] = (i % 5 == 0) ? 1.f : 0.f; return r; }
    M4 operator*(const M4& b) const {
        M4 r{};
        for (int c = 0; c < 4; c++)
            for (int rr = 0; rr < 4; rr++) {
                float s = 0;
                for (int k = 0; k < 4; k++) s += m[k * 4 + rr] * b.m[c * 4 + k];
                r.m[c * 4 + rr] = s;
            }
        return r;
    }
    V4 operator*(V4 v) const {
        return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
                m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
                m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
                m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w};
    }
};

inline M4 perspective(float fovyRad, float aspect, float zn, float zf) {
    float f = 1.0f / std::tan(fovyRad / 2);
    M4 r{};
    r.m[0] = f / aspect; r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1;
    r.m[14] = 2 * zf * zn / (zn - zf);
    return r;
}
inline M4 ortho(float l, float rgt, float b, float t, float zn, float zf) {
    M4 r = M4::identity();
    r.m[0] = 2 / (rgt - l); r.m[5] = 2 / (t - b); r.m[10] = -2 / (zf - zn);
    r.m[12] = -(rgt + l) / (rgt - l); r.m[13] = -(t + b) / (t - b); r.m[14] = -(zf + zn) / (zf - zn);
    return r;
}
inline M4 lookAt(V3 eye, V3 at, V3 up) {
    V3 f = normalize(at - eye), s = normalize(cross(f, up)), u = cross(s, f);
    M4 r = M4::identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye);
    return r;
}
inline M4 translate(V3 t) { M4 r = M4::identity(); r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
inline M4 scale(V3 s) { M4 r = M4::identity(); r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r; }
inline M4 rotX(float a) { M4 r = M4::identity(); float c = std::cos(a), s = std::sin(a); r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r; }
inline M4 rotY(float a) { M4 r = M4::identity(); float c = std::cos(a), s = std::sin(a); r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r; }
inline M4 rotZ(float a) { M4 r = M4::identity(); float c = std::cos(a), s = std::sin(a); r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c; return r; }

// Small deterministic RNG for scene decoration (not for anything security related).
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 0x9E3779B9u) : s(seed ? seed : 1) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (next() >> 8) * (1.0f / 16777216.0f); }          // [0,1)
    float range(float a, float b) { return a + (b - a) * f(); }
};
