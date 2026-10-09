// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// LINK - a cyberspace grid running to the horizon. The 66 books stand as wireframe data towers
// (height = chapters). The camera glides to the book you are reading; the verses hang above it
// as a holographic slab.
#include "scene.h"

namespace {
class Link : public Scene {
public:
    const char* name() const override { return "LINK"; }
    const char* blurb() const override { return "jacked in: books as data towers on the grid"; }
    int song() const override { return 1; }

    V3 towerPos(int i) const {
        // OT: 39 towers in a 7-wide block on the left, NT: 27 in a 6-wide block on the right
        bool nt = i >= 39;
        int k = nt ? i - 39 : i, cols = nt ? 6 : 7;
        float x = (k % cols) * 3.2f + (nt ? 6.0f : -26.0f), z = -(k / cols) * 3.2f;
        return {x, 0, z};
    }
    float towerH(const Ctx& c, int i) const { return 0.6f + 0.22f * c.bible.books()[i].chapters; }

    void draw(Ctx& c) override {
        clearTarget(c.pal.deep);
        int b = c.ref.book;
        V3 tp = towerPos(b);
        float th = std::min(towerH(c, b), 18.f);
        // smooth camera glide towards the current tower, slow orbit around it
        V3 want = tp + V3(0, th * 0.55f + 2.0f, 0);
        if (first_) { focus_ = want; first_ = false; }
        float k = 1 - std::exp(-c.dt * 1.6f);
        focus_ = focus_ + (want - focus_) * k;
        float orbit = c.t * 0.07f;
        V3 eye = focus_ + V3(std::sin(orbit) * 14, 4.5f, std::cos(orbit) * 14);
        M4 proj = perspective(55 * PI / 180, (float)c.w / c.h, 0.1f, 300);
        M4 view = lookAt(eye, focus_, {0, 1, 0});
        c.r.begin(proj * view);

        // grid floor, scrolling, fading with distance
        V4 g = c.pal.glowA;
        float scroll = std::fmod(c.t * 1.5f, 4.f);
        for (int i = -40; i <= 40; i++) {
            float x = i * 4.f + std::floor(focus_.x / 4) * 4;
            float a = 0.5f * (1 - std::abs((float)i) / 40.f);
            c.r.line({x, 0, focus_.z + 160}, {x, 0, focus_.z - 160}, withAlpha(scaleRGB(g, 0.9f), 0), withAlpha(scaleRGB(g, 0.9f), a));
        }
        for (int i = -40; i <= 40; i++) {
            float z = i * 4.f + std::floor(focus_.z / 4) * 4 + scroll;
            float a = 0.55f * (1 - std::abs((float)i) / 40.f);
            c.r.line({focus_.x - 160, 0, z}, {focus_.x + 160, 0, z}, withAlpha(scaleRGB(g, 0.9f), a * 0.1f), withAlpha(scaleRGB(g, 0.9f), a));
        }
        // towers
        for (int i = 0; i < 66; i++) {
            V3 p = towerPos(i);
            float h = std::min(towerH(c, i), 18.f), w = 0.9f;
            bool cur = i == b;
            V4 col = cur ? scaleRGB(c.pal.glowB, 2.2f) : (i < 39 ? scaleRGB(g, 0.9f) : scaleRGB(c.pal.glowC, 0.9f));
            float pulse = cur ? 0.6f + 0.4f * std::sin(c.t * 4) : 1;
            box(c, p, w, h, withAlpha(col, 0.9f * pulse));
            if (cur) {
                // data pulses rising up the current tower
                for (int j = 0; j < 6; j++) {
                    float y = std::fmod(c.t * 3 + j * h / 6, h);
                    V4 pc = withAlpha(scaleRGB(c.pal.glowB, 2.5f), 1 - y / h);
                    c.r.line(p + V3(-w, y, -w), p + V3(w, y, -w), pc);
                    c.r.line(p + V3(w, y, -w), p + V3(w, y, w), pc);
                    c.r.line(p + V3(w, y, w), p + V3(-w, y, w), pc);
                    c.r.line(p + V3(-w, y, w), p + V3(-w, y, -w), pc);
                }
                c.r.point(p + V3(0, h + 0.4f, 0), scaleRGB(c.pal.glowB, 3), 900);
            }
            // light traces between neighbouring books (the "link")
            if (i + 1 < 66 && (i + 1) != 39) {
                V3 q = towerPos(i + 1);
                float t = std::fmod(c.t * 0.5f + i * 0.13f, 1.f);
                V3 m = p + (q - p) * t;
                c.r.point(m + V3(0, 0.05f, 0), scaleRGB(g, 1.6f), 160);
            }
        }
        c.r.flush();

        // labels, billboarded towards the camera
        V3 fwd = normalize(focus_ - eye), right = normalize(cross(fwd, {0, 1, 0})), up = cross(right, fwd);
        auto billboard = [&](V3 at) {
            M4 m = M4::identity();
            m.m[0] = right.x; m.m[1] = right.y; m.m[2] = right.z;
            m.m[4] = up.x; m.m[5] = up.y; m.m[6] = up.z;
            m.m[8] = -fwd.x; m.m[9] = -fwd.y; m.m[10] = -fwd.z;
            m.m[12] = at.x; m.m[13] = at.y; m.m[14] = at.z;
            return m;
        };
        for (int i = 0; i < 66; i++) {
            V3 p = towerPos(i);
            float h = std::min(towerH(c, i), 18.f);
            float d = length(p - eye);
            if (d > 45 && i != b) continue;
            V4 col = i == b ? scaleRGB(c.pal.glowB, 2.5f) : withAlpha(scaleRGB(c.pal.ink, 0.9f), clampf(1.4f - d / 40, 0, 1));
            c.r.textCentered(c.fonts.mono, upper(c.bible.books()[i].abbr), billboard(p + V3(0, h + 0.9f, 0)), 0, i == b ? 0.7f : 0.45f, col);
        }

        c.r.flush();   // labels first, so the slab and its text sit in front of them
        // the holographic slab above the current tower
        M4 slab = billboard(focus_ + V3(0, 1.6f, 0)) * translate({0, 0, 1.5f});
        float W = 8.4f, size = 0.46f * c.textScale, lh = size * 1.35f;
        std::vector<std::pair<std::string, bool>> lines;
        int n = c.bible.verseCount(b, c.ref.chapter);
        int from = c.chapter ? std::max(1, c.ref.verse - 2) : c.ref.verse, to = c.chapter ? std::min(n, c.ref.verse + 3) : c.ref.verse;
        for (int v = from; v <= to; v++) {
            const std::string& txt = c.bible.verse({b, c.ref.chapter, v});
            if (txt.empty()) continue;
            for (auto& s : c.fonts.mono.wrap("[" + std::to_string(v) + "] " + txt, size, W - 0.6f)) lines.push_back({s, v == c.ref.verse});
        }
        float H = (lines.size() + 2.5f) * lh;
        V4 panel = withAlpha(c.pal.deep, 0.9f);
        V3 a = (slab * V4(-W / 2, -H / 2, 0, 1)).xyz(), bb = (slab * V4(W / 2, -H / 2, 0, 1)).xyz(),
           cc = (slab * V4(W / 2, H / 2, 0, 1)).xyz(), d = (slab * V4(-W / 2, H / 2, 0, 1)).xyz();
        c.r.setAdditive(false);
        c.r.quad(a, bb, cc, d, panel);
        c.r.flush();
        c.r.setAdditive(true);
        V4 edge = scaleRGB(c.pal.glowA, 1.8f);
        c.r.line(a, bb, edge); c.r.line(bb, cc, edge); c.r.line(cc, d, edge); c.r.line(d, a, edge);
        // scanning bar
        float sy = -H / 2 + std::fmod(c.t * 0.8f, 1.f) * H;
        c.r.line((slab * V4(-W / 2, sy, 0.01f, 1)).xyz(), (slab * V4(W / 2, sy, 0.01f, 1)).xyz(), withAlpha(edge, 0.35f));
        float y = H / 2 - lh * 1.2f;
        c.r.text(c.fonts.display, upper(c.bible.refString(c.ref, !c.chapter)) + "  //  " + c.bible.tr().code, slab, -W / 2 + 0.3f, y, size * 0.95f, scaleRGB(c.pal.glowB, 2));
        y -= lh * 1.3f;
        for (auto& L : lines) {
            c.r.text(c.fonts.mono, L.first, slab, -W / 2 + 0.3f, y, size, L.second ? scaleRGB(c.pal.ink, 1.5f) : withAlpha(scaleRGB(c.pal.ink, 0.9f), 0.65f));
            y -= lh;
        }
        c.r.flush();
    }

private:
    void box(Ctx& c, V3 p, float w, float h, V4 col) {
        V3 b[4] = {p + V3(-w, 0, -w), p + V3(w, 0, -w), p + V3(w, 0, w), p + V3(-w, 0, w)};
        V3 t[4] = {b[0] + V3(0, h, 0), b[1] + V3(0, h, 0), b[2] + V3(0, h, 0), b[3] + V3(0, h, 0)};
        for (int i = 0; i < 4; i++) {
            c.r.line(b[i], b[(i + 1) % 4], col);
            c.r.line(t[i], t[(i + 1) % 4], col);
            c.r.line(b[i], t[i], withAlpha(col, col.w * 0.2f), col);
        }
        // translucent faces so towers read as solid light
        c.r.quad(b[0], b[1], t[1], t[0], withAlpha(col, 0.05f));
        c.r.quad(b[1], b[2], t[2], t[1], withAlpha(col, 0.05f));
        c.r.quad(b[2], b[3], t[3], t[2], withAlpha(col, 0.05f));
        c.r.quad(b[3], b[0], t[0], t[3], withAlpha(col, 0.05f));
    }
    V3 focus_;
    bool first_ = true;
};
}  // namespace

std::unique_ptr<Scene> makeLink() { return std::make_unique<Link>(); }
