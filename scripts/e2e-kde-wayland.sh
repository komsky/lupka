#!/usr/bin/env bash
# End-to-end test under KDE Plasma 6's KWin (Wayland), nested in Xvfb, inside
# a Fedora container (scripts/containers/kde.Containerfile). Builds the app in
# the container, so it also checks that the code builds against current Qt.
#
#   podman build -t lupka-kde -f scripts/containers/kde.Containerfile scripts/containers
#   scripts/e2e-kde-wayland.sh
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)

if [[ "${E2E_INNER:-}" != 1 ]]; then
    build=${KDE_BUILD_DIR:-$root/build-kde}
    mkdir -p "$build"
    # KWin only takes screenshots with OpenGL compositing, which needs a GPU.
    gpu=()
    [[ -e /dev/dri/renderD128 ]] && gpu=(--device /dev/dri)
    exec podman run --rm "${gpu[@]}" -e E2E_INNER=1 -e KDE_MODE="${KDE_MODE:-single}" -v "$root:/src:ro,z" -v "$build:/build:z" --tmpfs /tmp:exec \
        lupka-kde /src/scripts/e2e-kde-wayland.sh
fi

# ---- inside the container ----------------------------------------------------
cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/tmp/cmake.log 2>&1 || { cat /tmp/cmake.log; exit 1; }
ninja -C /build >/tmp/ninja.log 2>&1 || { tail -30 /tmp/ninja.log; exit 1; }
bin=/build/lupka
pattern=/src/scripts/testpattern.png

export HOME=/tmp/home XDG_RUNTIME_DIR=/tmp/run XDG_CONFIG_HOME=/tmp/home/.config XDG_DATA_HOME=/tmp/home/.local/share
export XDG_CURRENT_DESKTOP=KDE XDG_SESSION_TYPE=wayland KDE_FULL_SESSION=true LANG=C.UTF-8
export QT_FORCE_STDERR_LOGGING=1 QT_LOGGING_RULES="app.*=true"
mkdir -p "$HOME" "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"
WORK=/tmp/work
mkdir -p $WORK

# ImageMagick 7 wants "magick <tool>" and warns about the old names.
if command -v magick >/dev/null; then
    convert() { magick "$@"; }
    compare() { magick compare "$@"; }
    identify() { magick identify "$@"; }
    import() { magick import "$@"; }
fi

pass=0
fail=0
ok() { echo "PASS  $1"; pass=$((pass + 1)); }
bad() { echo "FAIL  $1"; fail=$((fail + 1)); }
shot() { timeout 15 /build/tests/kwinshot "$WORK/$1.png"; }
rmse() { compare -metric RMSE "$1" "$2" null: 2>&1 | sed -E 's/.*\((.*)\).*/\1/'; }
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
wait_for() {
    local limit=$1; shift
    for _ in $(seq $((limit * 10))); do "$@" && return 0; sleep 0.1; done
    return 1
}
key() { DISPLAY=:0 xdotool "$@"; }

# KWin's virtual backend renders with EGL on the GPU, like a real session
# (KWin only takes screenshots with OpenGL compositing). Input goes in
# through Xwayland's XTEST, which KWin forwards via EIS; pictures come out
# through ScreenShot2.
mkdir -p /tmp/.X11-unix && chmod 1777 /tmp/.X11-unix
export DBUS_SESSION_BUS_ADDRESS=$(dbus-daemon --session --fork --print-address=1 | head -1)
pipewire >$WORK/pipewire.log 2>&1 &
wait_for 5 test -S $XDG_RUNTIME_DIR/pipewire-0
wireplumber >$WORK/wireplumber.log 2>&1 &

# Let xdotool's XTEST input through without KWin's "control input devices?" prompt.
mkdir -p "$XDG_CONFIG_HOME"
printf '[Xwayland]\nXwaylandEisNoPrompt=true\n' >"$XDG_CONFIG_HOME/kwinrc"

export WAYLAND_DISPLAY=wayland-0
# KDE_MODE: single (one 1920x1080 output), dual (two, side by side), hidpi (scale 2)
case "${KDE_MODE:-single}" in
    dual) outputs=(--output-count 2 --width 1920 --height 1080) ;;
    hidpi) outputs=(--width 1920 --height 1080 --scale 2) ;;
    *) outputs=(--width 1920 --height 1080) ;;
esac
KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1 kwin_wayland --virtual "${outputs[@]}" --no-lockscreen \
    --xwayland >$WORK/kwin.log 2>&1 &
wait_for 30 test -S $XDG_RUNTIME_DIR/$WAYLAND_DISPLAY || { echo "kwin did not start"; tail -20 $WORK/kwin.log; exit 1; }
wait_for 10 test -S /tmp/.X11-unix/X0
sleep 2
/usr/libexec/xdg-desktop-portal-kde >$WORK/xdp-kde.log 2>&1 &
sleep 1
/usr/libexec/xdg-desktop-portal -r >$WORK/xdp.log 2>&1 &

if [[ "${KDE_MODE:-single}" != single ]]; then
    exec /src/scripts/e2e-kde-modes.sh
fi

# A fullscreen picture stands in for the desktop.
QT_QPA_PLATFORM=wayland imv -f -s full "$pattern" >$WORK/imv.log 2>&1 &
sleep 3
key mousemove 960 540
shot baseline
ok "KWin $(kwin_wayland --version 2>/dev/null | awk '{print $2}') is up"
dbus-send --session --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.NameHasOwner string:org.kde.kglobalaccel |
    grep -q true && ok "kglobalaccel is running" || bad "kglobalaccel is not on the bus"

export QT_QPA_PLATFORM=wayland
"$bin" --background >$WORK/app.log 2>&1 &
app=$!
sleep 3
kill -0 $app 2>/dev/null && ok "daemon started" || bad "daemon started"
grep -o "capture: [^|]*| hotkeys: .*" $WORK/app.log | head -1 | sed 's/^/INFO  /'
grep -q "hotkeys: KDE global shortcuts" $WORK/app.log && ok "uses kglobalaccel" || bad "hotkeys backend: $(grep hotkeys $WORK/app.log)"

# Ctrl+1 through kglobalaccel, capture through KWin ScreenShot2.
key key ctrl+1
sleep 2
shot zoom
grep -q 'captured 1 screens via "KWin ScreenShot2"' $WORK/app.log && ok "KWin ScreenShot2 captured the screen" ||
    bad "capture: $(grep -i -E 'captur|fail' $WORK/app.log | tail -3)"
expect_different "Ctrl+1 zooms" "$WORK/zoom.png" "$WORK/baseline.png" 0.05
convert "$pattern" -crop 960x540+480+270 +repage -resize '1920x1080!' $WORK/want-zoom.png
expect_similar "zoomed view matches 2x around the pointer" "$WORK/zoom.png" "$WORK/want-zoom.png" 0.08

key click 1
sleep 0.3
key mousemove 700 500 mousedown 1 mousemove 800 520 mousemove 900 540 mouseup 1
sleep 0.5
shot draw
rgb=$(convert $WORK/draw.png -format '%[fx:int(255*p{800,520}.r)],%[fx:int(255*p{800,520}.g)],%[fx:int(255*p{800,520}.b)]' info:)
IFS=, read -r r g b <<<"$rgb"
(( r > 200 && g < 60 && b < 60 )) && ok "click and drag draws in red ($rgb)" || bad "no red stroke at the drag ($rgb)"

key key Escape
sleep 1.5
shot after-escape
expect_similar "Escape closes the zoom (overlay had keyboard focus)" "$WORK/after-escape.png" "$WORK/baseline.png" 0.03

key key super+shift+s
sleep 2
key mousemove 200 200 mousedown 1 mousemove 350 300 mousemove 500 400 mouseup 1
sleep 1.5
if timeout 5 wl-paste --list-types 2>/dev/null | grep -q image/png; then
    timeout 5 wl-paste --type image/png >$WORK/snip.png
    size=$(identify -format '%wx%h' $WORK/snip.png 2>/dev/null)
    [[ "$size" == "300x200" ]] && ok "Meta+Shift+S snips 300x200 to the clipboard" || bad "snip size '$size'"
    convert "$pattern" -crop 300x200+200+200 +repage $WORK/want-snip.png
    expect_similar "snip matches the screen" $WORK/snip.png $WORK/want-snip.png 0.03
else
    bad "no image on the clipboard after snip"
fi

key key ctrl+2
sleep 1.5
key mousemove 300 800 mousedown 1 mousemove 500 800 mouseup 1
sleep 0.3
key key ctrl+2
sleep 1
shot draw-closed
expect_similar "Ctrl+2 draws and Ctrl+2 again closes" $WORK/draw-closed.png $WORK/baseline.png 0.03

"$bin" quit
sleep 1
kill -0 $app 2>/dev/null && bad "quit stops the daemon" || ok "quit stops the daemon"

cp $WORK/*.png $WORK/*.log /build/ 2>/dev/null
echo "---- $pass passed, $fail failed"
[[ $fail -eq 0 ]]
