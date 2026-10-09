// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
#include "audio.h"
#include <cmath>
#include <cstdint>
#include <cstdio>

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio.h"

namespace {
constexpr float SR = 48000.f;

float midiHz(float n) { return 440.f * std::pow(2.f, (n - 69.f) / 12.f); }

struct Voice {
    enum Wave { Pulse, Tri, Noise } wave = Pulse;
    float duty = 0.5f, phase = 0, freq = 440, slide = 0;   // slide: semitones per second
    float env = 0, attack = 0.002f, decay = 0.12f, sustain = 0.4f, release = 0.08f, gain = 0.2f;
    bool gate = false, active = false;
    float vib = 0, vibPhase = 0;
    uint16_t lfsr = 1;
    float noiseHold = 0, noiseVal = 0;
    void on(float hz, float g) { freq = hz; gain = g; gate = true; active = true; phaseAttack = true; }
    void off() { gate = false; }
    float tick() {
        if (!active) return 0;
        // envelope
        if (gate) {
            if (phaseAttack) { env += 1 / (attack * SR); if (env >= 1) { env = 1; phaseAttack = false; } }
            else { phaseAttack = false; env += (sustain - env) * (1 / (decay * SR)) * 4; }
        } else {
            env -= 1 / (release * SR);
            if (env <= 0) { env = 0; active = false; phaseAttack = true; return 0; }
        }
        if (slide != 0) freq *= std::pow(2.f, slide / 12.f / SR);
        float f = freq;
        if (vib > 0) { vibPhase += 5.5f / SR; f *= 1 + vib * std::sin(vibPhase * 6.2831853f); }
        phase += f / SR;
        if (phase >= 1) phase -= std::floor(phase);
        float s = 0;
        switch (wave) {
            case Pulse: s = phase < duty ? 1.f : -1.f; break;
            case Tri: { float t = phase * 4; s = t < 1 ? t : t < 3 ? 2 - t : t - 4; s = std::round(s * 7.5f) / 7.5f; break; }   // 4-bit-ish triangle
            case Noise:
                noiseHold += f / SR;
                if (noiseHold >= 1) { noiseHold -= 1; uint16_t bit = ((lfsr >> 0) ^ (lfsr >> 1)) & 1; lfsr = (lfsr >> 1) | (bit << 14); noiseVal = (lfsr & 1) ? 1.f : -1.f; }
                s = noiseVal; break;
        }
        return s * env * gain;
    }
    bool phaseAttack = true;
};

struct Song {
    int tempo;                 // bpm
    int chords[4][3];          // midi notes of each bar's triad (bar = 16 steps)
    int bassOct;               // bass root octave offset
    float arpDuty, leadDuty;
    int arpPattern;            // 0 up, 1 up-down, 2 broken
    int drums;                 // 0 none, 1 four-on-floor, 2 breakbeat, 3 sparse
    float swing;
};

// Original progressions (no borrowed melodies): synthwave / terminal / space / 8-bit / grid
const Song kSongs[5] = {
    {92,  {{57, 60, 64}, {53, 57, 60}, {48, 52, 55}, {55, 59, 62}}, -12, 0.125f, 0.25f, 1, 3, 0.0f},   // crawl: Am F C G, slow and wide
    {118, {{54, 57, 61}, {50, 54, 57}, {57, 61, 64}, {52, 56, 59}}, -12, 0.25f, 0.5f, 0, 1, 0.0f},    // link: F#m D A E, driving
    {84,  {{52, 55, 59}, {48, 52, 55}, {55, 59, 62}, {50, 54, 57}}, -24, 0.5f, 0.125f, 2, 3, 0.0f},   // space command: Em C G D, slow pads
    {136, {{50, 53, 57}, {46, 50, 53}, {53, 57, 60}, {48, 52, 55}}, -12, 0.125f, 0.25f, 0, 2, 0.0f},  // hacker: Dm Bb F C, fast arps
    {120, {{48, 52, 55}, {45, 48, 52}, {53, 57, 60}, {55, 59, 62}}, -12, 0.25f, 0.5f, 1, 2, 0.12f},  // retro: C Am F G, bouncy
};

struct Synth {
    Voice lead, arp, bass, drum, fx, fx2;
    int step = -1;
    double stepClock = 0;
    uint32_t rng = 12345;
    int bar = 0;
    float rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (rng >> 8) / 16777216.f; }
    Synth() { lead.wave = Voice::Pulse; arp.wave = Voice::Pulse; bass.wave = Voice::Tri; drum.wave = Voice::Noise; fx.wave = Voice::Pulse; fx2.wave = Voice::Noise; }

    void sequence(const Song& s) {
        step = (step + 1) % 64;
        int barIx = step / 16, st = step % 16;
        if (st == 0) bar++;
        const int* ch = s.chords[barIx];
        // arpeggio: 16ths
        int order[3] = {0, 1, 2};
        int k = st % (s.arpPattern == 1 ? 4 : 3);
        int noteIx = s.arpPattern == 0 ? order[k % 3] : s.arpPattern == 1 ? (k == 3 ? 1 : k) : (st % 4 == 0 ? 0 : (st % 4 == 2 ? 2 : 1));
        int oct = (st / 4) % 2 ? 12 : 0;
        arp.duty = s.arpDuty; arp.attack = 0.001f; arp.decay = 0.05f; arp.sustain = 0.15f; arp.release = 0.04f;
        arp.on(midiHz(ch[noteIx] + 12 + oct), 0.07f);
        // bass: roots on 8ths with octave hops
        if (st % 2 == 0) {
            bass.attack = 0.002f; bass.decay = 0.1f; bass.sustain = 0.6f; bass.release = 0.05f;
            bass.on(midiHz(ch[0] + s.bassOct + ((st % 4 == 2) ? 12 : 0)), 0.22f);
        }
        // lead: sparse melody from chord tones + passing notes (deterministic per bar)
        if (st % 4 == 0 && rnd() < 0.55f) {
            static const int scaleSteps[] = {0, 2, 3, 5, 7, 8, 10, 12};
            int base = ch[0] + 12;
            int n = (rnd() < 0.6f) ? ch[(int)(rnd() * 3)] + 12 : base + scaleSteps[(int)(rnd() * 8)];
            lead.duty = s.leadDuty; lead.attack = 0.004f; lead.decay = 0.25f; lead.sustain = 0.35f; lead.release = 0.15f;
            lead.vib = 0.004f;
            lead.on(midiHz(n), 0.08f);
        } else if (st % 4 == 3) lead.off();
        // drums
        bool kick = false, snare = false, hat = false;
        switch (s.drums) {
            case 1: kick = st % 4 == 0; snare = st % 8 == 4; hat = st % 2 == 1; break;
            case 2: kick = st == 0 || st == 6 || st == 10; snare = st == 4 || st == 12; hat = st % 2 == 0; break;
            case 3: kick = st == 0; snare = st == 8; hat = st % 4 == 2; break;
        }
        if (kick) { drum.wave = Voice::Tri; drum.freq = 0; drum.slide = -90; drum.attack = 0.001f; drum.decay = 0.06f; drum.sustain = 0; drum.release = 0.05f; drum.on(midiHz(45), 0.35f); }
        else if (snare) { drum.wave = Voice::Noise; drum.slide = 0; drum.attack = 0.001f; drum.decay = 0.08f; drum.sustain = 0; drum.release = 0.06f; drum.on(9000, 0.12f); }
        else if (hat) { drum.wave = Voice::Noise; drum.slide = 0; drum.attack = 0.001f; drum.decay = 0.02f; drum.sustain = 0; drum.release = 0.02f; drum.on(16000, 0.04f); }
        if (st % 2 == 1) arp.off();
    }
    void trigger(Sfx e) {
        auto set = [](Voice& v, Voice::Wave w, float duty, float a, float d, float s, float r, float slide) {
            v.wave = w; v.duty = duty; v.attack = a; v.decay = d; v.sustain = s; v.release = r; v.slide = slide; v.vib = 0;
        };
        switch (e) {
            case Sfx::Blip:   set(fx, Voice::Pulse, 0.25f, 0.001f, 0.04f, 0, 0.03f, 24); fx.on(midiHz(84), 0.12f); break;
            case Sfx::Select: set(fx, Voice::Pulse, 0.125f, 0.001f, 0.12f, 0, 0.06f, 0); fx.on(midiHz(79), 0.12f); pendingArp = 3; arpBase = 79; break;
            case Sfx::Back:   set(fx, Voice::Pulse, 0.5f, 0.001f, 0.08f, 0, 0.05f, -30); fx.on(midiHz(72), 0.1f); break;
            case Sfx::Whoosh: set(fx2, Voice::Noise, 0.5f, 0.05f, 0.4f, 0, 0.3f, -40); fx2.on(6000, 0.08f); break;
            case Sfx::Boot:   set(fx, Voice::Pulse, 0.25f, 0.001f, 0.1f, 0.2f, 0.1f, 0); fx.on(midiHz(60), 0.1f); pendingArp = 6; arpBase = 60; break;
            case Sfx::Key:    set(fx2, Voice::Noise, 0.5f, 0.0005f, 0.01f, 0, 0.01f, 0); fx2.on(12000, 0.05f); break;
            case Sfx::Error:  set(fx, Voice::Pulse, 0.5f, 0.001f, 0.2f, 0, 0.1f, -12); fx.on(midiHz(45), 0.12f); break;
        }
        arpTimer = 0;
    }
    int pendingArp = 0, arpBase = 0;
    float arpTimer = 0;
};
Synth g_synth;
}  // namespace

static void callback(ma_device* d, void* out, const void*, ma_uint32 frames) {
    static_cast<Chip*>(d->pUserData)->render(static_cast<float*>(out), frames);
}

void Chip::render(float* out, unsigned frames) {
    Synth& s = g_synth;
    const Song& song = kSongs[song_.load() % 5];
    unsigned q = sfxQueue_.exchange(0);
    for (int i = 0; i < 7; i++) if (q & (1u << i)) s.trigger((Sfx)i);
    float vol = volume.load();
    bool mus = music.load();
    double stepLen = 60.0 / song.tempo / 4.0 * SR;
    for (unsigned i = 0; i < frames; i++) {
        if (mus) {
            s.stepClock -= 1;
            if (s.stepClock <= 0) {
                s.sequence(song);
                double sw = (s.step % 2 == 0) ? (1 + song.swing) : (1 - song.swing);
                s.stepClock += stepLen * sw;
            }
        } else if (s.arp.active || s.bass.active || s.lead.active) { s.arp.off(); s.bass.off(); s.lead.off(); }
        if (s.pendingArp > 0) {
            s.arpTimer += 1 / SR;
            if (s.arpTimer > 0.045f) {
                s.arpTimer = 0;
                static const int up[] = {0, 4, 7, 12, 16, 19};
                int k = (s.arpBase == 60 ? 6 : 3) - s.pendingArp;
                s.fx.on(midiHz(s.arpBase + up[k % 6]), 0.1f);
                s.pendingArp--;
            }
        }
        float m = s.arp.tick() + s.bass.tick() + s.lead.tick() + s.drum.tick();
        float f = s.fx.tick() + s.fx2.tick();
        float v = (m * 0.8f + f) * vol;
        v = std::tanh(v * 1.4f) * 0.8f;          // gentle saturation, never clips
        out[i * 2] = v;
        out[i * 2 + 1] = v;
    }
}

bool Chip::start() {
    auto* dev = new ma_device;
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate = (ma_uint32)SR;
    cfg.dataCallback = callback;
    cfg.pUserData = this;
    if (ma_device_init(nullptr, &cfg, dev) != MA_SUCCESS || ma_device_start(dev) != MA_SUCCESS) {
        std::fprintf(stderr, "omabiblia: no audio device (running silent)\n");
        delete dev;
        return false;
    }
    dev_ = dev;
    return true;
}

void Chip::stop() {
    if (!dev_) return;
    ma_device_uninit(static_cast<ma_device*>(dev_));
    delete static_cast<ma_device*>(dev_);
    dev_ = nullptr;
}

void Chip::play(Sfx e) { if (sfx.load()) sfxQueue_.fetch_or(1u << (int)e); }
void Chip::setSong(int scene) { song_.store(scene); }
