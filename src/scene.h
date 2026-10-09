// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// Omabiblia - scene interface. Each scene renders the reading into the 3D screen's texture.
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "audio.h"
#include "bible.h"
#include "render.h"
#include "theme.h"

class BibleShell;

struct Fonts {
    Font body;      // book-face serif (EB Garamond)
    Font mono;      // the user's Omarchy font (falls back to Share Tech Mono)
    Font display;   // Orbitron - space command HUD
    Font wide;      // Audiowide - titles
    Font pixel;     // Press Start 2P - 8-bit
    Font term;      // VT323 - phosphor terminal
};

struct Ctx {
    Renderer& r;
    Fonts& fonts;
    Palette& pal;
    Bible& bible;
    Chip& chip;
    Ref ref;                 // what is being read
    int refSerial = 0;       // bumps whenever ref (or translation) changes
    bool chapter = false;    // whole chapter vs single verse
    float t = 0, dt = 0;     // seconds
    int w = 1600, h = 1000;  // target size in pixels
    float textScale = 1.0f;  // user text size
    std::string hostname;
    BibleShell* shell = nullptr;   // the hacker scene's shell (bibsh)
};

class Scene {
public:
    virtual ~Scene() = default;
    virtual const char* name() const = 0;
    virtual const char* blurb() const = 0;
    virtual int song() const = 0;
    virtual bool lowRes() const { return false; }   // retro renders at 320x200 and is upscaled with no filtering
    virtual bool wantsTyping() const { return false; }
    virtual void enter(Ctx&) {}
    virtual void draw(Ctx&) = 0;
    virtual void onKeyPan(float, float) {}           // optional extra camera input inside the scene
};

std::unique_ptr<Scene> makeCrawl();
std::unique_ptr<Scene> makeLink();
std::unique_ptr<Scene> makeCommand();
std::unique_ptr<Scene> makeHacker();
std::unique_ptr<Scene> makeRetro();

// shared helpers
std::string upper(const std::string& s);
void clearTarget(V4 c);
