// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// HACKER - a phosphor terminal over digital rain made from the text itself. The prompt is live:
// type a reference ("john 3:16"), random, today, search <words>, tr <code>, help.
#include "scene.h"

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
        if (serial_ != c.refSerial) { serial_ = c.refSerial; typed_ = 0; text_ = build(c); }
        // typewriter: ~90 chars/s with a click every few characters
        float before = typed_;
        typed_ = std::min((float)text_.size(), typed_ + c.dt * 90 * (c.chapter ? 3.0f : 1.0f));
        if ((int)(typed_ / 4) != (int)(before / 4) && typed_ < text_.size()) c.chip.play(Sfx::Key);

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
        c.r.quad({tx, ty, 0}, {tx + tw, ty, 0}, {tx + tw, ty + th, 0}, {tx, ty + th, 0}, withAlpha(scaleRGB(c.pal.deep, 0.5f), 0.82f));
        c.r.quad({tx, ty + th - 34, 0}, {tx + tw, ty + th - 34, 0}, {tx + tw, ty + th, 0}, {tx, ty + th, 0}, withAlpha(c.pal.glowA, 0.18f));
        c.r.flush();
        c.r.setAdditive(true);
        V4 edge = scaleRGB(c.pal.glowA, 1.5f);
        c.r.line({tx, ty, 0}, {tx + tw, ty, 0}, edge); c.r.line({tx + tw, ty, 0}, {tx + tw, ty + th, 0}, edge);
        c.r.line({tx + tw, ty + th, 0}, {tx, ty + th, 0}, edge); c.r.line({tx, ty + th, 0}, {tx, ty, 0}, edge);
        std::string title = c.hostname + " :: omabiblia :: " + c.bible.tr().code + " :: offline (no network)";
        c.r.text(c.fonts.term, title, I, tx + 16, ty + th - 30, 34, scaleRGB(c.pal.glowA, 1.6f));

        float size = 46 * c.textScale, lh = size * 1.0f, x0 = tx + 24, maxW = tw - 48;
        V4 ink = scaleRGB(c.pal.glowA, 1.8f), soft = withAlpha(scaleRGB(c.pal.glowA, 1.1f), 0.7f), hi = scaleRGB(c.pal.ink, 1.6f);
        // assemble the visible lines bottom-up so the newest output stays on screen
        struct L { std::string s; V4 c; };
        std::vector<L> out;
        if (c.termLines) for (auto& s : *c.termLines) for (auto& w : c.fonts.term.wrap(s, size, maxW)) out.push_back({w, soft});
        out.push_back({"$ cat " + lowerRef(c), ink});
        std::string shown = text_.substr(0, (size_t)typed_);
        for (auto& w : c.fonts.term.wrap(shown, size, maxW)) out.push_back({w, hi});
        bool blink = std::fmod(c.t, 1.0f) < 0.55f;
        std::string prompt = "$ " + (c.termInput ? *c.termInput : std::string()) + (blink ? "_" : " ");
        out.push_back({prompt, ink});
        int maxLines = (int)((th - 80) / lh);
        int start = std::max(0, (int)out.size() - maxLines);
        float y = ty + th - 84;
        for (int i = start; i < (int)out.size(); i++) {
            c.r.text(c.fonts.term, out[i].s, I, x0, y, size, out[i].c);
            y -= lh;
        }
        c.r.flush();
    }

private:
    std::string lowerRef(Ctx& c) {
        std::string s = c.bible.books()[c.ref.book].usfm + "/" + std::to_string(c.ref.chapter);
        if (!c.chapter) s += ":" + std::to_string(c.ref.verse);
        for (auto& ch : s) ch = (char)std::tolower((unsigned char)ch);
        return s;
    }
    std::string build(Ctx& c) {
        std::string s;
        int n = c.bible.verseCount(c.ref.book, c.ref.chapter);
        int from = c.chapter ? 1 : c.ref.verse, to = c.chapter ? n : c.ref.verse;
        for (int v = from; v <= to; v++) {
            const std::string& t = c.bible.verse({c.ref.book, c.ref.chapter, v});
            if (t.empty()) continue;
            s += (s.empty() ? "" : "\n") + std::to_string(v) + " " + t;
        }
        return s;
    }
    std::vector<Drop> drops_;
    std::string text_;
    float typed_ = 0;
    int serial_ = -1;
};
}  // namespace

std::unique_ptr<Scene> makeHacker() { return std::make_unique<Hacker>(); }
