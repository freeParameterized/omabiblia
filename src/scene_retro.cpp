// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// RETRO - 320x200, 8-bit. Synthwave sunset with parallax pixel mountains and a scrolling grid;
// the verse arrives in a JRPG dialogue box, one blip per letter. The compositor dithers and
// posterises it to a chunky palette.
#include "scene.h"

namespace {
class Retro : public Scene {
public:
    const char* name() const override { return "RETRO"; }
    const char* blurb() const override { return "8-bit sunset, dialogue box, blips"; }
    int song() const override { return 4; }
    bool lowRes() const override { return true; }

    void draw(Ctx& c) override {
        const float W = (float)c.w, H = (float)c.h;      // 320 x 200
        if (serial_ != c.refSerial) { serial_ = c.refSerial; page_ = c.ref.verse; typed_ = 0; hold_ = 0; }
        clearTarget(c.pal.deep);
        M4 P = ortho(0, W, 0, H, -1, 1);
        c.r.begin(P);
        c.r.setAdditive(false);
        M4 I = M4::identity();
        float horizon = H * 0.46f;

        // sky: stepped gradient deep -> magenta -> orange near the horizon
        V4 top = scaleRGB(c.pal.deep, 1.0f), mid = scaleRGB(c.pal.glowB, 0.55f), low = scaleRGB(c.pal.warm, 0.9f);
        const int bands = 14;
        for (int i = 0; i < bands; i++) {
            float y0 = horizon + (H - horizon) * i / bands, y1 = horizon + (H - horizon) * (i + 1) / bands;
            float t = (float)i / (bands - 1);
            V4 col = t < 0.5f ? mix4(low, mid, t * 2) : mix4(mid, top, (t - 0.5f) * 2);
            c.r.quad({0, y0, 0}, {W, y0, 0}, {W, y1, 0}, {0, y1, 0}, col);
        }
        // stars
        Rng sr(3);
        for (int i = 0; i < 60; i++) {
            float x = sr.range(0, W), y = sr.range(horizon + 40, H);
            float tw = std::sin(c.t * 3 + i) > 0.6f ? 1.f : 0.6f;
            c.r.quad({x, y, 0}, {x + 1, y, 0}, {x + 1, y + 1, 0}, {x, y + 1, 0}, withAlpha(scaleRGB(c.pal.ink, tw), 1));
        }
        // striped sun
        V3 sc{W * 0.5f, horizon + 30, 0};
        float R = 38;
        for (int row = -(int)R; row < (int)R; row++) {
            float y = sc.y + row;
            if (y < horizon) continue;
            if (row < 0 && ((-row) % 7) < 2 + (-row) / 12) continue;   // the classic slices
            float hw = std::sqrt(R * R - (float)row * row);
            float t = (row + R) / (2 * R);
            V4 col = mix4(scaleRGB(c.pal.glowB, 1.2f), scaleRGB(c.pal.warm, 1.4f), t);
            c.r.quad({sc.x - hw, y, 0}, {sc.x + hw, y, 0}, {sc.x + hw, y + 1, 0}, {sc.x - hw, y + 1, 0}, col);
        }
        // mountains (two parallax layers, blocky)
        mountains(c, horizon, 0.18f, 26, 7, scaleRGB(c.pal.deep, 1.6f), 0.6f);
        mountains(c, horizon, 0.45f, 16, 13, scaleRGB(c.pal.deep, 1.1f), 1.0f);
        // ground + perspective grid
        c.r.quad({0, 0, 0}, {W, 0, 0}, {W, horizon, 0}, {0, horizon, 0}, scaleRGB(c.pal.deep, 0.7f));
        c.r.flush();
        c.r.setAdditive(true);
        V4 gc = scaleRGB(c.pal.glowB, 1.3f);
        for (int i = -16; i <= 16; i++) {
            float xb = W / 2 + i * 34.f;
            c.r.line({W / 2 + i * 3.f, horizon, 0}, {xb, 0, 0}, withAlpha(gc, 0.8f));
        }
        float ph = std::fmod(c.t * 0.9f, 1.f);
        for (int i = 0; i < 12; i++) {
            float z = (i + 1 - ph);
            float y = horizon - horizon * (1.0f / (z * 0.55f + 0.0f + 1e-3f)) * 0.22f;
            if (y < 0 || y > horizon) continue;
            c.r.line({0, y, 0}, {W, y, 0}, withAlpha(gc, 0.85f));
        }
        c.r.line({0, horizon, 0}, {W, horizon, 0}, scaleRGB(c.pal.warm, 1.5f));
        c.r.flush();
        c.r.setAdditive(false);

        // ---- dialogue box ----
        float bx = 8, by = 6, bw = W - 16, bh = 66;
        V4 frame = scaleRGB(c.pal.ink, 1.2f), fill = scaleRGB(c.pal.deep, 0.5f);
        c.r.quad({bx, by, 0}, {bx + bw, by, 0}, {bx + bw, by + bh, 0}, {bx, by + bh, 0}, frame);
        c.r.quad({bx + 2, by + 2, 0}, {bx + bw - 2, by + 2, 0}, {bx + bw - 2, by + bh - 2, 0}, {bx + 2, by + bh - 2, 0}, fill);
        c.r.quad({bx + 3, by + 3, 0}, {bx + bw - 3, by + 3, 0}, {bx + bw - 3, by + 4, 0}, {bx + 3, by + 4, 0}, scaleRGB(c.pal.glowA, 1.2f));
        // name plate
        std::string ref = upper(c.bible.books()[c.ref.book].abbr) + " " + std::to_string(c.ref.chapter) + ":" + std::to_string(page_);
        float nw = c.fonts.pixel.measure(ref, 8) + 12;
        c.r.quad({bx + 6, by + bh - 2, 0}, {bx + 6 + nw, by + bh - 2, 0}, {bx + 6 + nw, by + bh + 12, 0}, {bx + 6, by + bh + 12, 0}, frame);
        c.r.quad({bx + 8, by + bh, 0}, {bx + 4 + nw, by + bh, 0}, {bx + 4 + nw, by + bh + 10, 0}, {bx + 8, by + bh + 10, 0}, scaleRGB(c.pal.deep, 0.6f));
        c.r.flush();
        c.r.text(c.fonts.pixel, ref, I, bx + 12, by + bh + 1, 8, scaleRGB(c.pal.warm, 1.4f));

        // typewriter text, paged through the chapter in chapter mode
        std::string txt = c.bible.verse({c.ref.book, c.ref.chapter, page_});
        auto lines = c.fonts.pixel.wrap(txt, 8, bw - 18);
        int maxLines = 6, totalChars = 0;
        // long verses scroll: show the window of lines that contains the typing head
        std::vector<std::string> vis;
        int shownChars = (int)typed_;
        int lineOfHead = 0, acc = 0;
        for (int i = 0; i < (int)lines.size(); i++) { acc += (int)lines[i].size() + 1; if (acc <= shownChars) lineOfHead = i + 1; }
        int first = std::max(0, std::min(lineOfHead, (int)lines.size() - 1) - maxLines + 1);
        for (auto& l : lines) totalChars += (int)l.size() + 1;
        float before = typed_;
        typed_ = std::min((float)totalChars, typed_ + c.dt * 38);
        if ((int)typed_ != (int)before && (int)typed_ % 2 == 0 && typed_ < totalChars) c.chip.play(Sfx::Blip);
        acc = 0;
        float y = by + bh - 14;
        for (int i = 0; i < (int)lines.size(); i++) {
            int len = (int)lines[i].size();
            int show = std::max(0, std::min(len, shownChars - acc));
            acc += len + 1;
            if (i < first || i >= first + maxLines) continue;
            c.r.text(c.fonts.pixel, lines[i].substr(0, show), I, bx + 9, y, 8, scaleRGB(c.pal.ink, 1.25f));
            y -= 10;
        }
        bool done = typed_ >= totalChars;
        if (done && std::fmod(c.t, 0.8f) < 0.5f) c.r.text(c.fonts.pixel, c.chapter ? "v" : "*", I, bx + bw - 14, by + 5, 8, scaleRGB(c.pal.warm, 1.5f));
        if (done && c.chapter) {
            hold_ += c.dt;
            if (hold_ > 2.5f && page_ < c.bible.verseCount(c.ref.book, c.ref.chapter)) { page_++; typed_ = 0; hold_ = 0; c.chip.play(Sfx::Select); }
        }
        c.r.flush();
        c.r.setAdditive(true);
    }

private:
    void mountains(Ctx& c, float horizon, float parallax, float height, int seed, V4 col, float jag) {
        float W = (float)c.w;
        float off = std::fmod(c.t * 6 * parallax, 64.f);
        Rng r(seed);
        float hs[16];
        for (auto& h : hs) h = r.range(0.25f, 1.0f);
        for (int x = -64; x < (int)W + 64; x += 4) {
            float fx = (x + off) / 64.f;
            int i0 = ((int)std::floor(fx) % 16 + 16) % 16, i1 = (i0 + 1) % 16;
            float f = fx - std::floor(fx);
            float hgt = height * mixf(hs[i0], hs[i1], f) * (1 + jag * 0.15f * ((x / 4) % 2));
            float px = (float)x;
            c.r.quad({px, horizon, 0}, {px + 4, horizon, 0}, {px + 4, horizon + std::floor(hgt), 0}, {px, horizon + std::floor(hgt), 0}, col);
        }
    }
    int serial_ = -1, page_ = 1;
    float typed_ = 0, hold_ = 0;
};
}  // namespace

std::unique_ptr<Scene> makeRetro() { return std::make_unique<Retro>(); }
