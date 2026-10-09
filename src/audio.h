// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// Omabiblia - chiptune synth: pulse / triangle / noise voices, a 16th-note sequencer with
// original per-scene progressions, and UI sound effects. Runs on miniaudio's audio thread.
#pragma once
#include <atomic>

enum class Sfx { Blip, Select, Back, Whoosh, Boot, Key, Error };

class Chip {
public:
    bool start();
    void stop();
    void play(Sfx s);
    void setSong(int scene);                 // 0..4, chooses progression / tempo / instruments
    std::atomic<bool> music{true};
    std::atomic<bool> sfx{true};
    std::atomic<float> volume{0.55f};
    bool running() const { return dev_ != nullptr; }
    void render(float* out, unsigned frames);   // audio thread
private:
    void* dev_ = nullptr;
    std::atomic<int> song_{0};
    std::atomic<unsigned> sfxQueue_{0};          // bitmask of pending effects
};
