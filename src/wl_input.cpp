// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
#include "wl_input.h"
#include <cstdio>
#include <cstring>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
#include <wayland-client.h>

namespace {
void onGlobal(void* data, wl_registry* reg, uint32_t name, const char* iface, uint32_t version) {
    if (std::strcmp(iface, wl_compositor_interface.name) == 0)
        *static_cast<wl_compositor**>(data) =
            static_cast<wl_compositor*>(wl_registry_bind(reg, name, &wl_compositor_interface, version < 4 ? version : 4));
}
void onGlobalRemove(void*, wl_registry*, uint32_t) {}
const wl_registry_listener kListener = {onGlobal, onGlobalRemove};
}  // namespace

bool InputRegion::init(GLFWwindow* w) {
    if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND) return false;
    auto* dpy = glfwGetWaylandDisplay();
    auto* surf = glfwGetWaylandWindow(w);
    if (!dpy || !surf) return false;
    wl_compositor* comp = nullptr;
    wl_registry* reg = wl_display_get_registry(dpy);
    wl_registry_add_listener(reg, &kListener, &comp);
    wl_display_roundtrip(dpy);
    wl_registry_destroy(reg);
    if (!comp) { std::fprintf(stderr, "omabiblia: no wl_compositor; pop-out stays clickable everywhere\n"); return false; }
    display_ = dpy; surface_ = surf; compositor_ = comp;
    return true;
}

void InputRegion::set(const std::vector<InRect>& rects) {
    if (!surface_) return;
    bool same = rects.size() == last_.size();
    for (size_t i = 0; same && i < rects.size(); i++)
        same = std::memcmp(&rects[i], &last_[i], sizeof(InRect)) == 0;
    if (same) return;
    last_ = rects;
    wl_region* r = wl_compositor_create_region(static_cast<wl_compositor*>(compositor_));
    for (const auto& q : rects)
        if (q.w > 0 && q.h > 0) wl_region_add(r, q.x, q.y, q.w, q.h);
    wl_surface_set_input_region(static_cast<wl_surface*>(surface_), r);   // applied with the next buffer commit
    wl_region_destroy(r);
}

void InputRegion::clear() {
    if (!surface_) return;
    last_.clear();
    wl_surface_set_input_region(static_cast<wl_surface*>(surface_), nullptr);
}
