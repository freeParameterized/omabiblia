// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// SPACE COMMAND - the Bible as a star system: the Old Testament on the inner ring, the New on the
// outer ring, planet size by chapter count. A tactical HUD locks onto the book being read.
#include <cstdio>
#include "scene.h"

namespace {
class Command : public Scene {
public:
    const char* name() const override { return "SPACE COMMAND"; }
    const char* blurb() const override { return "tactical display: 66 worlds in two orbits"; }
    int song() const override { return 2; }

    V3 planet(int i, float t) const {
        bool nt = i >= 39;
        int k = nt ? i - 39 : i, n = nt ? 27 : 39;
        float r = nt ? 30.f : 18.f;
        float a = 2 * PI * k / n + t * (nt ? 0.010f : 0.016f);
        return {r * std::cos(a), (nt ? 1.5f : -1.0f) * std::sin(a * 3) * 0.6f, r * std::sin(a)};
    }

    void draw(Ctx& c) override {
        clearTarget(c.pal.deep);
        int b = c.ref.book;
        V3 target = planet(b, c.t);
        if (first_) { look_ = target; first_ = false; }
        look_ = look_ + (target - look_) * (1 - std::exp(-c.dt * 1.2f));
        float yaw = c.t * 0.03f;
        V3 eye = look_ * 0.35f + V3(std::sin(yaw) * 46, 26, std::cos(yaw) * 46);
        M4 proj = perspective(45 * PI / 180, (float)c.w / c.h, 0.1f, 500);
        // shift the system left in the frame so the target panel on the right stays clear
        M4 vp = translate({-0.55f, 0.05f, 0}) * proj * lookAt(eye, look_ * 0.6f, {0, 1, 0});
        c.r.begin(vp);

        // background stars
        Rng r(99);
        for (int i = 0; i < 1400; i++) {
            V3 d = normalize(V3(r.range(-1, 1), r.range(-1, 1), r.range(-1, 1))) * 220;
            c.r.point(d, scaleRGB(c.pal.ink, 0.6f + 0.6f * r.f()), r.range(20, 60));
        }
        // the sun (the Word) and its corona
        c.r.point({0, 0, 0}, scaleRGB(c.pal.warm, 4), 9000);
        c.r.point({0, 0, 0}, withAlpha(scaleRGB(c.pal.warm, 1.5f), 0.4f), 26000);
        // orbit rings
        c.r.circle({0, 0, 0}, {1, 0, 0}, {0, 0, 1}, 18, withAlpha(scaleRGB(c.pal.glowA, 0.8f), 0.5f), 160);
        c.r.circle({0, 0, 0}, {1, 0, 0}, {0, 0, 1}, 30, withAlpha(scaleRGB(c.pal.glowC, 0.8f), 0.5f), 200);
        for (int i = 0; i < 12; i++) {   // range marks
            float a = i * PI / 6;
            c.r.line({std::cos(a) * 15, 0, std::sin(a) * 15}, {std::cos(a) * 34, 0, std::sin(a) * 34}, withAlpha(scaleRGB(c.pal.muted, 1), 0.25f));
        }
        // planets
        for (int i = 0; i < 66; i++) {
            V3 p = planet(i, c.t);
            float sz = 120 + 22 * std::sqrt((float)c.bible.books()[i].chapters) * 10;
            V4 col = i < 39 ? c.pal.glowA : c.pal.glowC;
            if (i == b) col = c.pal.glowB;
            c.r.point(p, scaleRGB(col, i == b ? 2.6f : 1.2f), sz);
            c.r.line(p, {p.x, 0, p.z}, withAlpha(col, 0.25f));     // drop line to the plane
        }
        // target lock: rotating brackets + line from the sun
        V3 tp = planet(b, c.t);
        c.r.line({0, 0, 0}, tp, withAlpha(scaleRGB(c.pal.glowB, 1.5f), 0.4f));
        float rot = c.t * 1.2f, rr = 2.6f + 0.3f * std::sin(c.t * 3);
        for (int k = 0; k < 4; k++) {
            float a = rot + k * PI / 2;
            V3 o = tp + V3(std::cos(a) * rr, 0, std::sin(a) * rr);
            V3 o2 = tp + V3(std::cos(a + 0.5f) * rr, 0, std::sin(a + 0.5f) * rr);
            c.r.line(o, o2, scaleRGB(c.pal.glowB, 2.2f));
        }
        c.r.flush();

        // ---------------- HUD (screen space, pixels, y up) ----------------
        M4 hud = ortho(0, (float)c.w, 0, (float)c.h, -1, 1);
        c.r.begin(hud);
        M4 I = M4::identity();
        V4 hc = scaleRGB(c.pal.glowA, 1.4f), dim = withAlpha(hc, 0.35f);
        float W = (float)c.w, H = (float)c.h, m = 34;
        // frame corners
        auto corner = [&](float x, float y, float sx, float sy) {
            c.r.line({x, y, 0}, {x + sx * 80, y, 0}, hc);
            c.r.line({x, y, 0}, {x, y + sy * 80, 0}, hc);
        };
        corner(m, m, 1, 1); corner(W - m, m, -1, 1); corner(m, H - m, 1, -1); corner(W - m, H - m, -1, -1);
        // top bar
        c.r.text(c.fonts.display, "OMABIBLIA  //  SPACE COMMAND", I, m + 20, H - m - 46, 32, hc);
        char buf[160];
        std::snprintf(buf, sizeof buf, "TRANSLATION %s   STARDATE %s", c.bible.tr().code.c_str(), todayIso().c_str());
        c.r.text(c.fonts.display, buf, I, W - m - 20 - c.fonts.display.measure(buf, 22), H - m - 42, 22, withAlpha(hc, 0.6f));
        // radar (bottom-left)
        V3 rc{m + 170, m + 170, 0};
        float R = 140;
        c.r.circle(rc, {1, 0, 0}, {0, 1, 0}, R, dim, 72);
        c.r.circle(rc, {1, 0, 0}, {0, 1, 0}, R * 0.6f, withAlpha(hc, 0.2f), 72);
        float sweep = c.t * 1.6f;
        for (int k = 0; k < 18; k++) {
            float a = sweep - k * 0.03f;
            c.r.line(rc, rc + V3(std::cos(a) * R, std::sin(a) * R, 0), withAlpha(hc, 0.5f * (1 - k / 18.f)));
        }
        for (int i = 0; i < 66; i++) {
            V3 p = planet(i, c.t);
            V3 q = rc + V3(p.x / 34 * R, p.z / 34 * R, 0);
            float ang = std::atan2(p.z, p.x), dA = std::fmod(sweep - ang + 20 * PI, 2 * PI);
            float glow = clampf(1 - dA / 2.5f, 0.15f, 1);
            c.r.point(q, scaleRGB(i == b ? c.pal.glowB : hc, (i == b ? 2.5f : 1.2f) * glow), i == b ? 60 : 26);
        }
        // target panel (right)
        float px = W * 0.5f, pw = W - m - 30 - px, py = H * 0.1f, ph = H * 0.76f;
        c.r.setAdditive(false);
        c.r.quad({px, py, 0}, {px + pw, py, 0}, {px + pw, py + ph, 0}, {px, py + ph, 0}, withAlpha(c.pal.deep, 0.72f));
        c.r.flush();
        c.r.setAdditive(true);
        c.r.line({px, py, 0}, {px + pw, py, 0}, hc); c.r.line({px, py + ph, 0}, {px + pw, py + ph, 0}, hc);
        c.r.line({px, py, 0}, {px, py + ph, 0}, dim); c.r.line({px + pw, py, 0}, {px + pw, py + ph, 0}, dim);
        float y = py + ph - 52;
        c.r.text(c.fonts.display, "TARGET LOCK", I, px + 26, y, 22, withAlpha(scaleRGB(c.pal.glowB, 2), 0.6f + 0.4f * std::sin(c.t * 5)));
        y -= 62;
        c.r.text(c.fonts.display, upper(c.bible.books()[b].name), I, px + 26, y, 52, scaleRGB(c.pal.glowB, 2.2f));
        y -= 44;
        std::snprintf(buf, sizeof buf, "ORBIT %s  /  CH %d OF %d  /  VS %d", b < 39 ? "INNER" : "OUTER", c.ref.chapter,
                      c.bible.chapterCount(b), c.ref.verse);
        c.r.text(c.fonts.display, buf, I, px + 26, y, 20, withAlpha(hc, 0.7f));
        y -= 26;
        c.r.line({px + 22, y, 0}, {px + pw - 22, y, 0}, dim);
        y -= 48;
        float size = 34 * c.textScale, lh = size * 1.3f;
        int n = c.bible.verseCount(b, c.ref.chapter);
        int from = c.chapter ? std::max(1, c.ref.verse - 1) : c.ref.verse, to = c.chapter ? n : c.ref.verse;
        for (int v = from; v <= to && y > py + 20; v++) {
            const std::string& txt = c.bible.verse({b, c.ref.chapter, v});
            if (txt.empty()) continue;
            bool cur = v == c.ref.verse;
            for (auto& s : c.fonts.mono.wrap(std::to_string(v) + "  " + txt, size, pw - 44)) {
                if (y < py + 20) break;
                c.r.text(c.fonts.mono, s, I, px + 22, y, size, cur ? scaleRGB(c.pal.ink, 1.3f) : withAlpha(c.pal.ink, 0.55f));
                y -= lh;
            }
            y -= lh * 0.4f;
        }
        c.r.flush();
    }

private:
    V3 look_;
    bool first_ = true;
};
}  // namespace

std::unique_ptr<Scene> makeCommand() { return std::make_unique<Command>(); }
