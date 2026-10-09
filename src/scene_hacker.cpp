// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// HACKER - a phosphor terminal over digital rain made from the text itself. The prompt is bibsh
// (src/shell.cpp): the Bible as a read-only filesystem - ls, cd, cat jer/29:11, grep, pipes.
#include "scene.h"
#include "shell.h"

namespace {
struct Drop { float x, y, speed; int len; uint32_t seed; };

class Hacker : public Scene {
public:
    const char* name() const override { return "HACKER"; }
    const char* blurb() const override { return "phosphor terminal; type a reference and press enter"; }
    int song() const override { return 3; }
    bool wantsTyping() const override { return true; }

    void draw(Ctx& c) override {
        clearTarget(scaleRGB(c.pal.deep, 0.6f));
        BibleShell& sh = *c.shell;
        // typewriter for the newest reading only; everything older is already "printed"
        if (serial_ != sh.typeSerial) { serial_ = sh.typeSerial; typed_ = 0; }
        size_t typeChars = 0;
        if (sh.typeFrom != (size_t)-1) for (size_t i = sh.typeFrom; i < sh.lines.size(); i++) typeChars += sh.lines[i].s.size() + 1;
        float before = typed_;
        typed_ = std::min((float)typeChars, typed_ + c.dt * (typeChars > 600 ? 900.f : 110.f));
        if ((int)(typed_ / 4) != (int)(before / 4) && typed_ < typeChars) c.chip.play(Sfx::Key);

        float W = (float)c.w, H = (float)c.h;
        M4 hud = ortho(0, W, 0, H, -1, 1);
        c.r.begin(hud);
        M4 I = M4::identity();
        // ---- digital rain, glyphs drawn from the current verse ----
        const float cell = 30;
        if (drops_.empty()) {
            Rng r(5);
            for (float x = 0; x < W; x += cell) drops_.push_back({x, r.range(0, H), r.range(80, 260), (int)r.range(6, 26), r.next()});
        }
        const std::string& src = c.bible.verse(c.ref);
        for (auto& d : drops_) {
            d.y -= d.speed * c.dt;
            if (d.y + d.len * cell < 0) { d.y = H + Rng(d.seed).range(0, 200); d.seed = d.seed * 1664525u + 1013904223u; }
            for (int k = 0; k < d.len; k++) {
                float y = d.y + k * cell;
                if (y < 0 || y > H) continue;
                uint32_t h = (d.seed + k * 2654435761u + (uint32_t)(c.t * 6) * (k == 0 ? 1u : 0u));
                char ch = src.empty() ? (char)('0' + h % 10) : src[h % src.size()];
                if (ch == ' ') ch = (char)('0' + h % 2);
                float a = k == 0 ? 1.0f : 0.75f * (1 - (float)k / d.len);
                V4 col = k == 0 ? scaleRGB(c.pal.ink, 1.6f) : scaleRGB(c.pal.glowA, 0.9f);
                c.r.text(c.fonts.term, std::string(1, ch), I, d.x, y, cell * 1.15f, withAlpha(col, a * 0.8f));
            }
        }
        c.r.flush();

        // ---- terminal window ----
        float tx = W * 0.06f, ty = H * 0.07f, tw = W * 0.88f, th = H * 0.86f;
        c.r.setAdditive(false);
        c.r.quad({tx, ty, 0}, {tx + tw, ty, 0}, {tx + tw, ty + th, 0}, {tx, ty + th, 0}, withAlpha(scaleRGB(c.pal.deep, 0.5f), 0.86f));
        c.r.quad({tx, ty + th - 40, 0}, {tx + tw, ty + th - 40, 0}, {tx + tw, ty + th, 0}, {tx, ty + th, 0}, withAlpha(c.pal.glowA, 0.18f));
        c.r.flush();
        c.r.setAdditive(true);
        V4 edge = scaleRGB(c.pal.glowA, 1.5f);
        c.r.line({tx, ty, 0}, {tx + tw, ty, 0}, edge); c.r.line({tx + tw, ty, 0}, {tx + tw, ty + th, 0}, edge);
        c.r.line({tx + tw, ty + th, 0}, {tx, ty + th, 0}, edge); c.r.line({tx, ty + th, 0}, {tx, ty, 0}, edge);
        std::string title = sh.user + "@" + sh.host + " : bibsh : " + sh.cwd() + " : " + c.bible.tr().code + " : offline (no network)";
        c.r.text(c.fonts.term, title, I, tx + 16, ty + th - 30, 34, scaleRGB(c.pal.glowA, 1.6f));

        float size = 42 * c.textScale, lh = size * 1.0f, x0 = tx + 24, maxW = tw - 48;
        V4 cmdC = scaleRGB(c.pal.glowA, 1.8f), outC = withAlpha(scaleRGB(c.pal.ink, 1.05f), 0.85f), textC = scaleRGB(c.pal.ink, 1.6f),
           errC = scaleRGB(c.pal.red, 1.5f), dimC = withAlpha(scaleRGB(c.pal.glowA, 1.1f), 0.65f);
        struct L { std::string s; V4 c; };
        std::vector<L> out;
        size_t budget = (size_t)typed_;
        for (size_t i = 0; i < sh.lines.size(); i++) {
            const TLine& t = sh.lines[i];
            std::string s = t.s;
            if (sh.typeFrom != (size_t)-1 && i >= sh.typeFrom) {
                if (budget == 0) break;
                if (s.size() + 1 > budget) s = s.substr(0, budget);
                budget = budget > s.size() + 1 ? budget - (s.size() + 1) : 0;
            }
            V4 col = t.kind == TLine::Cmd ? cmdC : t.kind == TLine::Text ? textC : t.kind == TLine::Err ? errC : t.kind == TLine::Dim ? dimC : outC;
            for (auto& w : c.fonts.term.wrap(s, size, maxW)) out.push_back({w, col});
        }
        bool typing = sh.typeFrom != (size_t)-1 && typed_ < typeChars;
        bool blink = std::fmod(c.t, 1.0f) < 0.55f;
        if (!typing) {
            std::string prompt = sh.prompt() + sh.input + (blink ? "_" : " ");
            for (auto& w : c.fonts.term.wrap(prompt, size, maxW)) out.push_back({w, cmdC});
        }
        int maxLines = (int)((th - 90) / lh);
        int start = std::max(0, (int)out.size() - maxLines);
        float y = ty + th - 88;
        for (int i = start; i < (int)out.size(); i++) {
            c.r.text(c.fonts.term, out[i].s, I, x0, y, size, out[i].c);
            y -= lh;
        }
        c.r.flush();
    }

private:
    std::vector<Drop> drops_;
    float typed_ = 0;
    int serial_ = -1;
};
}  // namespace

std::unique_ptr<Scene> makeHacker() { return std::make_unique<Hacker>(); }
