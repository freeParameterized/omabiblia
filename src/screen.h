// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// Omabiblia - compositor: Omarchy wallpaper backdrop, bloom, and the floating 3D CRT screen.
#pragma once
#include <string>
#include "gl.h"
#include "theme.h"

struct ScreenParams {
    float yaw = 0, pitch = 0, zoom = 1;    // user rotation (radians) and distance factor
    bool flat = false;                     // 2D mode: no 3D screen
    bool cinematic = false;                // slow automatic swing through depth angles
    bool popout = false;                   // transparent window: no wallpaper, the monitor floats on the desktop
    float crt = 1.0f;                      // scanlines / mask / aberration strength 0..1
    float bloom = 1.0f;
    float curvature = 1.0f;
    bool retro = false;                    // dither + posterise (low-res scene)
    int lowW = 320, lowH = 200;
};

class Screen {
public:
    void init();
    void setWallpaper(const std::string& path);
    // sceneTex: HDR scene; draws to the default framebuffer of size (W,H)
    void render(int W, int H, GLuint sceneTex, int sceneW, int sceneH, const Palette& pal, float t, const ScreenParams& p);
    float aspect = 1.6f;                   // screen aspect (16:10)
    float rectX0 = 0, rectY0 = 0, rectX1 = 0, rectY1 = 0;   // last frame: monitor bounds in framebuffer px, y down
private:
    GLuint bgProg_ = 0, brightProg_ = 0, blurProg_ = 0, crtProg_ = 0;
    GLuint wall_ = 0;
    int wallW_ = 0, wallH_ = 0;
    std::string wallPath_;
    RenderTarget bright_, blurA_, blurB_;
    GLuint quadVao_ = 0, quadVbo_ = 0;
};
