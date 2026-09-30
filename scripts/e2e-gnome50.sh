#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Komsky
# SPDX-License-Identifier: GPL-3.0-or-later
#
# End-to-end test under GNOME 50 (the Ubuntu 26.04 generation) in a Fedora
# container: gnome-shell --devkit shown in Xvfb, input through Mutter's
# RemoteDesktop API (scripts/containers/rdinput.py).
#
#   podman build -t lupka-gnome -f scripts/containers/gnome.Containerfile scripts/containers
#   scripts/e2e-gnome50.sh
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)

if [[ "${E2E_INNER:-}" != 1 ]]; then
    build=${GNOME_BUILD_DIR:-$root/build-gnome}
    mkdir -p "$build"
    gpu=()
    [[ -e /dev/dri/renderD128 ]] && gpu=(--device /dev/dri)
    exec podman run --rm "${gpu[@]}" -e E2E_INNER=1 -v "$root:/src:ro,z" -v "$build:/build:z" --tmpfs /tmp:exec \
        lupka-gnome /src/scripts/e2e-gnome50.sh
fi

cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/tmp/cmake.log 2>&1 || { cat /tmp/cmake.log; exit 1; }
ninja -C /build >/tmp/ninja.log 2>&1 || { tail -30 /tmp/ninja.log; exit 1; }
bin=/build/lupka
pattern=/src/scripts/testpattern.png
WORK=/tmp/work
mkdir -p $WORK

convert() { magick "$@"; }
compare() { magick compare "$@"; }
identify() { magick identify "$@"; }
pass=0
fail=0
ok() { echo "PASS  $1"; pass=$((pass + 1)); }
bad() { echo "FAIL  $1"; fail=$((fail + 1)); }
rmse() { compare -metric RMSE "$1" "$2" null: 2>&1 | sed -E 's/.*\((.*)\).*/\1/'; }
expect_similar() {
    local v; v=$(rmse "$2" "$3")
    awk -v v="$v" -v t="${4:-0.05}" 'BEGIN { exit !(v < t) }' && ok "$1 (rmse $v)" || bad "$1 (rmse $v)"
}
expect_different() {
    local v; v=$(rmse "$2" "$3")
    awk -v v="$v" -v t="${4:-0.05}" 'BEGIN { exit !(v >= t) }' && ok "$1 (rmse $v)" || bad "$1 (rmse $v)"
}
wait_for() { local limit=$1; shift; for _ in $(seq $((limit * 10))); do "$@" && return 0; sleep 0.1; done; return 1; }
# Input goes through rdinput.py; the last pointer position is kept for shot().
input() {
    local commands
    commands=$(cat)
    printf '%s\n' "$commands" | python3 /src/scripts/containers/rdinput.py
    printf '%s\n' "$commands" | awk '$1 == "move" { p = $2 " " $3 } END { if (p != "") print p }' >>$WORK/pointer
}
# The devkit window shows the 1920x1080 monitor below its header bar. It
# displays a new compositor frame only when the next one arrives, so nudge the
# pointer (one pixel and back) to push the current frame through first.
shot() {
    local x=960 y=540
    [[ -s $WORK/pointer ]] && read -r x y < <(tail -1 $WORK/pointer)
    printf 'move %d %d\nsleep 0.1\nmove %d %d\n' $((x + 1)) "$y" "$x" "$y" | python3 /src/scripts/containers/rdinput.py
    sleep 0.3
    DISPLAY=:1 magick import -window root $WORK/raw.png
    convert $WORK/raw.png -crop 1920x1080+$left+$top +repage "$WORK/$1.png"
}

# System bus with a stand-in logind: gnome-shell will not start without one.
mkdir -p /run/dbus && dbus-daemon --system --fork
python3 -m dbusmock --system --template logind >$WORK/logind.log 2>&1 &
sleep 1

export LANG=C.UTF-8 HOME=/tmp/home XDG_RUNTIME_DIR=/tmp/run XDG_CONFIG_HOME=/tmp/home/.config
export XDG_CURRENT_DESKTOP=GNOME QT_FORCE_STDERR_LOGGING=1 QT_LOGGING_RULES="app.*=true"
mkdir -p "$HOME" "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"
# A session mode without the Overview, which GNOME otherwise opens at startup.
mkdir -p $WORK/data/gnome-shell/modes
printf '{ "parentMode": "user", "hasOverview": false }\n' >$WORK/data/gnome-shell/modes/lupka-test.json
export XDG_DATA_DIRS=$WORK/data:/usr/local/share:/usr/share
Xvfb :1 -screen 0 2000x1300x24 -nolisten tcp +extension GLX >$WORK/xvfb.log 2>&1 &
wait_for 10 xdpyinfo -display :1 >/dev/null 2>&1
export DBUS_SESSION_BUS_ADDRESS=$(dbus-daemon --session --fork --print-address=1 | head -1)
gsettings set org.gnome.desktop.background picture-uri "file://$pattern"
gsettings set org.gnome.desktop.background picture-uri-dark "file://$pattern"
gsettings set org.gnome.desktop.background picture-options stretched
gsettings set org.gnome.desktop.interface enable-animations false
gsettings set org.gnome.desktop.notifications show-banners false
pipewire >$WORK/pipewire.log 2>&1 &
wait_for 5 test -S $XDG_RUNTIME_DIR/pipewire-0
wireplumber >$WORK/wireplumber.log 2>&1 &

# The shell D-Bus-activates the portals as it starts, so they take their
# environment from the bus, not from us. The devkit viewer is a GTK client of
# Xvfb: export the Wayland variables here only after the shell has started, or
# the viewer connects to the shell it is meant to show.
dbus-update-activation-environment WAYLAND_DISPLAY=wayland-g XDG_SESSION_TYPE=wayland XDG_CURRENT_DESKTOP=GNOME
# GSK_RENDERER=cairo: the GL renderer only repaints damaged areas of the
# viewer under Xvfb, so screenshots come out torn.
DISPLAY=:1 GSK_RENDERER=cairo MUTTER_DEBUG_DUMMY_MODE_SPECS=1920x1080 \
    gnome-shell --devkit --mode=lupka-test --wayland-display=wayland-g >$WORK/shell.log 2>&1 &
wait_for 30 test -S $XDG_RUNTIME_DIR/wayland-g || { echo "gnome-shell did not start"; tail -20 $WORK/shell.log; exit 1; }
export WAYLAND_DISPLAY=wayland-g XDG_SESSION_TYPE=wayland
viewer=""
for _ in $(seq 60); do
    viewer=$(DISPLAY=:1 xdotool search --onlyvisible --name "Mutter Development Kit" 2>/dev/null | head -1)
    [[ -n "$viewer" ]] && break
    sleep 0.5
done
[[ -n "$viewer" ]] || { echo "the devkit viewer did not appear"; exit 1; }
sleep 3
# The devkit monitor takes the size of the viewer's content area, inside a
# small frame below the header bar. Size the viewer for a 1920x1080 monitor,
# then find where it landed: the shell's black top bar starts the content and
# the frame around it is light.
DISPLAY=:1 xdotool windowsize "$viewer" 1930 1136 windowmove "$viewer" 0 0
sleep 3
DISPLAY=:1 magick import -window root $WORK/raw.png
edge() {  # crop geometry, max r+g+b -> offset of the first pixel darker than that
    convert $WORK/raw.png -crop "$1" +repage -depth 8 txt:- |
        awk -F'[^0-9]+' -v max="$2" 'NR > 1 && $3 + $4 + $5 < max { print $1 + $2; exit }'
}
top=$(edge 1x400+300+0 120)
left=$(edge 400x1+0+$((top + 300)) 600)
[[ -n "$top" && -n "$left" ]] || { echo "could not find the monitor in the viewer"; exit 1; }
echo "INFO  monitor shown at +$left+$top"

# Reading a property activates the portal frontend and, through it, the backend.
wait_for 10 sh -c 'gdbus call --session --dest org.freedesktop.portal.Desktop --object-path /org/freedesktop/portal/desktop \
    --method org.freedesktop.DBus.Properties.Get org.freedesktop.portal.ScreenCast version >/dev/null 2>&1' ||
    echo "INFO  the ScreenCast portal did not answer"
/usr/libexec/gsd-media-keys >$WORK/gsd.log 2>&1 &
sleep 2
# Wait for a still screen.
printf 'move 960 540\n' | input
shot baseline
for _ in $(seq 20); do
    sleep 0.5
    shot settled
    v=$(rmse $WORK/settled.png $WORK/baseline.png)
    cp $WORK/settled.png $WORK/baseline.png
    awk -v v="$v" 'BEGIN { exit !(v < 0.002) }' && break
done
ok "GNOME Shell $(gnome-shell --version | awk '{print $3}') is up"

export QT_QPA_PLATFORM=wayland
"$bin" --background >$WORK/app.log 2>&1 &
app=$!
sleep 3
kill -0 $app 2>/dev/null && ok "daemon started" || bad "daemon started"
grep -o "capture: [^|]*| hotkeys: .*" $WORK/app.log | head -1 | sed 's/^/INFO  /'
binding=$(gsettings get org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/lupka-zoom/ binding)
[[ "$binding" == "'<Primary>1'" ]] && ok "GNOME custom shortcut <Primary>1 registered" || bad "binding is $binding"

# First capture: GNOME asks which screen to share. The Share button is the
# button-sized patch of accent blue; it only turns that colour once a monitor
# is selected, so click the monitor until it does.
share_button() {
    convert $WORK/share-dialog.png -fuzz 10% -fill '#ff0000' -opaque '#3584e4' -fill black +opaque '#ff0000' \
        -define connected-components:verbose=true -define connected-components:area-threshold=200 \
        -connected-components 8 null: |
        awk '/srgb\(255,0,0\)|srgb\(100%,0,0\)/ { split($2, g, /[x+]/); split($3, c, ",");
             if (g[1] >= 50 && g[1] <= 130 && g[2] >= 24 && g[2] <= 46) { printf "%d %d\n", c[1], c[2]; exit } }'
}
"$bin" zoom
sleep 3
answered=0
for _ in $(seq 6); do
    printf 'move 960 1070\n' | input
    sleep 0.6
    shot share-dialog
    button=$(share_button)
    if [[ -n "$button" ]]; then
        printf 'move %s\nclick\n' "$button" | input
        answered=1
        break
    fi
    printf 'move 960 640\nclick\n' | input
done
[[ $answered == 1 ]] || echo "INFO  Share Screen dialog not found where expected (see share-dialog.png)"
printf 'move 960 540\n' | input
sleep 3
shot zoom-1
grep -q 'captured 1 screens via "ScreenCast portal"' $WORK/app.log && ok "captured through the ScreenCast portal" ||
    bad "capture: $(grep -iE 'captur|fail' $WORK/app.log | tail -2)"
convert "$pattern" -crop 960x540+480+270 +repage -resize '1920x1080!' $WORK/want-zoom.png
expect_similar "zoomed 2x around the pointer" $WORK/zoom-1.png $WORK/want-zoom.png 0.1

printf 'key escape\n' | input
sleep 1.5
shot after-escape
expect_similar "Escape closes the zoom (overlay had keyboard focus)" $WORK/after-escape.png $WORK/baseline.png 0.04

printf 'move 960 540\nkey ctrl+1\n' | input
sleep 2.5
shot hotkey-zoom
expect_similar "Ctrl+1 through GNOME's custom shortcut zooms, no dialog" $WORK/hotkey-zoom.png $WORK/want-zoom.png 0.1
printf 'click\nsleep 0.3\nmove 700 500\ndown\nmove 800 520\nmove 900 540\nup\n' | input
sleep 0.5
shot draw
rgb=$(convert $WORK/draw.png -format '%[fx:int(255*p{800,520}.r)],%[fx:int(255*p{800,520}.g)],%[fx:int(255*p{800,520}.b)]' info:)
IFS=, read -r r g b <<<"$rgb"
(( r > 200 && g < 60 && b < 60 )) && ok "drawing works ($rgb)" || bad "no red stroke ($rgb)"
printf 'key ctrl+1\n' | input
sleep 1.5
shot closed
expect_similar "Ctrl+1 again closes" $WORK/closed.png $WORK/baseline.png 0.04

printf 'key super+shift+s\n' | input
sleep 2.5
printf 'move 200 200\ndown\nmove 350 300\nmove 500 400\nup\n' | input
sleep 1.5
if timeout 5 wl-paste --list-types 2>/dev/null | grep -q image/png; then
    timeout 5 wl-paste --type image/png >$WORK/snip.png
    size=$(identify -format '%wx%h' $WORK/snip.png 2>/dev/null)
    [[ "$size" == 300x200 ]] && ok "Super+Shift+S snips 300x200 to the clipboard" || bad "snip size '$size'"
else
    bad "no image on the clipboard after the snip"
fi

"$bin" quit
sleep 1
bindings=$(gsettings get org.gnome.settings-daemon.plugins.media-keys custom-keybindings)
[[ "$bindings" != *lupka* ]] && ok "quit removes the shortcuts" || bad "left behind: $bindings"

cp $WORK/*.png $WORK/*.log /build/ 2>/dev/null
echo "---- $pass passed, $fail failed"
[[ $fail -eq 0 ]]
