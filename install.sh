#!/bin/bash
# Build Omabiblia and install it for this user (~/.local), with an Omarchy app-launcher entry.
set -euo pipefail
cd "$(dirname "$0")"
PREFIX="${PREFIX:-$HOME/.local}"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" >/dev/null
cmake --build build -j"$(( $(nproc) > 6 ? 6 : $(nproc) ))"
cmake --install build >/dev/null

mkdir -p "$PREFIX/share/applications" "$PREFIX/share/icons/hicolor/256x256/apps"
cp assets/icon.png "$PREFIX/share/icons/hicolor/256x256/apps/omabiblia.png"
cat > "$PREFIX/share/applications/omabiblia.desktop" <<EOF
[Desktop Entry]
Name=Omabiblia
Comment=Offline Bible reader: 3D CRT screen, cyberspace, space command, hacker terminal, 8-bit
Exec=$PREFIX/bin/omabiblia
Icon=$PREFIX/share/icons/hicolor/256x256/apps/omabiblia.png
Type=Application
Categories=Education;
Terminal=false
StartupWMClass=omabiblia
EOF
cat > "$PREFIX/share/applications/omabiblia-popout.desktop" <<EOF
[Desktop Entry]
Name=Omabiblia Pop-out
Comment=Omabiblia's 3D screen floating right on the desktop (transparent, click-through around it)
Exec=$PREFIX/bin/omabiblia --popout
Icon=$PREFIX/share/icons/hicolor/256x256/apps/omabiblia.png
Type=Application
Categories=Education;
Terminal=false
StartupWMClass=omabiblia-popout
EOF

# Hyprland: float the pop-out borderless and fully opaque-capable (Omarchy's default opacity would
# make the whole screen translucent). It stays on the workspace it was opened on (Super+1..9);
# Super+O pins it to every workspace.
HYPR_LUA="$HOME/.config/hypr/hyprland.lua"
HYPR_CONF="$HOME/.config/hypr/hyprland.conf"
MARKER="-- Omabiblia pop-out window rules (added by install.sh)"
if [ -f "$HYPR_LUA" ]; then
    if ! grep -qF -- "$MARKER" "$HYPR_LUA"; then
        cp "$HYPR_LUA" "$HYPR_LUA.bak-omabiblia"
        cat >> "$HYPR_LUA" <<'EOF'

-- Omabiblia pop-out window rules (added by install.sh)
o.window("^omabiblia-popout$", {
    tag = "-default-opacity",
    opacity = "1 1",
    float = true,
    center = true,
    size = { 1280, 820 },
    border_size = 0,
    no_shadow = true,
    no_blur = true,
    no_dim = true
})
EOF
        echo "Added pop-out window rule to $HYPR_LUA (backup: $HYPR_LUA.bak-omabiblia)"
    fi
elif [ -f "$HYPR_CONF" ] && ! grep -qF "omabiblia-popout" "$HYPR_CONF"; then
    cat >> "$HYPR_CONF" <<'EOF'

# Omabiblia pop-out window rules (added by install.sh)
windowrulev2 = float, class:^(omabiblia-popout)$
windowrulev2 = size 1280 820, class:^(omabiblia-popout)$
windowrulev2 = center, class:^(omabiblia-popout)$
windowrulev2 = noborder, class:^(omabiblia-popout)$
windowrulev2 = noshadow, class:^(omabiblia-popout)$
windowrulev2 = noblur, class:^(omabiblia-popout)$
windowrulev2 = nodim, class:^(omabiblia-popout)$
windowrulev2 = opacity 1 override 1 override, class:^(omabiblia-popout)$
EOF
    echo "Added pop-out window rules to $HYPR_CONF"
fi
command -v update-desktop-database >/dev/null && update-desktop-database "$PREFIX/share/applications" 2>/dev/null || true
echo "Installed: $PREFIX/bin/omabiblia  (launcher: Omabiblia)"
