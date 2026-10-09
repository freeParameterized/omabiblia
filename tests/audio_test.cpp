// Renders every scene's song plus all sound effects offline and checks the signal is sane.
// Writes tests/out/song<N>.wav so you can listen without the app.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "audio.h"

static void wav(const char* path, const std::vector<float>& st) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return;
    uint32_t n = (uint32_t)st.size(), bytes = n * 2, rate = 48000;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32(rate); w32(rate * 4); w16(4); w16(16);
    std::fwrite("data", 1, 4, f); w32(bytes);
    for (float s : st) { int16_t v = (int16_t)std::lround(std::fmax(-1.f, std::fmin(1.f, s)) * 32767); w16((uint16_t)v); }
    std::fclose(f);
}

int main() {
    int fails = 0;
    for (int song = 0; song < 5; song++) {
        Chip c;
        c.setSong(song);
        std::vector<float> buf(48000 * 2 * 8);
        for (size_t off = 0; off < buf.size(); off += 512 * 2) {
            if (off == 48000 * 2) for (int e = 0; e < 7; e++) c.play((Sfx)e);
            c.render(buf.data() + off, 512);
        }
        double rms = 0; float peak = 0; bool nan = false;
        for (float s : buf) { if (!std::isfinite(s)) nan = true; rms += s * s; peak = std::fmax(peak, std::fabs(s)); }
        rms = std::sqrt(rms / buf.size());
        bool ok = !nan && peak < 0.99f && rms > 0.01;
        std::printf("song %d: rms %.3f peak %.3f %s\n", song, rms, peak, ok ? "ok" : "FAIL");
        fails += !ok;
        char p[64]; std::snprintf(p, sizeof p, "tests/out/song%d.wav", song + 1);
        wav(p, buf);
    }
    return fails;
}
