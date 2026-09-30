#!/usr/bin/env bash
# End-to-end test under a nested GNOME Shell running as a Wayland compositor
# inside Xvfb, with its own D-Bus session, dconf, portals and media-keys
# daemon. Nothing touches the real desktop.
#
#   scripts/e2e-gnome-wayland.sh [path/to/binary]
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
bin=$(realpath "${1:-$root/build/lupka}")

if [[ "${E2E_INNER:-}" != 1 ]]; then
    work=$(mktemp -d /tmp/lupka-gnome.XXXXXX)
    mkdir -p "$work/run" && chmod 700 "$work/run"
    display=":$(( 70 + RANDOM % 9 ))"
    Xvfb "$display" -screen 0 1920x1080x24 -nolisten tcp +extension GLX >/dev/null 2>&1 &
    xvfb=$!
    trap 'kill $xvfb 2>/dev/null; wait $xvfb 2>/dev/null' EXIT
    for _ in $(seq 50); do xdpyinfo -display "$display" >/dev/null 2>&1 && break; sleep 0.1; done
    env -i HOME="$HOME" PATH="$PATH" USER="$USER" LANG=C.UTF-8 SHELL=/bin/bash \
        DISPLAY="$display" XDG_CONFIG_HOME="$work/config" XDG_DATA_HOME="$work/data" \
        XDG_CACHE_HOME="$work/cache" XDG_RUNTIME_DIR="$work/run" XDG_CURRENT_DESKTOP=GNOME \
        XDG_SESSION_TYPE=wayland XDG_DATA_DIRS=/usr/local/share:/usr/share \
        MUTTER_DEBUG_DUMMY_MODE_SPECS=1920x1080 E2E_INNER=1 WORK="$work" \
        dbus-run-session -- "$0" "$bin"
    status=$?
    echo "artifacts: $work"
    exit $status
fi

pattern="$here/testpattern.png"
pass=0
fail=0
ok() { echo "PASS  $1"; pass=$((pass + 1)); }
bad() { echo "FAIL  $1"; fail=$((fail + 1)); }
shot() { import -window root "$WORK/$1.png"; }
rmse() { compare -metric RMSE "$1" "$2" null: 2>&1 | sed -E 's/.*\((.*)\).*/\1/'; }
similar() { awk -v v="$(rmse "$1" "$2")" -v t="${3:-0.05}" 'BEGIN { exit !(v < t) }'; }
expect_similar() {
    local value
    value=$(rmse "$2" "$3")
    if awk -v v="$value" -v t="${4:-0.05}" 'BEGIN { exit !(v < t) }'; then ok "$1 (rmse $value)"; else bad "$1 (rmse $value)"; fi
}
expect_different() {
    local value
    value=$(rmse "$2" "$3")
    if awk -v v="$value" -v t="${4:-0.05}" 'BEGIN { exit !(v >= t) }'; then ok "$1 (rmse $value)"; else bad "$1 (rmse $value)"; fi
}
wait_for() {  # seconds command...
    local limit=$1; shift
    for _ in $(seq $((limit * 10))); do "$@" && return 0; sleep 0.1; done
    return 1
}

# The desktop background doubles as our test picture.
gsettings set org.gnome.desktop.background picture-uri "file://$pattern"
gsettings set org.gnome.desktop.background picture-uri-dark "file://$pattern"
gsettings set org.gnome.desktop.background picture-options stretched
gsettings set org.gnome.desktop.interface enable-animations false
gsettings set org.gnome.shell welcome-dialog-last-shown-version '999'

export WAYLAND_DISPLAY=wayland-e2e
gnome-shell --nested --wayland --wayland-display=$WAYLAND_DISPLAY >"$WORK/shell.log" 2>&1 &
wait_for 30 test -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" || { echo "gnome-shell did not start"; tail "$WORK/shell.log"; exit 1; }
sleep 4
# GNOME opens the overview at startup; close it.
xdotool key Escape
sleep 1

/usr/libexec/xdg-desktop-portal-gnome >"$WORK/xdp-gnome.log" 2>&1 &
/usr/libexec/xdg-desktop-portal -r >"$WORK/xdp.log" 2>&1 &
/usr/libexec/gsd-media-keys >"$WORK/gsd.log" 2>&1 &
sleep 2

shot baseline
ok "nested GNOME Shell is up"

export QT_QPA_PLATFORM=wayland
"$bin" --background >"$WORK/app.log" 2>&1 &
app=$!
sleep 2
kill -0 $app 2>/dev/null && ok "daemon started" || bad "daemon started"
grep -q "Wayland" "$WORK/app.log" && ok "runs as a native Wayland client" || bad "not on Wayland: $(head -3 "$WORK/app.log")"

bindings=$(gsettings get org.gnome.settings-daemon.plugins.media-keys custom-keybindings)
[[ "$bindings" == *"lupka-zoom"* ]] && ok "GNOME custom shortcuts registered" || bad "no custom shortcuts: $bindings"
binding=$(gsettings get org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/lupka-zoom/ binding)
[[ "$binding" == "'<Primary>1'" ]] && ok "zoom is bound to <Primary>1" || bad "zoom binding is $binding"

# First portal screenshot asks for permission once; answer "Allow".
xdotool mousemove 960 540
"$bin" zoom
sleep 3
shot permission-dialog
xdotool key Tab Return 2>/dev/null
sleep 3
shot zoom-1
if grep -q "captured" "$WORK/app.log"; then ok "screen captured ($(grep -o 'via "[^"]*"' "$WORK/app.log" | tail -1))"; else bad "screen not captured: $(tail -3 "$WORK/app.log")"; fi
expect_different "overlay shows a zoomed picture" "$WORK/zoom-1.png" "$WORK/baseline.png" 0.05

# Keyboard focus: Escape must reach the overlay.
xdotool key Escape
sleep 1.5
shot after-escape
expect_similar "Escape closes the zoom (overlay had keyboard focus)" "$WORK/after-escape.png" "$WORK/baseline.png" 0.03

# The real hotkey path: gnome-shell grabs Ctrl+1, gsd-media-keys runs "lupka zoom".
xdotool mousemove 960 540
xdotool key ctrl+1
sleep 3
shot hotkey-zoom
expect_different "Ctrl+1 through GNOME's shortcut zooms" "$WORK/hotkey-zoom.png" "$WORK/baseline.png" 0.05
xdotool click 1
sleep 0.4
xdotool mousemove 700 500 mousedown 1 mousemove 800 520 mousemove 900 540 mouseup 1
sleep 0.5
shot hotkey-draw
red=$(convert "$WORK/hotkey-draw.png" -fuzz 15% -fill black +opaque red -fill white -opaque red -format '%[fx:mean]' info:)
awk -v r="$red" 'BEGIN { exit !(r > 0.0005) }' && ok "click and drag draws in red ($red)" || bad "no red stroke ($red)"
xdotool key ctrl+1
sleep 1.5
shot hotkey-closed
expect_similar "Ctrl+1 again closes the zoom" "$WORK/hotkey-closed.png" "$WORK/baseline.png" 0.03

# Snip with Super+Shift+S; the clipboard must hold an image afterwards.
xdotool key super+shift+s
sleep 3
xdotool mousemove 200 200 mousedown 1 mousemove 350 300 mousemove 500 400 mouseup 1
sleep 1.5
if timeout 5 wl-paste --list-types 2>/dev/null | grep -q image/png; then
    timeout 5 wl-paste --type image/png >"$WORK/snip.png"
    size=$(identify -format '%wx%h' "$WORK/snip.png" 2>/dev/null)
    [[ "$size" == "300x200" ]] && ok "Super+Shift+S snips 300x200 to the Wayland clipboard" || bad "snip size '$size'"
else
    bad "no image on the Wayland clipboard after snip"
fi

"$bin" quit
sleep 1
bindings=$(gsettings get org.gnome.settings-daemon.plugins.media-keys custom-keybindings)
[[ "$bindings" != *"lupka"* ]] && ok "quit removes the custom shortcuts" || bad "shortcuts left behind: $bindings"

echo "---- $pass passed, $fail failed"
[[ $fail -eq 0 ]]
