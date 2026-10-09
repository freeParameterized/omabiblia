// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
#include "theme.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sys/stat.h>

static std::string home() { const char* h = std::getenv("HOME"); return h ? h : "."; }
static std::string stateDir() { return home() + "/.local/state/omarchy/current"; }

static long long mtimeOf(const std::string& p) {
    struct stat st{};
    if (stat(p.c_str(), &st) != 0) return 0;
    return (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
}

V4 hexColor(const std::string& s, V4 fb) {
    size_t i = s.find('#');
    if (i == std::string::npos || s.size() < i + 7) return fb;
    auto hx = [&](size_t k) { return (float)std::strtol(s.substr(i + k, 2).c_str(), nullptr, 16) / 255.f; };
    return {hx(1), hx(3), hx(5), 1};
}

static void rgb2hsv(V4 c, float& h, float& s, float& v) {
    float mx = std::fmax(c.x, std::fmax(c.y, c.z)), mn = std::fmin(c.x, std::fmin(c.y, c.z)), d = mx - mn;
    v = mx; s = mx > 0 ? d / mx : 0; h = 0;
    if (d > 1e-5f) {
        if (mx == c.x) h = std::fmod((c.y - c.z) / d, 6.f);
        else if (mx == c.y) h = (c.z - c.x) / d + 2;
        else h = (c.x - c.y) / d + 4;
        h /= 6; if (h < 0) h += 1;
    }
}
static V4 hsv2rgb(float h, float s, float v, float a) {
    float r = clampf(std::fabs(h * 6 - 3) - 1, 0, 1), g = clampf(2 - std::fabs(h * 6 - 2), 0, 1), b = clampf(2 - std::fabs(h * 6 - 4), 0, 1);
    return {v * mixf(1, r, s), v * mixf(1, g, s), v * mixf(1, b, s), a};
}

V4 neon(V4 c, float boost) {
    float h, s, v;
    rgb2hsv(c, h, s, v);
    // greys stay grey-ish but brightened; colours become saturated and bright
    s = s < 0.08f ? s : clampf(s * 1.15f + 0.15f, 0, 1);
    v = clampf(0.75f + 0.25f * v, 0, 1);
    V4 o = hsv2rgb(h, s, v, c.w);
    return scaleRGB(o, boost);
}

void Palette::derive() {
    float h, s, v;
    rgb2hsv(accent, h, s, v);
    // deep space for the inside of the screen: the theme background if dark, else a very dark accent tint
    deep = dark ? scaleRGB(mix4(background, accent, 0.06f), 1.0f) : hsv2rgb(h, clampf(s * 0.8f, 0, 0.9f), 0.07f, 1);
    if (dark && (background.x + background.y + background.z) > 0.6f) deep = hsv2rgb(h, 0.6f, 0.08f, 1);
    glowA = neon(accent);
    glowB = neon(magenta);
    glowC = neon(cyan);
    warm = neon(yellow);
    ink = dark ? neon(foreground, 1.0f) : V4{0.92f, 0.94f, 0.98f, 1};   // text on the deep screen
    // themes whose palette is all one hue (e.g. green "matrix") still get distinct glows
    float h2, s2, v2;
    rgb2hsv(glowB, h2, s2, v2);
    if (std::fabs(h2 - h) < 0.04f) glowB = hsv2rgb(std::fmod(h + 0.12f, 1.f), 0.9f, 1, 1);
}

static std::map<std::string, std::string> readToml(const std::string& path) {
    std::map<std::string, std::string> kv;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos || line[0] == '#' || line[0] == '[') continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        auto trim = [](std::string& x) {
            size_t a = x.find_first_not_of(" \t\""), b = x.find_last_not_of(" \t\"\r");
            x = a == std::string::npos ? "" : x.substr(a, b - a + 1);
        };
        trim(k); trim(v);
        kv[k] = v;
    }
    return kv;
}

static std::string run(const std::string& cmd) {
    std::string out;
    if (FILE* p = popen(cmd.c_str(), "r")) {
        char buf[512];
        while (fgets(buf, sizeof buf, p)) out += buf;
        pclose(p);
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

bool OmarchyTheme::load() {
    // OMABIBLIA_THEME=<omarchy theme name or dir> previews another theme without switching Omarchy
    std::string dir = stateDir();
    std::string themeDir = dir + "/theme";
    const char* ov = std::getenv("OMABIBLIA_THEME");
    if (ov && *ov) {
        std::string o = ov;
        themeDir = o.find('/') != std::string::npos ? o : "/usr/share/omarchy/themes/" + o;
        struct stat st{};
        if (stat(themeDir.c_str(), &st) != 0) themeDir = home() + "/.config/omarchy/themes/" + o;
    }
    long long st = mtimeOf(dir + "/theme.name") ^ (mtimeOf(themeDir + "/colors.toml") << 1) ^ (mtimeOf(dir + "/background") << 2)
                 ^ (mtimeOf(home() + "/.config/fontconfig/fonts.conf") << 3);
    if (st == stamp && stamp != 0) return false;
    stamp = st;

    Palette p;
    {
        std::ifstream n(dir + "/theme.name");
        std::getline(n, p.name);
        if (ov && *ov) p.name = themeDir.substr(themeDir.find_last_of('/') + 1);
        if (p.name.empty()) p.name = "fallback";
    }
    auto kv = readToml(themeDir + "/colors.toml");
    if (!kv.empty()) {
        auto c = [&](const char* k, V4 fb) { auto it = kv.find(k); return it == kv.end() ? fb : hexColor(it->second, fb); };
        p.dark = kv.count("mode") ? kv["mode"] != "light" : true;
        p.background = c("background", p.background);
        p.foreground = c("foreground", p.foreground);
        p.accent = c("accent", p.accent);
        p.muted = c("muted", p.muted);
        p.selection = c("selection", p.selection);
        p.red = c("red", p.red); p.yellow = c("yellow", p.yellow); p.orange = c("orange", p.orange);
        p.green = c("green", p.green); p.cyan = c("cyan", p.cyan); p.blue = c("blue", p.blue);
        p.magenta = c("magenta", p.magenta);
    }
    p.derive();
    pal = p;

    // Omarchy sets the monospace font via fontconfig; omarchy-font-current names it
    fontName = run("omarchy-font-current 2>/dev/null");
    fontPath = fontName.empty() ? "" : run("fc-match -f '%{file}' '" + fontName + "' 2>/dev/null");
    wallpaperPath = run("readlink -f '" + dir + "/background' 2>/dev/null");
    if (ov && *ov) wallpaperPath = run("ls -1 '" + themeDir + "/backgrounds/' 2>/dev/null | head -1");
    if (ov && *ov && !wallpaperPath.empty()) wallpaperPath = themeDir + "/backgrounds/" + wallpaperPath;
    std::fprintf(stderr, "omabiblia: Omarchy theme '%s' (%s), font '%s', wallpaper '%s'\n", pal.name.c_str(),
                 pal.dark ? "dark" : "light", fontName.c_str(), wallpaperPath.c_str());
    return true;
}

bool OmarchyTheme::poll() { return load(); }
