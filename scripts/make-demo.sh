#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Komsky
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Records the README demo (docs/images/demo.gif) and screenshots on a private
# X server, so nothing touches the running desktop. Needs Xvfb, xdotool, feh,
# ImageMagick, ffmpeg, python3-pygments and Google Chrome or Chromium.
#
#   scripts/make-demo.sh [path/to/lupka]
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
bin=$(realpath "${1:-$root/build/lupka}")
out=$root/docs/images

if [[ "${DEMO_INNER:-}" != 1 ]]; then
    work=$(mktemp -d /tmp/lupka-demo.XXXXXX)
    python3 "$here/demo/scene.py" "$work/scene.html"
    chrome=$(command -v google-chrome || command -v chromium || command -v chromium-browser)
    "$chrome" --headless=new --disable-gpu --hide-scrollbars --no-first-run --user-data-dir="$work/chrome" \
        --window-size=1920,1080 --screenshot="$work/scene.png" "file://$work/scene.html" >/dev/null 2>&1
    display=":$((90 + RANDOM % 9))"
    Xvfb "$display" -screen 0 1920x1080x24 -nolisten tcp >/dev/null 2>&1 &
    xvfb=$!
    trap 'kill $xvfb 2>/dev/null; wait $xvfb 2>/dev/null' EXIT
    for _ in $(seq 50); do xdpyinfo -display "$display" >/dev/null 2>&1 && break; sleep 0.1; done
    env -i HOME="$HOME" PATH="$PATH" USER="$USER" LANG=C.UTF-8 DISPLAY="$display" \
        XDG_CONFIG_HOME="$work/config" XDG_DATA_HOME="$work/data" XDG_CACHE_HOME="$work/cache" \
        XDG_RUNTIME_DIR="$work/run" XDG_CURRENT_DESKTOP=XFCE QT_QPA_PLATFORM=xcb \
        XCURSOR_THEME=Adwaita XCURSOR_SIZE=32 DEMO_INNER=1 WORK="$work" \
        dbus-run-session -- "$0" "$bin"
    echo "work files: $work"
    exit 0
fi

mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME/lupka" "$out" && chmod 700 "$XDG_RUNTIME_DIR"
cat >"$XDG_CONFIG_HOME/lupka/lupka.ini" <<EOF
[app]
firstRunDone=true
startAtLogin=false
trayIcon=false

[draw]
fontFamily=Ubuntu Sans
fontScale=14

[break]
background=1
minutes=5
EOF

# A themed arrow on the desktop instead of the X server's default cross.
cursor=/usr/share/icons/Adwaita/cursors/left_ptr
[[ -e $cursor ]] && xsetroot -xcf "$cursor" 32
feh -F -x -N -Z "$WORK/scene.png" &
sleep 1
"$bin" --background >"$WORK/app.log" 2>&1 &
sleep 2

still() { import -window root "$WORK/$1.png"; }
now() { date +%s.%N; }
caption() { printf '%s\t%s\n' "$(now)" "$*" >>"$WORK/captions.tsv"; }
glide() {  # x1 y1 x2 y2 seconds
    local steps; steps=$(awk -v s="$5" 'BEGIN { print int(s * 60) }')
    for i in $(seq 1 "$steps"); do
        xdotool mousemove "$(awk -v a="$1" -v b="$3" -v i="$i" -v n="$steps" 'BEGIN { t = i / n; t = t * t * (3 - 2 * t); print int(a + (b - a) * t) }')" \
            "$(awk -v a="$2" -v b="$4" -v i="$i" -v n="$steps" 'BEGIN { t = i / n; t = t * t * (3 - 2 * t); print int(a + (b - a) * t) }')"
        sleep 0.016
    done
}
drag() {  # x1 y1 x2 y2 seconds [modifier keys]
    [[ -n "${6:-}" ]] && xdotool keydown "$6"
    xdotool mousemove "$1" "$2" mousedown 1
    glide "$1" "$2" "$3" "$4" "$5"
    xdotool mouseup 1
    [[ -n "${6:-}" ]] && xdotool keyup "$6"
    sleep 0.2
}

# ---- the animated demo -------------------------------------------------------
xdotool mousemove 900 760
ffmpeg -loglevel info -f x11grab -draw_mouse 1 -framerate 30 -video_size 1920x1080 -i "$DISPLAY" \
    -c:v libx264rgb -preset ultrafast -crf 0 -y "$WORK/raw.mkv" 2>"$WORK/ffmpeg.log" &
ffmpeg_pid=$!
sleep 1.0
glide 900 760 640 600 0.8
sleep 0.4

caption "Ctrl+1    zoom in, the view follows the mouse"
xdotool key ctrl+1
sleep 0.8
glide 640 600 1150 760 1.4
glide 1150 760 640 600 1.2
sleep 0.3

caption "Click    freeze and draw"
xdotool click 1
sleep 0.8
caption "Tab + drag    ellipse"
drag 545 752 875 832 0.9 Tab
sleep 0.3
caption "Ctrl+Shift + drag    arrow"
drag 760 842 1120 985 0.9 ctrl+shift
sleep 0.3
caption "G,  Ctrl + drag    green rectangle"
xdotool key g
drag 140 843 928 889 0.9 ctrl
sleep 0.3
caption "R,  T    type text in red"
xdotool key r
xdotool mousemove 1135 950
sleep 0.2
xdotool key t
sleep 0.2
xdotool type --delay 70 "ZoomIt's formula"
sleep 0.6
xdotool key Escape
sleep 0.4
still zoom-draw
sleep 0.6
caption "Esc    back to the desktop"
xdotool key Escape
sleep 1.4

caption "Super+Shift+S    snip to the clipboard"
xdotool key super+shift+s
sleep 0.8
xdotool mousemove 1392 62 mousedown 1
glide 1392 62 1878 598 0.9
sleep 0.2
still snip
xdotool mouseup 1
sleep 1.2
caption ""
sleep 0.8
kill -INT $ffmpeg_pid
wait $ffmpeg_pid || true

# ---- stills --------------------------------------------------------------------
# Draw (Ctrl+2) on a whiteboard.
xdotool mousemove 960 540 key ctrl+2
sleep 1
xdotool key ctrl+w
sleep 0.4
xdotool key b
drag 260 330 760 690 0.3 ctrl
xdotool key r
drag 1010 510 800 510 0.3 ctrl+shift
xdotool key g
drag 1080 330 1640 700 0.3 Tab
xdotool key k
xdotool mousemove 320 460 key t
sleep 0.2
xdotool type --delay 20 "Overlay"
xdotool key Escape
xdotool mousemove 1190 470 key t
sleep 0.2
xdotool type --delay 20 "Your screen"
xdotool key Escape
xdotool key p
drag 300 800 1600 800 0.3 shift
sleep 0.5
still whiteboard
xdotool key Escape
sleep 1

# Break timer over the faded desktop.
xdotool key ctrl+3
sleep 1.5
still break
xdotool key Escape
sleep 1

# Settings window.
"$bin" settings
sleep 2
settings=$(xdotool search --onlyvisible --name "Lupka" | tail -1)
xdotool windowmove "$settings" 560 200
sleep 0.5
import -window "$settings" "$WORK/settings.png"
"$bin" quit
sleep 1

# ---- post-processing -------------------------------------------------------------
start=$(grep -oE 'start: [0-9.]+' "$WORK/ffmpeg.log" | head -1 | awk '{ print $2 }')
filters=""
font=/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf
n=0
mapfile -t rows <"$WORK/captions.tsv"
for i in "${!rows[@]}"; do
    IFS=$'\t' read -r at text <<<"${rows[$i]}"
    [[ -z "$text" ]] && continue
    next=${rows[$((i + 1))]:-}
    until=$(awk -v s="$start" -v t="${next%%$'\t'*}" 'BEGIN { print (t == "" ? 9999 : t - s) }')
    from=$(awk -v s="$start" -v t="$at" 'BEGIN { print t - s }')
    n=$((n + 1))
    printf '%s' "$text" >"$WORK/caption$n.txt"
    filters+="drawtext=fontfile=$font:textfile=$WORK/caption$n.txt:fontsize=40:fontcolor=white:"
    filters+="box=1:boxcolor=0x14161c@0.82:boxborderw=22:x=(w-text_w)/2:y=h-text_h-64:"
    filters+="enable='between(t,$from,$until)',"
done
ffmpeg -loglevel error -y -i "$WORK/raw.mkv" -filter_complex \
    "[0:v]${filters}fps=12,scale=960:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle" \
    "$out/demo.gif"
for shot in zoom-draw snip whiteboard break; do
    convert "$WORK/$shot.png" -resize 1600x -strip -define png:compression-level=9 "$out/$shot.png"
done
convert "$WORK/settings.png" -strip -define png:compression-level=9 "$out/settings.png"
ls -la "$out"
