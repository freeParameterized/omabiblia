// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// Omabiblia - pop-out click-through on Wayland: the window only accepts input inside the
// rectangles we give it (the monitor and any open panels); clicks elsewhere reach the desktop.
#pragma once
#include <vector>

struct GLFWwindow;
struct InRect { int x, y, w, h; };      // surface-local logical px, y down

class InputRegion {
public:
    bool init(GLFWwindow* w);            // false on X11 / when the compositor lacks wl_compositor
    void set(const std::vector<InRect>& rects);
    void clear();                        // whole window accepts input again
private:
    void* display_ = nullptr;
    void* surface_ = nullptr;
    void* compositor_ = nullptr;
    std::vector<InRect> last_;
};
