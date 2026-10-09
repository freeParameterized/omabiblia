// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// Omabiblia - follows the active Omarchy theme (colours, font, wallpaper) and hot-reloads it.
#pragma once
#include <string>
#include "math.h"

struct Palette {
    std::string name = "fallback";
    bool dark = true;
    V4 background{0.02f, 0.03f, 0.06f, 1}, foreground{0.85f, 0.9f, 1, 1};
    V4 accent{0.2f, 0.9f, 1, 1}, muted{0.3f, 0.35f, 0.45f, 1}, selection{0.1f, 0.2f, 0.3f, 1};
    V4 red{1, 0.25f, 0.4f, 1}, yellow{1, 0.85f, 0.25f, 1}, orange{1, 0.55f, 0.2f, 1}, green{0.3f, 1, 0.5f, 1};
    V4 cyan{0.2f, 0.95f, 1, 1}, blue{0.3f, 0.5f, 1, 1}, magenta{1, 0.3f, 0.9f, 1};

    // derived for the 3D screen: the inside of the screen is always deep (even for light themes,
    // where the theme's own background tints the room around the screen instead)
    V4 deep, glowA, glowB, glowC, warm, ink;
    void derive();
};

struct OmarchyTheme {
    Palette pal;
    std::string fontPath;        // the user's Omarchy font (fc-match), may be empty
    std::string fontName;
    std::string wallpaperPath;   // current background image, may be empty
    long long stamp = 0;         // change detector
    bool load();                 // (re)read everything; returns true if something changed
    bool poll();                 // cheap check, reloads when the theme changed
};

V4 hexColor(const std::string& s, V4 fallback);
V4 neon(V4 c, float boost = 1.0f);   // emissive version of a theme colour (saturated, bright)
