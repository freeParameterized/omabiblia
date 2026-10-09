// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// CRAWL - the reading recedes into deep space on a tilted plane, over a slowly turning starfield.
#include <cctype>
#include "scene.h"

std::string upper(const std::string& s) {
    std::string o = s;
    for (auto& c : o) c = (char)std::toupper((unsigned char)c);
    return o;
}
void clearTarget(V4 c) {
    glClearColor(c.x, c.y, c.z, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

namespace {
struct Star { V3 p; float s, tw; };

class Crawl : public Scene {
public:
    const char* name() const override { return "CRAWL"; }
    const char* blurb() const override { return "the chapter rolls away into deep space"; }
    int song() const override { return 0; }

    void enter(Ctx& c) override { start_ = c.t; serial_ = -1; }

    void draw(Ctx& c) override {
        if (stars_.empty()) {
            Rng r(7);
            for (int i = 0; i < 2600; i++) {
                V3 d = normalize(V3(r.range(-1, 1), r.range(-1, 1), r.range(-1, 1)));
                stars_.push_back({d * r.range(60, 140), r.range(0.6f, 2.6f), r.range(0, 6.28f)});
            }
        }
        if (serial_ != c.refSerial) { serial_ = c.refSerial; start_ = c.t; layout(c); }
        float t = c.t - start_;

        clearTarget(c.pal.deep);
        M4 proj = perspective(50 * PI / 180, (float)c.w / c.h, 0.1f, 400);
        // gentle drift of the camera so the crawl never feels flat
        float yaw = 0.06f * std::sin(c.t * 0.11f), pitch = 0.03f * std::sin(c.t * 0.07f);
        M4 view = rotX(pitch) * rotY(yaw) * lookAt({0, 0.9f, 7.5f}, {0, 2.4f, -6}, {0, 1, 0});
        c.r.begin(proj * view);

        // starfield (slow rotation) + a faint nebula ring of accent points
        M4 spin = rotY(c.t * 0.006f) * rotX(0.3f);
        for (auto& s : stars_) {
            V3 p = (spin * V4(s.p, 1)).xyz();
            float tw = 0.65f + 0.35f * std::sin(c.t * 2.1f + s.tw);
            c.r.point(p, scaleRGB(c.pal.ink, 1.3f * tw), s.s * 260);
        }
        Rng nr(11);
        for (int i = 0; i < 160; i++) {
            float a = nr.range(0, 6.28f), rad = nr.range(70, 110);
            V3 p = (spin * V4(rad * std::cos(a), nr.range(-6, 6) + 30 * std::sin(a * 2) * 0.1f, rad * std::sin(a), 1)).xyz();
            V4 col = i % 2 ? c.pal.glowB : c.pal.glowC;
            c.r.point(p, withAlpha(scaleRGB(col, 0.45f), 0.12f), nr.range(5000, 14000));
        }
        c.r.flush();

        // the crawl plane: tilted back, text moves up (away) over time
        float speed = 0.8f;
        float scroll = t * speed - 2.5f;
        M4 plane = translate({0, -1.1f, 0}) * rotX(-74 * PI / 180);
        float size = 0.55f * c.textScale, lh = size * 1.3f;
        V4 gold = c.pal.warm;
        float y = scroll;
        // title block
        auto fade = [&](float yy) { return clampf(1 - (yy - 16) / 12, 0, 1) * clampf((yy + 3) / 1.5f, 0, 1); };
        c.r.textCentered(c.fonts.wide, title_, plane, y + 3.2f * lh, size * 2.4f, withAlpha(scaleRGB(gold, 1.6f), fade(y + 3.2f * lh)));
        c.r.textCentered(c.fonts.wide, sub_, plane, y + 1.5f * lh, size * 0.9f, withAlpha(scaleRGB(c.pal.glowA, 1.3f), fade(y + 1.4f * lh)));
        for (auto& L : lines_) {
            y -= lh;
            if (y < -5 || y > 30) continue;
            V4 col = L.hl ? scaleRGB(gold, 2.0f) : scaleRGB(gold, 1.15f);
            c.r.text(c.fonts.body, L.s, plane, -width_ / 2, y, size, withAlpha(col, fade(y)));
        }
        // loop: when everything has gone, start again
        if (y > 30) start_ = c.t;
        c.r.flush();
    }

private:
    struct Line { std::string s; bool hl; };
    void layout(Ctx& c) {
        lines_.clear();
        const Bible& b = c.bible;
        title_ = upper(b.books()[c.ref.book].name);
        sub_ = (c.chapter ? "CHAPTER " + std::to_string(c.ref.chapter) : "CHAPTER " + std::to_string(c.ref.chapter) + "  -  VERSE " + std::to_string(c.ref.verse));
        width_ = 9.0f;
        float size = 0.55f * c.textScale;
        int n = c.bible.verseCount(c.ref.book, c.ref.chapter);
        int from = c.chapter ? 1 : std::max(1, c.ref.verse), to = c.chapter ? n : std::min(n, c.ref.verse);
        for (int v = from; v <= to; v++) {
            std::string txt = b.verse({c.ref.book, c.ref.chapter, v});
            if (txt.empty()) continue;
            bool hl = c.chapter && v == c.ref.verse;
            auto w = c.fonts.body.wrap(std::to_string(v) + "  " + txt, size, width_);
            for (auto& s : w) lines_.push_back({s, hl});
            lines_.push_back({"", false});
        }
    }
    std::vector<Star> stars_;
    std::vector<Line> lines_;
    std::string title_, sub_;
    float width_ = 9, start_ = 0;
    int serial_ = -1;
};
}  // namespace

std::unique_ptr<Scene> makeCrawl() { return std::make_unique<Crawl>(); }
