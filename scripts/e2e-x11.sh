#!/usr/bin/env bash
# End-to-end test on a virtual X server (Xvfb) with its own D-Bus session and
# config directories, so it never touches the real desktop or its settings.
#
#   scripts/e2e-x11.sh [path/to/binary] [--wm mutter]
#
# Drives the app with xdotool and checks what ends up on the screen.
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
bin=$(realpath "${1:-$root/build/lupka}")
wm=""
[[ "${2:-}" == "--wm" ]] && wm="${3:-}"

if [[ "${E2E_INNER:-}" != 1 ]]; then
    work=$(mktemp -d /tmp/lupka-e2e.XXXXXX)
    display=":$(( 90 + RANDOM % 9 ))"
    Xvfb "$display" -screen 0 1920x1080x24 -nolisten tcp >/dev/null 2>&1 &
    xvfb=$!
    trap 'kill $xvfb 2>/dev/null; wait $xvfb 2>/dev/null' EXIT
    for _ in $(seq 50); do xdpyinfo -display "$display" >/dev/null 2>&1 && break; sleep 0.1; done
    env -i HOME="$HOME" PATH="$PATH" USER="$USER" LANG=C.UTF-8 \
        DISPLAY="$display" XDG_CONFIG_HOME="$work/config" XDG_DATA_HOME="$work/data" \
        XDG_CACHE_HOME="$work/cache" XDG_RUNTIME_DIR="$work/run" XDG_CURRENT_DESKTOP=X-Test \
        QT_QPA_PLATFORM=xcb E2E_INNER=1 WORK="$work" \
        dbus-run-session -- "$0" "$bin" ${wm:+--wm "$wm"}
    status=$?
    echo "artifacts: $work"
    exit $status
fi

mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"
pattern="$here/testpattern.png"
pass=0
fail=0
ok() { echo "PASS  $1"; pass=$((pass + 1)); }
bad() { echo "FAIL  $1"; fail=$((fail + 1)); }
shot() { import -window root "$WORK/$1.png"; }

# Normalised RMSE between a screenshot and an expected image (0 = identical).
rmse() {
    compare -metric RMSE "$1" "$2" null: 2>&1 | sed -E 's/.*\((.*)\).*/\1/'
}
expect_similar() {  # name screenshot expected [threshold]
    local value
    value=$(rmse "$2" "$3")
    if awk -v v="$value" -v t="${4:-0.06}" 'BEGIN { exit !(v < t) }'; then
        ok "$1 (rmse $value)"
    else
        bad "$1 (rmse $value)"
    fi
}
pixel() {  # image x y -> "r,g,b"
    convert "$1" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info:
}
expect_pixel() {  # name image x y r g b
    local got
    got=$(pixel "$2" "$3" "$4")
    IFS=, read -r r g b <<<"$got"
    if (( ${r} >= $5 - 40 && ${r} <= $5 + 40 && ${g} >= $6 - 40 && ${g} <= $6 + 40 && ${b} >= $7 - 40 && ${b} <= $7 + 40 )); then
        ok "$1 ($got)"
    else
        bad "$1 (got $got, want $5,$6,$7)"
    fi
}
crop_zoom() {  # w h x y out  -> expected zoomed view
    convert "$pattern" -crop "$1x$2+$3+$4" +repage -resize '1920x1080!' "$WORK/$5.png"
}

# A fullscreen picture stands in for the desktop.
feh -F -x -N -Z --geometry 1920x1080+0+0 "$pattern" >/dev/null 2>&1 &
for _ in $(seq 100); do
    shot baseline
    awk -v v="$(rmse "$WORK/baseline.png" "$pattern")" 'BEGIN { exit !(v < 0.02) }' && break
    sleep 0.1
done
if [[ -n "$wm" ]]; then
    "$wm" --x11 --replace --sm-disable >"$WORK/wm.log" 2>&1 &
    sleep 3
fi
shot baseline
expect_similar "baseline shows the test pattern" "$WORK/baseline.png" "$pattern" 0.02

# Keep saved files inside the test directory.
mkdir -p "$XDG_CONFIG_HOME/lupka" "$WORK/videos" "$WORK/pictures"
cat >"$XDG_CONFIG_HOME/lupka/lupka.ini" <<INI
[record]
directory=$WORK/videos

[snip]
saveDirectory=$WORK/pictures
lastSaveDirectory=$WORK/pictures
INI

"$bin" --background >"$WORK/app.log" 2>&1 &
app=$!
sleep 2
kill -0 $app 2>/dev/null && ok "daemon started" || bad "daemon started"

# --- Zoom (Ctrl+1) ------------------------------------------------------------
xdotool mousemove 960 540
xdotool key ctrl+1
sleep 0.9
shot zoom-centre
crop_zoom 960 540 480 270 want-centre
expect_similar "Ctrl+1 zooms 2x around the pointer" "$WORK/zoom-centre.png" "$WORK/want-centre.png"

xdotool mousemove 0 0
sleep 0.4
shot zoom-topleft
crop_zoom 960 540 0 0 want-topleft
expect_similar "moving to the top-left corner pans there" "$WORK/zoom-topleft.png" "$WORK/want-topleft.png"

xdotool mousemove 1919 1079
sleep 0.4
shot zoom-bottomright
crop_zoom 960 540 960 540 want-bottomright
expect_similar "moving to the bottom-right corner pans there" "$WORK/zoom-bottomright.png" "$WORK/want-bottomright.png"

xdotool click 4
sleep 0.8
shot zoom-wheel
crop_zoom 480 270 1440 810 want-wheel
expect_similar "wheel up doubles the zoom to 4x" "$WORK/zoom-wheel.png" "$WORK/want-wheel.png"

xdotool key Down
sleep 0.6

# --- Draw on the zoomed screen ------------------------------------------------
xdotool mousemove 960 540
sleep 0.3
xdotool click 1
sleep 0.3
xdotool mousemove 400 300 mousedown 1 mousemove 500 300 mousemove 600 300 mousemove 700 300 mouseup 1
sleep 0.3
xdotool key g
xdotool keydown ctrl mousemove 900 600 mousedown 1 mousemove 1000 650 mousemove 1100 700 mouseup 1 keyup ctrl
sleep 0.4
shot draw-zoomed
expect_pixel "freehand stroke is red" "$WORK/draw-zoomed.png" 550 300 255 0 0
expect_pixel "G + Ctrl+drag draws a green rectangle" "$WORK/draw-zoomed.png" 1000 600 0 255 0
bg=$(pixel "$WORK/want-centre.png" 1000 650)
expect_pixel "rectangle is hollow" "$WORK/draw-zoomed.png" 1000 650 "${bg%%,*}" "$(cut -d, -f2 <<<"$bg")" "${bg##*,}"

xdotool key ctrl+z
sleep 0.3
shot draw-undo
bg=$(pixel "$WORK/want-centre.png" 1000 600)
expect_pixel "Ctrl+Z removes the rectangle" "$WORK/draw-undo.png" 1000 600 "${bg%%,*}" "$(cut -d, -f2 <<<"$bg")" "${bg##*,}"
expect_pixel "Ctrl+Z keeps the earlier stroke" "$WORK/draw-undo.png" 550 300 255 0 0

xdotool key ctrl+w
sleep 0.3
shot whiteboard
expect_pixel "Ctrl+W blanks to a whiteboard" "$WORK/whiteboard.png" 550 300 255 255 255
xdotool key ctrl+z
sleep 0.3

# Right-click leaves pen mode but keeps the drawing, which pans with the view.
xdotool click 3
sleep 0.3
shot pen-off
expect_pixel "right-click keeps the drawing" "$WORK/pen-off.png" 550 300 255 0 0
xdotool mousemove 0 0
sleep 0.4
shot pen-off-panned
expect_pixel "the drawing pans with the zoomed screen" "$WORK/pen-off-panned.png" 1510 840 255 0 0

xdotool key Escape
sleep 0.8
shot after-zoom
expect_similar "Esc zooms out and closes" "$WORK/after-zoom.png" "$pattern" 0.02

# --- Draw without zoom (Ctrl+2) -----------------------------------------------
xdotool key ctrl+2
sleep 0.8
xdotool key r
xdotool mousemove 300 800 mousedown 1 mousemove 400 800 mousemove 500 800 mouseup 1
xdotool key shift+y
xdotool mousemove 200 200 mousedown 1 mousemove 300 200 mousemove 400 200 mouseup 1
xdotool mousemove 1300 700 key t
xdotool type --delay 30 "Hi"
xdotool key Escape
sleep 0.3
shot draw-plain
expect_pixel "Ctrl+2 draws on the unzoomed screen" "$WORK/draw-plain.png" 450 800 255 0 0
expect_pixel "Shift+Y highlights like a marker" "$WORK/draw-plain.png" 300 200 58 95 128
convert "$WORK/draw-plain.png" -crop 300x140+1300+700 +repage "$WORK/text-area.png"
convert "$pattern" -crop 300x140+1300+700 +repage "$WORK/text-area-before.png"
text_changed=$(compare -metric AE "$WORK/text-area.png" "$WORK/text-area-before.png" null: 2>&1 | cut -d' ' -f1)
[[ "$text_changed" =~ ^[0-9]+$ && "$text_changed" -gt 500 ]] && ok "T types text ($text_changed px)" || bad "T typed nothing ($text_changed px)"
xdotool key e
sleep 0.2
shot draw-erased
expect_similar "E erases everything" "$WORK/draw-erased.png" "$pattern" 0.02
xdotool key ctrl+2
sleep 0.6
shot draw-closed
expect_similar "Ctrl+2 again closes the drawing" "$WORK/draw-closed.png" "$pattern" 0.02

# --- Snip while zoomed copies what is shown ------------------------------------
xdotool mousemove 960 540
xdotool key ctrl+1
sleep 0.9
xdotool key ctrl+6
sleep 0.3
xdotool mousemove 800 400 mousedown 1 mousemove 1000 500 mousemove 1120 640 mouseup 1
sleep 0.5
if xclip -selection clipboard -t image/png -o >"$WORK/zoom-snip.png" 2>/dev/null; then
    size=$(identify -format '%wx%h' "$WORK/zoom-snip.png" 2>/dev/null)
    [[ "$size" == "320x240" ]] && ok "snip while zoomed copies the selected 320x240" || bad "zoomed snip size is '$size'"
    convert "$WORK/want-centre.png" -crop 320x240+800+400 +repage "$WORK/want-zoom-snip.png"
    expect_similar "zoomed snip matches the zoomed view" "$WORK/zoom-snip.png" "$WORK/want-zoom-snip.png" 0.05
else
    bad "clipboard holds no image after a zoomed snip"
fi
xdotool mousemove 960 540
sleep 0.3
shot zoom-after-snip
expect_similar "zoom stays open after a snip" "$WORK/zoom-after-snip.png" "$WORK/want-centre.png" 0.06
xdotool key Escape
sleep 0.8

# --- Live draw (Ctrl+Shift+4) ---------------------------------------------------
xdotool key ctrl+shift+4
sleep 0.8
xdotool key r
xdotool mousemove 600 900 mousedown 1 mousemove 700 900 mousemove 800 900 mouseup 1
sleep 0.3
shot livedraw
expect_pixel "Ctrl+Shift+4 draws over the desktop" "$WORK/livedraw.png" 700 900 255 0 0
xdotool key Escape
sleep 0.6
shot livedraw-closed
expect_similar "Esc ends live draw" "$WORK/livedraw-closed.png" "$pattern" 0.02

# --- Snip (Super+Shift+S) -----------------------------------------------------
printf 'stale' | xclip -selection clipboard
xdotool key super+shift+s
sleep 0.9
xdotool mousemove 100 100 mousedown 1 mousemove 250 200 mousemove 400 300 mouseup 1
sleep 0.8
if xclip -selection clipboard -t image/png -o >"$WORK/snip.png" 2>/dev/null; then
    size=$(identify -format '%wx%h' "$WORK/snip.png" 2>/dev/null)
    [[ "$size" == "300x200" ]] && ok "snip puts a 300x200 PNG on the clipboard" || bad "snip size is '$size'"
    convert "$pattern" -crop 300x200+100+100 +repage "$WORK/want-snip.png"
    expect_similar "snip matches the selected area" "$WORK/snip.png" "$WORK/want-snip.png" 0.02
else
    bad "clipboard holds no image after snip"
fi
shot after-snip
expect_similar "overlay is gone after the snip" "$WORK/after-snip.png" "$pattern" 0.02

# --- Command line and break timer ---------------------------------------------
xdotool mousemove 960 540
"$bin" zoom
sleep 0.9
shot cli-zoom
expect_similar "'lupka zoom' from the command line zooms" "$WORK/cli-zoom.png" "$WORK/want-centre.png" 0.08
"$bin" zoom
sleep 0.8
shot cli-zoom-off
expect_similar "'lupka zoom' again closes the zoom" "$WORK/cli-zoom-off.png" "$pattern" 0.02

xdotool key ctrl+3
sleep 0.8
shot break
brightness=$(convert "$WORK/break.png" -colorspace Gray -format '%[fx:mean]' info:)
awk -v b="$brightness" 'BEGIN { exit !(b > 0.85) }' && ok "Ctrl+3 shows the white break screen ($brightness)" || bad "break timer not shown ($brightness)"
red=$(convert "$WORK/break.png" -fuzz 20% -fill black +opaque red -fill white -opaque red -format '%[fx:mean]' info:)
awk -v r="$red" 'BEGIN { exit !(r > 0.005) }' && ok "the countdown is drawn in red ($red)" || bad "no red countdown ($red)"
xdotool key Escape
sleep 0.5
shot break-closed
expect_similar "Esc closes the break timer" "$WORK/break-closed.png" "$pattern" 0.02

# --- Recording (Ctrl+5, Ctrl+Shift+5) ---------------------------------------------
media_info() {  # file -> "duration width height"
    ffprobe -v error -select_streams v:0 -show_entries format=duration:stream=width,height -of csv=p=0 "$1" |
        tr '\n' ',' | awk -F, '{ print $3, $1, $2 }'
}
xdotool key ctrl+5
sleep 3
xdotool key ctrl+5
sleep 3
video=$(ls "$WORK"/videos/*.mp4 "$WORK"/videos/*.webm 2>/dev/null | head -1)
if [[ -n "$video" ]]; then
    read -r duration width height <<<"$(media_info "$video")"
    awk -v d="$duration" 'BEGIN { exit !(d > 2.3 && d < 6) }' && ok "Ctrl+5 records ${duration}s" || bad "recording lasted '$duration'"
    [[ "$width" == 1920 && "$height" == 1080 ]] && ok "recording is 1920x1080" || bad "recording is ${width}x${height}"
else
    bad "no recording in $WORK/videos"
fi

xdotool key ctrl+shift+5
sleep 1
xdotool mousemove 100 100 mousedown 1 mousemove 300 250 mousemove 500 400 mouseup 1
sleep 3
xdotool key ctrl+5
sleep 3
region=$(ls -t "$WORK"/videos/*.mp4 "$WORK"/videos/*.webm 2>/dev/null | head -1)
if [[ -n "$region" && "$region" != "$video" ]]; then
    read -r duration width height <<<"$(media_info "$region")"
    [[ "$width" == 400 && "$height" == 300 ]] && ok "Ctrl+Shift+5 records the 400x300 region (${duration}s)" || bad "region recording is ${width}x${height}"
else
    bad "no region recording"
fi

"$bin" quit
sleep 1
kill -0 $app 2>/dev/null && bad "quit stops the daemon" || ok "quit stops the daemon"

echo "---- $pass passed, $fail failed"
[[ $fail -eq 0 ]]
