// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// Omabiblia - an offline, Omarchy-themed 3D Bible reader.
//
// Privacy: this program contains no networking code. Texts are read from local files; settings are
// written only to ~/.config/omabiblia/omabiblia.ini.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <unistd.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "audio.h"
#include "bible.h"
#include "gl.h"
#include "render.h"
#include "scene.h"
#include "screen.h"
#include "theme.h"
#include "wl_input.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifndef OMA_SOURCE_DIR
#define OMA_SOURCE_DIR "."
#endif

namespace {

std::string exeDir() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return ".";
    buf[n] = 0;
    std::string s(buf);
    return s.substr(0, s.find_last_of('/'));
}
bool exists(const std::string& p) { struct stat st{}; return stat(p.c_str(), &st) == 0; }

std::string findRoot() {
    const std::string e = exeDir();
    for (const std::string& c : {e + "/../share/omabiblia", e, e + "/..", std::string(OMA_SOURCE_DIR)})
        if (exists(c + "/data/books.tsv") && exists(c + "/assets/fonts")) return c;
    return OMA_SOURCE_DIR;
}

std::string configPath() {
    const char* x = std::getenv("XDG_CONFIG_HOME");
    std::string base = x && *x ? x : std::string(std::getenv("HOME") ? std::getenv("HOME") : ".") + "/.config";
    return base + "/omabiblia/omabiblia.ini";
}

ImVec4 iv(V4 c, float a = -1) { return ImVec4(c.x, c.y, c.z, a < 0 ? c.w : a); }

struct App {
    GLFWwindow* win = nullptr;
    std::string root;
    OmarchyTheme theme;
    Bible bible;
    Chip chip;
    Renderer r;
    Fonts fonts;
    Screen screen;
    ScreenParams sp;
    RenderTarget sceneRT, lowRT;
    std::vector<std::unique_ptr<Scene>> scenes;
    int scene = 0;
    std::unique_ptr<Ctx> ctx;
    std::vector<std::string> termLines;
    std::string termInput;
    unsigned randomCounter = 0;

    float hudA = 1;
    bool showHud = true, showBooks = false, showSearch = false, showSettings = false, showHelp = false;
    int pickBook = -1, pickChapter = -1;
    char searchBuf[128] = "";
    std::vector<Hit> hits;
    double lastMouseMove = 0;
    double lastX = 0, lastY = 0;
    bool dragging = false;
    float textScale = 1.0f;
    ImFont* uiFont = nullptr;
    ImFont* uiTitle = nullptr;
    double themeCheck = 0;
    bool popout = false;
    bool relaunch = false;          // P: restart in the other mode (full view <-> pop-out)
    InputRegion region;

    // ------------------------------------------------------------------ navigation
    void setRef(Ref r, bool sound = true) {
        ctx->ref = r;
        ctx->refSerial++;
        if (sound) chip.play(Sfx::Select);
    }
    void today() { randomCounter = 0; setRef(bible.verseOfDay(todayIso())); }
    void random() {
        randomCounter++;
        uint64_t h = cyrb53(todayIso() + "KJV" + std::to_string(randomCounter) + std::to_string((long)(glfwGetTime() * 1000)));
        setRef(bible.randomVerse((uint32_t)h));
    }
    void setScene(int i) {
        i = (i % (int)scenes.size() + (int)scenes.size()) % (int)scenes.size();
        if (i == scene) return;
        scene = i;
        scenes[scene]->enter(*ctx);
        ctx->refSerial++;
        chip.setSong(scenes[scene]->song());
        chip.play(Sfx::Whoosh);
    }
    void setTranslation(int i) {
        if (bible.select(i)) {
            Ref r = ctx->ref;
            r.chapter = std::min(r.chapter, std::max(1, bible.chapterCount(r.book)));
            r.verse = std::min(r.verse, std::max(1, bible.verseCount(r.book, r.chapter)));
            setRef(r);
        } else chip.play(Sfx::Error);
    }

    // ------------------------------------------------------------------ hacker prompt
    void say(const std::string& s) { termLines.push_back(s); if (termLines.size() > 200) termLines.erase(termLines.begin()); }
    void exec(std::string cmd) {
        while (!cmd.empty() && cmd.back() == ' ') cmd.pop_back();
        while (!cmd.empty() && cmd.front() == ' ') cmd.erase(cmd.begin());
        if (cmd.empty()) return;
        say("$ " + cmd);
        std::string head = cmd.substr(0, cmd.find(' ')), arg = cmd.find(' ') == std::string::npos ? "" : cmd.substr(cmd.find(' ') + 1);
        for (auto& c : head) c = (char)std::tolower((unsigned char)c);
        if (head == "help" || head == "?") {
            say("  <book> <ch>[:<v>]  read (e.g. john 3:16, ps 23, 1 cor 13)");
            say("  random | today | chapter | verse | next | prev");
            say("  search <words>     find in the current translation");
            say("  tr <code>          translation: " + trList());
            say("  scene <1-5>        crawl link command hacker retro");
            say("  clear              clear the screen");
        } else if (head == "clear" || head == "cls") termLines.clear();
        else if (head == "random") random();
        else if (head == "today") today();
        else if (head == "chapter") { ctx->chapter = true; ctx->refSerial++; }
        else if (head == "verse") { ctx->chapter = false; ctx->refSerial++; }
        else if (head == "next") setRef(bible.step(ctx->ref, 1));
        else if (head == "prev") setRef(bible.step(ctx->ref, -1));
        else if (head == "tr") {
            std::string a = arg;
            for (auto& c : a) c = (char)std::toupper((unsigned char)c);
            int found = -1;
            for (int i = 0; i < (int)bible.translations().size(); i++) if (bible.translations()[i].code == a) found = i;
            if (found < 0) say("  unknown translation. have: " + trList()); else { setTranslation(found); say("  -> " + bible.tr().name); }
        } else if (head == "scene") {
            int n = std::atoi(arg.c_str());
            if (n >= 1 && n <= 5) setScene(n - 1); else say("  scene 1-5");
        } else if (head == "search" || head == "grep") {
            auto hs = bible.search(arg, 9);
            if (hs.empty()) say("  no match");
            for (auto& h : hs) say("  " + bible.refString(h.ref) + "  " + h.text.substr(0, 70) + (h.text.size() > 70 ? "..." : ""));
            if (!hs.empty()) setRef(hs[0].ref);
        } else {
            Ref r;
            if (bible.parseRef(cmd, r)) { setRef(r); ctx->chapter = cmd.find(':') == std::string::npos && std::count(cmd.begin(), cmd.end(), ' ') >= 1 ? ctx->chapter : false; }
            else { say("  command not found: " + head + "  (try help)"); chip.play(Sfx::Error); }
        }
    }
    std::string trList() {
        std::string s;
        for (auto& t : bible.translations()) s += (s.empty() ? "" : " ") + t.code;
        return s;
    }

    // ------------------------------------------------------------------ prefs (local file only)
    void loadPrefs() {
        std::ifstream f(configPath());
        std::string line;
        std::map<std::string, std::string> kv;
        while (std::getline(f, line)) {
            auto eq = line.find('=');
            if (eq != std::string::npos) kv[line.substr(0, eq)] = line.substr(eq + 1);
        }
        auto num = [&](const char* k, float d) { return kv.count(k) ? (float)std::atof(kv[k].c_str()) : d; };
        scene = std::clamp((int)num("scene", 0), 0, 4);
        if (kv.count("translation"))
            for (int i = 0; i < (int)bible.translations().size(); i++) if (bible.translations()[i].code == kv["translation"]) bible.select(i);
        ctx->ref = Ref{std::clamp((int)num("book", 42), 0, 65), std::max(1, (int)num("chapter", 3)), std::max(1, (int)num("verse", 16))};
        ctx->chapter = num("chapterMode", 0) > 0;
        chip.music = num("music", 1) > 0;
        chip.sfx = num("sfx", 1) > 0;
        chip.volume = num("volume", 0.55f);
        sp.crt = num("crt", 1); sp.bloom = num("bloom", 1); sp.curvature = num("curvature", 1); sp.flat = num("flat", 0) > 0; sp.cinematic = num("cinematic", 0) > 0;
        textScale = std::clamp(num("textScale", 1), 0.6f, 2.0f);
        if (!kv.count("book")) today();
    }
    void savePrefs() {
        std::string p = configPath();
        std::string dir = p.substr(0, p.find_last_of('/'));
        std::string cmd = "mkdir -p '" + dir + "'";
        if (std::system(cmd.c_str()) != 0) return;
        std::ofstream f(p);
        f << "scene=" << scene << "\ntranslation=" << bible.tr().code << "\nbook=" << ctx->ref.book << "\nchapter=" << ctx->ref.chapter
          << "\nverse=" << ctx->ref.verse << "\nchapterMode=" << (ctx->chapter ? 1 : 0) << "\nmusic=" << (chip.music ? 1 : 0)
          << "\nsfx=" << (chip.sfx ? 1 : 0) << "\nvolume=" << chip.volume.load() << "\ncrt=" << sp.crt << "\nbloom=" << sp.bloom
          << "\ncurvature=" << sp.curvature << "\nflat=" << (sp.flat ? 1 : 0) << "\ncinematic=" << (sp.cinematic ? 1 : 0) << "\ntextScale=" << textScale << "\n";
    }

    // ------------------------------------------------------------------ look
    void applyStyle() {
        const Palette& p = theme.pal;
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 2; s.FrameRounding = 2; s.GrabRounding = 2; s.WindowBorderSize = 1; s.FrameBorderSize = 1;
        s.WindowPadding = ImVec2(14, 12); s.ItemSpacing = ImVec2(8, 7);
        ImVec4* c = s.Colors;
        V4 deep = p.deep, ink = p.ink, acc = neon(p.accent), mag = p.glowB;
        c[ImGuiCol_WindowBg] = iv(deep, popout ? 0.96f : 0.86f);
        c[ImGuiCol_PopupBg] = iv(deep, 0.95f);
        c[ImGuiCol_ChildBg] = iv(deep, 0.0f);
        c[ImGuiCol_Border] = iv(acc, 0.55f);
        c[ImGuiCol_Text] = iv(ink);
        c[ImGuiCol_TextDisabled] = iv(ink, 0.45f);
        c[ImGuiCol_TitleBg] = iv(deep, 0.95f);
        c[ImGuiCol_TitleBgActive] = iv(mix4(deep, acc, 0.25f), 0.95f);
        c[ImGuiCol_FrameBg] = iv(mix4(deep, acc, 0.10f), 0.9f);
        c[ImGuiCol_FrameBgHovered] = iv(mix4(deep, acc, 0.25f), 0.9f);
        c[ImGuiCol_FrameBgActive] = iv(mix4(deep, acc, 0.35f), 0.9f);
        c[ImGuiCol_Button] = iv(mix4(deep, acc, 0.18f), 0.9f);
        c[ImGuiCol_ButtonHovered] = iv(mix4(deep, acc, 0.45f), 1);
        c[ImGuiCol_ButtonActive] = iv(mix4(deep, mag, 0.6f), 1);
        c[ImGuiCol_Header] = iv(mix4(deep, acc, 0.25f), 0.9f);
        c[ImGuiCol_HeaderHovered] = iv(mix4(deep, acc, 0.45f), 1);
        c[ImGuiCol_HeaderActive] = iv(mix4(deep, mag, 0.55f), 1);
        c[ImGuiCol_CheckMark] = iv(acc);
        c[ImGuiCol_SliderGrab] = iv(acc);
        c[ImGuiCol_SliderGrabActive] = iv(mag);
        c[ImGuiCol_Separator] = iv(acc, 0.4f);
        c[ImGuiCol_ScrollbarBg] = iv(deep, 0.3f);
        c[ImGuiCol_ScrollbarGrab] = iv(acc, 0.5f);
        c[ImGuiCol_Tab] = iv(mix4(deep, acc, 0.15f), 0.9f);
        c[ImGuiCol_TabHovered] = iv(mix4(deep, acc, 0.45f), 1);
        c[ImGuiCol_TabSelected] = iv(mix4(deep, acc, 0.35f), 1);
    }

    void loadFonts() {
        const std::string fd = root + "/assets/fonts/";
        fonts.body.bake(fd + "EBGaramond.ttf", 64);
        std::string mono = theme.fontPath.empty() || !exists(theme.fontPath) ? fd + "ShareTechMono-Regular.ttf" : theme.fontPath;
        if (!fonts.mono.bake(mono, 56)) fonts.mono.bake(fd + "ShareTechMono-Regular.ttf", 56);
        fonts.display.bake(fd + "Orbitron.ttf", 56);
        fonts.wide.bake(fd + "Audiowide-Regular.ttf", 72);
        fonts.pixel.bake(fd + "PressStart2P-Regular.ttf", 8, true);
        fonts.term.bake(fd + "VT323-Regular.ttf", 48);
    }

    // ------------------------------------------------------------------ UI
    void ui() {
        ImGuiIO& io = ImGui::GetIO();
        const Palette& p = theme.pal;
        double now = glfwGetTime();
        bool idle = now - lastMouseMove > 4.0 && !showBooks && !showSearch && !showSettings && !showHelp;
        float hudAlpha = showHud ? (idle ? 0.0f : 1.0f) : 0.0f;
        hudA += (hudAlpha - hudA) * std::min(1.0f, io.DeltaTime * 6);

        if (hudA > 0.02f) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, hudA);
            ImGui::SetNextWindowPos(ImVec2(16, 14));
            ImGui::Begin("##hud", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
            ImGui::PushFont(uiTitle);
            ImGui::TextColored(iv(neon(p.accent)), "OMABIBLIA");
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::TextDisabled("  %s  ·  %s", bible.refString(ctx->ref, !ctx->chapter).c_str(), bible.tr().code.c_str());
            for (int i = 0; i < (int)scenes.size(); i++) {
                if (i) ImGui::SameLine();
                bool sel = i == scene;
                if (sel) ImGui::PushStyleColor(ImGuiCol_Button, iv(mix4(p.deep, p.glowB, 0.55f), 1));
                char lbl[64];
                std::snprintf(lbl, sizeof lbl, "%d %s", i + 1, scenes[i]->name());
                if (ImGui::Button(lbl)) setScene(i);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", scenes[i]->blurb());
                if (sel) ImGui::PopStyleColor();
            }
            if (ImGui::Button("Books")) { showBooks = !showBooks; pickBook = -1; chip.play(Sfx::Blip); }
            ImGui::SameLine(); if (ImGui::Button("Search")) { showSearch = !showSearch; chip.play(Sfx::Blip); }
            ImGui::SameLine(); if (ImGui::Button("Today")) today();
            ImGui::SameLine(); if (ImGui::Button("Random")) random();
            ImGui::SameLine(); if (ImGui::Button(ctx->chapter ? "Chapter" : "Verse")) { ctx->chapter = !ctx->chapter; ctx->refSerial++; chip.play(Sfx::Blip); }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90);
            if (ImGui::BeginCombo("##tr", bible.tr().code.c_str())) {
                for (int i = 0; i < (int)bible.translations().size(); i++) {
                    auto& t = bible.translations()[i];
                    if (ImGui::Selectable((t.code + "  " + t.name).c_str(), i == bible.current())) setTranslation(i);
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine(); if (ImGui::Button("Settings")) showSettings = !showSettings;
            ImGui::SameLine(); if (ImGui::Button("?")) showHelp = !showHelp;
            ImGui::End();
            ImGui::PopStyleVar();
        }

        // attribution required for CC-licensed texts, always visible while that text is shown
        if (!bible.tr().attribution.empty()) {
            ImGui::SetNextWindowPos(ImVec2(16, io.DisplaySize.y - 16), 0, ImVec2(0, 1));
            ImGui::SetNextWindowBgAlpha(0.55f);
            ImGui::Begin("##attr", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                                ImGuiWindowFlags_NoSavedSettings);
            ImGui::PushTextWrapPos(io.DisplaySize.x * 0.6f);
            ImGui::TextDisabled("%s", bible.tr().attribution.c_str());
            ImGui::PopTextWrapPos();
            ImGui::End();
        }

        if (showBooks) booksWindow();
        if (showSearch) searchWindow();
        if (showSettings) settingsWindow();
        if (showHelp) helpWindow();
    }

    void booksWindow() {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(std::min(960.f, io.DisplaySize.x - 40), std::min(640.f, io.DisplaySize.y - 120)), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (!ImGui::Begin("Books", &showBooks, ImGuiWindowFlags_NoSavedSettings)) { ImGui::End(); return; }
        const auto& B = bible.books();
        if (pickBook < 0) {
            float colW = ImGui::GetContentRegionAvail().x / 2 - 8;
            for (int half = 0; half < 2; half++) {
                if (half) ImGui::SameLine();
                ImGui::BeginChild(half ? "nt" : "ot", ImVec2(colW, 0));
                ImGui::TextColored(iv(theme.pal.glowB), half ? "NEW TESTAMENT" : "OLD TESTAMENT");
                ImGui::Separator();
                int from = half ? 39 : 0, to = half ? 66 : 39, cols = 3, k = 0;
                float bw = (colW - 24) / cols;
                for (int i = from; i < to; i++, k++) {
                    if (k % cols) ImGui::SameLine();
                    bool cur = i == ctx->ref.book;
                    if (cur) ImGui::PushStyleColor(ImGuiCol_Button, iv(mix4(theme.pal.deep, theme.pal.glowB, 0.5f), 1));
                    if (ImGui::Button(B[i].name.c_str(), ImVec2(bw, 0))) {
                        chip.play(Sfx::Blip);
                        if (bible.chapterCount(i) == 1) { setRef({i, 1, 1}); ctx->chapter = true; showBooks = false; }
                        else pickBook = i;
                    }
                    if (cur) ImGui::PopStyleColor();
                }
                ImGui::EndChild();
            }
        } else {
            if (ImGui::Button("< Books")) { pickBook = -1; chip.play(Sfx::Back); }
            ImGui::SameLine();
            ImGui::TextColored(iv(theme.pal.glowB), "%s", B[pickBook].name.c_str());
            ImGui::Separator();
            int n = bible.chapterCount(pickBook);
            float avail = ImGui::GetContentRegionAvail().x;
            int cols = std::max(1, (int)(avail / 64));
            for (int c = 1; c <= n; c++) {
                if ((c - 1) % cols) ImGui::SameLine();
                if (ImGui::Button(std::to_string(c).c_str(), ImVec2(56, 40))) {
                    setRef({pickBook, c, 1});
                    ctx->chapter = true;
                    showBooks = false;
                }
            }
        }
        ImGui::End();
    }

    void searchWindow() {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(720, 480), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (!ImGui::Begin("Search", &showSearch, ImGuiWindowFlags_NoSavedSettings)) { ImGui::End(); return; }
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##q", "words or a reference (john 3:16), Enter", searchBuf, sizeof searchBuf, ImGuiInputTextFlags_EnterReturnsTrue)) {
            Ref r;
            std::string q = searchBuf;
            bool hasDigit = q.find_first_of("0123456789") != std::string::npos;
            if (hasDigit && bible.parseRef(q, r)) { setRef(r); showSearch = false; }
            else { hits = bible.search(q, 200); chip.play(hits.empty() ? Sfx::Error : Sfx::Blip); }
        }
        ImGui::TextDisabled("%d match%s in %s", (int)hits.size(), hits.size() == 1 ? "" : "es", bible.tr().code.c_str());
        ImGui::BeginChild("hits");
        for (auto& h : hits) {
            std::string label = bible.refString(h.ref) + "   " + h.text;
            if (ImGui::Selectable(label.c_str())) { setRef(h.ref); ctx->chapter = false; }
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void settingsWindow() {
        ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_Appearing);
        if (!ImGui::Begin("Settings", &showSettings, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
        ImGui::TextColored(iv(theme.pal.glowB), "SOUND");
        bool m = chip.music, s = chip.sfx;
        float v = chip.volume;
        if (ImGui::Checkbox("Chiptune music", &m)) chip.music = m;
        ImGui::SameLine(); if (ImGui::Checkbox("Blips", &s)) chip.sfx = s;
        if (ImGui::SliderFloat("Volume", &v, 0, 1)) chip.volume = v;
        if (!chip.running()) ImGui::TextDisabled("(no audio device found)");
        ImGui::Separator();
        ImGui::TextColored(iv(theme.pal.glowB), "SCREEN");
        bool threeD = !sp.flat;
        if (ImGui::Checkbox("3D floating screen (drag to rotate, wheel to zoom)", &threeD)) sp.flat = !threeD;
        ImGui::Checkbox("Cinematic swing (V)", &sp.cinematic);
        ImGui::SliderFloat("CRT", &sp.crt, 0, 1);
        ImGui::SliderFloat("Bloom", &sp.bloom, 0, 2);
        ImGui::SliderFloat("Curvature", &sp.curvature, 0, 2);
        ImGui::SliderFloat("Text size", &textScale, 0.6f, 2.0f);
        if (ImGui::Button("Reset view")) { sp.yaw = sp.pitch = 0; sp.zoom = 1; }
        ImGui::Separator();
        ImGui::TextColored(iv(theme.pal.glowB), "OMARCHY");
        ImGui::Text("Theme: %s (%s)", theme.pal.name.c_str(), theme.pal.dark ? "dark" : "light");
        ImGui::Text("Font: %s", theme.fontName.empty() ? "(default)" : theme.fontName.c_str());
        ImGui::TextWrapped("Wallpaper: %s", theme.wallpaperPath.empty() ? "(none)" : theme.wallpaperPath.c_str());
        ImGui::TextDisabled("Follows your Omarchy theme live: switch themes and Omabiblia recolours.");
        ImGui::End();
    }

    void helpWindow() {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(680, 560), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (!ImGui::Begin("Omabiblia", &showHelp, ImGuiWindowFlags_NoSavedSettings)) { ImGui::End(); return; }
        static const char* keys[][2] = {
            {"Left / Right", "previous / next verse"}, {"Up / Down", "previous / next chapter"}, {"PgUp / PgDn", "previous / next book"},
            {"Tab / Shift+Tab", "next / previous scene"}, {"1 - 5", "scene (Ctrl+1-5 in the hacker terminal)"},
            {"Space / R", "random verse"}, {"T", "verse of the day"}, {"C", "whole chapter / single verse"}, {"B", "books"},
            {"/ or Ctrl+F", "search"}, {"N", "next translation"}, {"M", "music on/off"}, {"+ / -", "text size"},
            {"Drag / wheel", "rotate / zoom the 3D screen (double-click resets)"}, {"V", "cinematic: the screen swings through depth"},
            {"F", "flat (2D) / floating 3D screen"}, {"P", "pop the screen out onto the desktop / back in"},
            {"Super+1..9", "pop-out lives on a workspace; Super+O pins it to all of them"}, {"F11", "fullscreen"}, {"H", "hide the bar"},
            {"Esc", "close panels (Ctrl+Q quits)"}};
        if (ImGui::BeginTable("keys", 2)) {
            for (auto& k : keys) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextColored(iv(theme.pal.glowB), "%s", k[0]);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(k[1]);
            }
            ImGui::EndTable();
        }
        ImGui::Separator();
        ImGui::TextColored(iv(theme.pal.glowB), "PRIVACY");
        ImGui::TextWrapped("Omabiblia has no network code. Every text is a local file; nothing you read leaves this computer. "
                           "Settings are saved only to ~/.config/omabiblia/omabiblia.ini.");
        ImGui::Separator();
        ImGui::TextColored(iv(theme.pal.glowB), "TRANSLATIONS");
        for (auto& t : bible.translations()) ImGui::BulletText("%s  %s (%s) - %s", t.code.c_str(), t.name.c_str(), t.year.c_str(), t.license.c_str());
        ImGui::TextDisabled("Details and attributions: data/translations/NOTICE.md");
        ImGui::End();
    }

    // ------------------------------------------------------------------ input
    void key(int k, int mods) {
        bool ctrl = mods & GLFW_MOD_CONTROL, shift = mods & GLFW_MOD_SHIFT;
        bool typing = scenes[scene]->wantsTyping();
        if (ctrl && k == GLFW_KEY_Q) { glfwSetWindowShouldClose(win, 1); return; }
        if (ctrl && k >= GLFW_KEY_1 && k <= GLFW_KEY_5) { setScene(k - GLFW_KEY_1); return; }
        if (ctrl && k == GLFW_KEY_F) { showSearch = true; return; }
        switch (k) {
            case GLFW_KEY_ESCAPE:
                if (showBooks || showSearch || showSettings || showHelp) { showBooks = showSearch = showSettings = showHelp = false; chip.play(Sfx::Back); }
                else if (typing && !termInput.empty()) termInput.clear();
                return;
            case GLFW_KEY_TAB: setScene(scene + (shift ? -1 : 1)); return;
            case GLFW_KEY_LEFT: setRef(bible.step(ctx->ref, -1)); return;
            case GLFW_KEY_RIGHT: setRef(bible.step(ctx->ref, 1)); return;
            case GLFW_KEY_UP: setRef(bible.stepChapter(ctx->ref, -1)); return;
            case GLFW_KEY_DOWN: setRef(bible.stepChapter(ctx->ref, 1)); return;
            case GLFW_KEY_PAGE_UP: setRef({(ctx->ref.book + 65) % 66, 1, 1}); return;
            case GLFW_KEY_PAGE_DOWN: setRef({(ctx->ref.book + 1) % 66, 1, 1}); return;
            case GLFW_KEY_F11: toggleFullscreen(); return;
            default: break;
        }
        if (typing) {
            if (k == GLFW_KEY_ENTER || k == GLFW_KEY_KP_ENTER) { std::string c = termInput; termInput.clear(); exec(c); }
            else if (k == GLFW_KEY_BACKSPACE && !termInput.empty()) { termInput.pop_back(); chip.play(Sfx::Key); }
            return;   // letters go to the prompt via the char callback
        }
        switch (k) {
            case GLFW_KEY_1: case GLFW_KEY_2: case GLFW_KEY_3: case GLFW_KEY_4: case GLFW_KEY_5: setScene(k - GLFW_KEY_1); break;
            case GLFW_KEY_SPACE: case GLFW_KEY_R: random(); break;
            case GLFW_KEY_T: today(); break;
            case GLFW_KEY_C: ctx->chapter = !ctx->chapter; ctx->refSerial++; chip.play(Sfx::Blip); break;
            case GLFW_KEY_B: showBooks = !showBooks; pickBook = -1; chip.play(Sfx::Blip); break;
            case GLFW_KEY_SLASH: showSearch = true; break;
            case GLFW_KEY_N: setTranslation((bible.current() + 1) % (int)bible.translations().size()); break;
            case GLFW_KEY_M: chip.music = !chip.music; break;
            case GLFW_KEY_H: showHud = !showHud; break;
            case GLFW_KEY_V: sp.cinematic = !sp.cinematic; chip.play(Sfx::Whoosh); break;
            case GLFW_KEY_F: sp.flat = !sp.flat; chip.play(Sfx::Blip); break;
            case GLFW_KEY_P: relaunch = true; glfwSetWindowShouldClose(win, 1); break;
            case GLFW_KEY_EQUAL: case GLFW_KEY_KP_ADD: textScale = std::min(2.0f, textScale + 0.1f); ctx->refSerial++; break;
            case GLFW_KEY_MINUS: case GLFW_KEY_KP_SUBTRACT: textScale = std::max(0.6f, textScale - 0.1f); ctx->refSerial++; break;
            case GLFW_KEY_F1: showHelp = !showHelp; break;
            default: break;
        }
    }
    void character(unsigned cp) {
        if (!scenes[scene]->wantsTyping() || cp < 32 || cp > 126 || termInput.size() > 120) return;
        termInput += (char)cp;
        chip.play(Sfx::Key);
    }
    int wx = 0, wy = 0, ww = 1280, wh = 800;
    void toggleFullscreen() {
        if (glfwGetWindowMonitor(win)) { glfwSetWindowMonitor(win, nullptr, wx, wy, ww, wh, 0); return; }
        glfwGetWindowPos(win, &wx, &wy);
        glfwGetWindowSize(win, &ww, &wh);
        GLFWmonitor* m = glfwGetPrimaryMonitor();
        const GLFWvidmode* vm = glfwGetVideoMode(m);
        glfwSetWindowMonitor(win, m, 0, 0, vm->width, vm->height, vm->refreshRate);
    }
};

App* g_app = nullptr;

void onKey(GLFWwindow*, int key, int, int action, int mods) {
    if (action == GLFW_RELEASE || !g_app) return;
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;   // typing into an ImGui field
    if (io.WantCaptureKeyboard && key != GLFW_KEY_ESCAPE && key != GLFW_KEY_TAB && action == GLFW_PRESS && ImGui::IsAnyItemActive()) return;
    g_app->key(key, mods);
}
void onChar(GLFWwindow*, unsigned cp) {
    if (!g_app || ImGui::GetIO().WantTextInput) return;
    g_app->character(cp);
}
void onScroll(GLFWwindow*, double, double dy) {
    if (!g_app || ImGui::GetIO().WantCaptureMouse) return;
    g_app->sp.zoom = std::clamp(g_app->sp.zoom * (float)std::pow(1.1, dy), 0.5f, 2.5f);
}

}  // namespace

int main(int argc, char** argv) {
    // --shot out.png [--scene N] [--size WxH] [--seconds S] [--ref "john 3:16"] [--chapter] [--ui] : render hidden, save, exit
    std::string shotPath, shotRef, shotTr;
    int shotScene = -1, shotW = 1600, shotH = 1000;
    float shotSeconds = 2.5f;
    bool shotChapter = false, shotUi = false;
    float shotYaw = 0;
    bool popoutArg = false;
    std::vector<std::string> rest;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--shot" && i + 1 < argc) shotPath = argv[++i];
        else if (a == "--scene" && i + 1 < argc) shotScene = std::atoi(argv[++i]) - 1;
        else if (a == "--size" && i + 1 < argc) std::sscanf(argv[++i], "%dx%d", &shotW, &shotH);
        else if (a == "--seconds" && i + 1 < argc) shotSeconds = (float)std::atof(argv[++i]);
        else if (a == "--ref" && i + 1 < argc) shotRef = argv[++i];
        else if (a == "--tr" && i + 1 < argc) shotTr = argv[++i];
        else if (a == "--chapter") shotChapter = true;
        else if (a == "--ui") shotUi = true;
        else if (a == "--popout") popoutArg = true;
        else if (a == "--yaw" && i + 1 < argc) shotYaw = (float)std::atof(argv[++i]);
        else rest.push_back(a);
    }
    const bool shot = !shotPath.empty();
    App app;
    g_app = &app;
    app.popout = popoutArg;
    app.sp.popout = popoutArg;
    app.root = findRoot();
    std::string err;
    if (!app.bible.open(app.root + "/data", err)) { std::fprintf(stderr, "omabiblia: %s\n", err.c_str()); return 1; }

    if (!glfwInit()) { std::fprintf(stderr, "omabiblia: GLFW init failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, "omabiblia");
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "omabiblia");
    if (shot) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    if (app.popout) {
        // its own app id so Hyprland can float it borderless and fully opaque-capable (see install.sh)
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHintString(GLFW_WAYLAND_APP_ID, "omabiblia-popout");
        glfwWindowHintString(GLFW_X11_CLASS_NAME, "omabiblia-popout");
    }
    app.win = glfwCreateWindow(app.popout ? 1280 : 1440, app.popout ? 820 : 900, app.popout ? "Omabiblia (pop-out)" : "Omabiblia", nullptr, nullptr);
    if (!app.win) { std::fprintf(stderr, "omabiblia: cannot open a window\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(app.win);
    glfwSwapInterval(1);
    if (!omaLoadGL(reinterpret_cast<void* (*)(const char*)>(glfwGetProcAddress))) return 1;
    std::fprintf(stderr, "omabiblia: GL %s on %s\n", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));

    if (app.popout && !shot) app.region.init(app.win);
    glfwSetKeyCallback(app.win, onKey);
    glfwSetCharCallback(app.win, onChar);
    glfwSetScrollCallback(app.win, onScroll);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;            // no ImGui layout file written anywhere
    ImGui_ImplGlfw_InitForOpenGL(app.win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    app.theme.load();
    float scale = 1;
    { float sx, sy; glfwGetWindowContentScale(app.win, &sx, &sy); scale = std::max(1.f, sx); }
    std::string uiFontPath = app.theme.fontPath.empty() ? app.root + "/assets/fonts/ShareTechMono-Regular.ttf" : app.theme.fontPath;
    app.uiFont = io.Fonts->AddFontFromFileTTF(uiFontPath.c_str(), 19 * scale);
    if (!app.uiFont) app.uiFont = io.Fonts->AddFontFromFileTTF((app.root + "/assets/fonts/ShareTechMono-Regular.ttf").c_str(), 17 * scale);
    app.uiTitle = io.Fonts->AddFontFromFileTTF((app.root + "/assets/fonts/Audiowide-Regular.ttf").c_str(), 26 * scale);
    if (!app.uiTitle) app.uiTitle = app.uiFont;
    io.FontDefault = app.uiFont;
    app.applyStyle();

    app.r.init();
    app.screen.init();
    app.screen.setWallpaper(app.theme.wallpaperPath);
    app.loadFonts();
    app.scenes.push_back(makeCrawl());
    app.scenes.push_back(makeLink());
    app.scenes.push_back(makeCommand());
    app.scenes.push_back(makeHacker());
    app.scenes.push_back(makeRetro());

    char host[256] = "localhost";
    gethostname(host, sizeof host);
    app.ctx = std::make_unique<Ctx>(Ctx{app.r, app.fonts, app.theme.pal, app.bible, app.chip});
    app.ctx->hostname = shot ? "omarchy" : host;      // screenshots never show the real machine name
    app.ctx->termLines = &app.termLines;
    app.ctx->termInput = &app.termInput;
    app.loadPrefs();
    for (size_t i = 0; i < rest.size(); i++) {          // omabiblia [scene-number] [reference...]
        const std::string& a = rest[i];
        if (a.size() == 1 && a[0] >= '1' && a[0] <= '5') app.scene = a[0] - '1';
        else { std::string q; for (size_t j = i; j < rest.size(); j++) q += rest[j] + " "; Ref r; if (app.bible.parseRef(q, r)) app.ctx->ref = r; break; }
    }
    if (shot) {
        if (shotScene >= 0 && shotScene < 5) app.scene = shotScene;
        for (int i = 0; i < (int)app.bible.translations().size(); i++) if (app.bible.translations()[i].code == shotTr) app.bible.select(i);
        Ref r;
        if (!shotRef.empty() && app.bible.parseRef(shotRef, r)) app.ctx->ref = r;
        app.ctx->chapter = shotChapter;
        app.chip.music = false; app.chip.sfx = false;
        app.showHud = shotUi;
        app.hudA = shotUi ? 1.f : 0.f;
        app.sp.yaw = shotYaw; app.sp.cinematic = false;
        if (shotUi) app.lastMouseMove = 1e9;
    }
    app.say("omabiblia: offline reader. " + std::to_string(app.bible.translations().size()) + " translations on disk. type help");
    if (!shot) app.chip.start();
    RenderTarget shotRT;
    if (shot) { shotRT.ensure(shotW, shotH, false, true, false); g_defaultFbo = shotRT.fbo; }
    double simT = 10.0;
    app.chip.setSong(app.scenes[app.scene]->song());
    app.chip.play(Sfx::Boot);
    app.scenes[app.scene]->enter(*app.ctx);

    double prev = glfwGetTime();
    double lastClick = -1;
    while (!glfwWindowShouldClose(app.win)) {
        glfwPollEvents();
        double now = shot ? (simT += 1.0 / 60) : glfwGetTime();
        float dt = (float)std::min(0.1, now - prev);
        prev = now;
        app.ctx->t = (float)now;
        app.ctx->dt = dt;
        app.ctx->textScale = app.textScale;

        // follow Omarchy theme switches
        if (now - app.themeCheck > 1.0) {
            app.themeCheck = now;
            if (app.theme.poll()) {
                app.applyStyle();
                app.screen.setWallpaper(app.theme.wallpaperPath);
                app.chip.play(Sfx::Whoosh);
            }
        }

        // mouse: drag rotates the 3D screen
        double mx, my;
        glfwGetCursorPos(app.win, &mx, &my);
        if (mx != app.lastX || my != app.lastY) app.lastMouseMove = now;
        bool down = glfwGetMouseButton(app.win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (down && !io.WantCaptureMouse) {
            if (!app.dragging) {
                app.dragging = true;
                if (now - lastClick < 0.3) { app.sp.yaw = app.sp.pitch = 0; app.sp.zoom = 1; }
                lastClick = now;
            } else {
                app.sp.yaw = std::clamp(app.sp.yaw + (float)(mx - app.lastX) * 0.004f, -1.1f, 1.1f);
                app.sp.pitch = std::clamp(app.sp.pitch + (float)(my - app.lastY) * 0.004f, -0.8f, 0.8f);
            }
        } else app.dragging = false;
        app.lastX = mx; app.lastY = my;

        // ---- scene into its texture ----
        Scene& sc = *app.scenes[app.scene];
        GLuint sceneTex;
        int sw, sh;
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        if (sc.lowRes()) {
            app.lowRT.ensure(320, 200, false, false, true);
            app.lowRT.bind();
            app.ctx->w = 320; app.ctx->h = 200;
            sc.draw(*app.ctx);
            sceneTex = app.lowRT.tex; sw = 320; sh = 200;
        } else {
            app.sceneRT.ensure(1600, 1000, false, true, true);
            app.sceneRT.bind();
            app.ctx->w = 1600; app.ctx->h = 1000;
            sc.draw(*app.ctx);
            sceneTex = app.sceneRT.tex; sw = 1600; sh = 1000;
        }
        int W, H;
        glfwGetFramebufferSize(app.win, &W, &H);
        if (shot) { W = shotW; H = shotH; io.DisplaySize = ImVec2((float)W, (float)H); }
        app.sp.retro = sc.lowRes();
        app.screen.render(W, H, sceneTex, sw, sh, app.theme.pal, (float)now, app.sp);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        if (shot) { io.DisplaySize = ImVec2((float)W, (float)H); io.DisplayFramebufferScale = ImVec2(1, 1); }
        ImGui::NewFrame();
        app.ui();
        ImGui::Render();
        glxBindDefault(W, H);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (app.popout && !shot) {
            // only the monitor and visible panels take clicks; the rest of the window is the desktop
            int lw, lh;
            glfwGetWindowSize(app.win, &lw, &lh);
            float kx = lw / (float)std::max(1, W), ky = lh / (float)std::max(1, H);
            std::vector<InRect> rs;
            rs.push_back({(int)(app.screen.rectX0 * kx), (int)(app.screen.rectY0 * ky),
                          (int)((app.screen.rectX1 - app.screen.rectX0) * kx) + 1, (int)((app.screen.rectY1 - app.screen.rectY0) * ky) + 1});
            for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows)
                if (w->WasActive && !w->Hidden && !(w->Flags & ImGuiWindowFlags_NoInputs))
                    rs.push_back({(int)w->Pos.x, (int)w->Pos.y, (int)w->Size.x + 1, (int)w->Size.y + 1});
            app.region.set(rs);
        }
        if (shot && now - 10.0 >= shotSeconds) {
            std::vector<unsigned char> px((size_t)W * H * 4);
            glBindFramebuffer(GL_FRAMEBUFFER, shotRT.fbo);
            glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            stbi_flip_vertically_on_write(1);
            int ok = stbi_write_png(shotPath.c_str(), W, H, 4, px.data(), W * 4);
            std::fprintf(stderr, "omabiblia: %s %s (%dx%d, scene %s)\n", ok ? "wrote" : "FAILED", shotPath.c_str(), W, H, app.scenes[app.scene]->name());
            break;
        }
        if (!shot) glfwSwapBuffers(app.win);
    }
    if (!shot) app.savePrefs();
    app.chip.stop();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(app.win);
    glfwTerminate();
    if (app.relaunch) {
        // swap modes by re-executing ourselves (a transparent framebuffer can only be chosen at window creation)
        std::string self = "/proc/self/exe";
        std::vector<char*> args{argv[0]};
        static char flag[] = "--popout";
        if (!app.popout) args.push_back(flag);
        args.push_back(nullptr);
        execv(self.c_str(), args.data());
        std::perror("omabiblia: relaunch failed");
    }
    return 0;
}
