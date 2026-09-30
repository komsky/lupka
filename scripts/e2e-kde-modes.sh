#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Komsky
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Multi-monitor and HiDPI checks, run by e2e-kde-wayland.sh (KDE_MODE=dual or
# hidpi) after it has started KWin. Uses its environment.
set -uo pipefail
WORK=/tmp/work
bin=/build/lupka
pattern=/src/scripts/testpattern.png
convert() { magick "$@"; }
compare() { magick compare "$@"; }
pass=0
fail=0
ok() { echo "PASS  $1"; pass=$((pass + 1)); }
bad() { echo "FAIL  $1"; fail=$((fail + 1)); }
shot() { timeout 15 /build/tests/kwinshot "$WORK/$1.png"; }
key() { DISPLAY=:0 xdotool "$@"; }
rmse() { compare -metric RMSE "$1" "$2" null: 2>&1 | sed -E 's/.*\((.*)\).*/\1/'; }
crop() { convert "$1" -crop "$2" +repage "$3"; }
below() { awk -v v="$1" -v t="$2" 'BEGIN { exit !(v < t) }'; }
above() { awk -v v="$1" -v t="$2" 'BEGIN { exit !(v >= t) }'; }

export QT_QPA_PLATFORM=wayland
case "$KDE_MODE" in
dual)
    # A picture on each monitor; KWin opens windows where the pointer is.
    key mousemove 960 540
    imv -f -s full "$pattern" >$WORK/imv1.log 2>&1 &
    sleep 2
    key mousemove 2880 540
    imv -f -s full "$pattern" >$WORK/imv2.log 2>&1 &
    sleep 2
    shot baseline
    size=$(magick identify -format '%wx%h' $WORK/baseline.png)
    [[ "$size" == 3840x1080 ]] && ok "two monitors side by side ($size)" || bad "workspace is $size"

    "$bin" --background >$WORK/app.log 2>&1 &
    sleep 3
    key mousemove 2880 540 key ctrl+1
    sleep 2.5
    shot zoom-right
    grep -q 'captured 2 screens' $WORK/app.log && ok "captured both monitors" || bad "capture: $(grep captur $WORK/app.log | tail -1)"
    crop $WORK/baseline.png 1920x1080+0+0 $WORK/left-before.png
    crop $WORK/zoom-right.png 1920x1080+0+0 $WORK/left-after.png
    crop $WORK/zoom-right.png 1920x1080+1920+0 $WORK/right-after.png
    convert "$pattern" -crop 960x540+480+270 +repage -resize '1920x1080!' $WORK/want-zoom.png
    v=$(rmse $WORK/left-after.png $WORK/left-before.png); below "$v" 0.02 && ok "the other monitor is left alone (rmse $v)" || bad "left monitor changed (rmse $v)"
    v=$(rmse $WORK/right-after.png $WORK/want-zoom.png); below "$v" 0.08 && ok "the monitor under the pointer zooms (rmse $v)" || bad "right monitor not zoomed (rmse $v)"
    key key Escape
    sleep 1.5
    shot after
    v=$(rmse $WORK/after.png $WORK/baseline.png); below "$v" 0.02 && ok "Escape closes it (rmse $v)" || bad "still open (rmse $v)"

    key mousemove 960 540 key ctrl+1
    sleep 2.5
    shot zoom-left
    crop $WORK/zoom-left.png 1920x1080+0+0 $WORK/left-zoom.png
    crop $WORK/zoom-left.png 1920x1080+1920+0 $WORK/right-plain.png
    crop $WORK/baseline.png 1920x1080+1920+0 $WORK/right-before.png
    v=$(rmse $WORK/left-zoom.png $WORK/want-zoom.png); below "$v" 0.08 && ok "and the left one when the pointer is there (rmse $v)" || bad "left monitor not zoomed (rmse $v)"
    v=$(rmse $WORK/right-plain.png $WORK/right-before.png); below "$v" 0.02 && ok "leaving the right one alone (rmse $v)" || bad "right monitor changed (rmse $v)"
    key key Escape
    sleep 1
    ;;
hidpi)
    # Xwayland's root window is in physical pixels here, twice the logical size.
    xs=$(DISPLAY=:0 xdpyinfo | awk '/dimensions/ { split($2, d, "x"); print d[1] / 960 }')
    at() { echo "$(awk -v v="$1" -v s="$xs" 'BEGIN { print int(v * s) }')"; }
    key mousemove "$(at 480)" "$(at 270)"
    imv -f -s full "$pattern" >$WORK/imv1.log 2>&1 &
    sleep 2
    shot baseline
    size=$(magick identify -format '%wx%h' $WORK/baseline.png)
    ok "scale-2 output, workspace screenshot $size, X11 scale $xs"
    "$bin" --background >$WORK/app.log 2>&1 &
    sleep 3
    key mousemove "$(at 480)" "$(at 270)" key ctrl+1
    sleep 2.5
    shot zoom
    grep -q 'captured 1 screens' $WORK/app.log && ok "captured" || bad "capture: $(grep captur $WORK/app.log | tail -1)"
    w=${size%x*}; h=${size#*x}
    convert "$pattern" -resize "${w}x${h}!" $WORK/pattern-native.png
    convert $WORK/pattern-native.png -crop "$((w / 2))x$((h / 2))+$((w / 4))+$((h / 4))" +repage -resize "${w}x${h}!" $WORK/want-zoom.png
    v=$(rmse $WORK/zoom.png $WORK/want-zoom.png); below "$v" 0.08 && ok "zoom is right at scale 2 (rmse $v)" || bad "zoom wrong at scale 2 (rmse $v)"
    # The native-resolution capture keeps small text sharp when zoomed.
    convert $WORK/zoom.png -colorspace Gray -define convolve:scale='!' -morphology Convolve Laplacian:0 -format '%[fx:standard_deviation]' info: >$WORK/sharpness.txt
    key click 1
    sleep 0.3
    key mousemove "$(at 300)" "$(at 200)" mousedown 1 mousemove "$(at 400)" "$(at 220)" mousemove "$(at 500)" "$(at 240)" mouseup 1
    sleep 0.5
    shot draw
    px=$((400 * w / 960)); py=$((220 * h / 540))
    rgb=$(convert $WORK/draw.png -format "%[fx:int(255*p{$px,$py}.r)],%[fx:int(255*p{$px,$py}.g)]" info:)
    [[ "${rgb%%,*}" -gt 200 && "${rgb##*,}" -lt 60 ]] && ok "drawing lands under the pointer at scale 2 ($rgb)" || bad "stroke not under the pointer ($rgb)"
    key key Escape
    sleep 1
    ;;
esac

"$bin" quit
cp $WORK/*.png $WORK/*.log /build/ 2>/dev/null
echo "---- $pass passed, $fail failed"
[[ $fail -eq 0 ]]
