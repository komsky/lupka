#!/usr/bin/env bash
# Start a nested GNOME Shell (Wayland) inside Xvfb with its own D-Bus session,
# dconf, portals and media-keys daemon, and keep it running. Writes the
# environment to $WORK/env so commands can be run inside it:
#
#   scripts/nested-gnome.sh /tmp/gnome-dev &
#   env $(cat /tmp/gnome-dev/env) build/lupka zoom
#   DISPLAY=$(grep DISPLAY /tmp/gnome-dev/env | cut -d= -f2) import -window root shot.png
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=${1:?usage: nested-gnome.sh WORKDIR}
mkdir -p "$work/run" "$work/config" "$work/data" "$work/cache"
chmod 700 "$work/run"
display=":$(( 60 + RANDOM % 9 ))"

Xvfb "$display" -screen 0 1920x1080x24 -nolisten tcp +extension GLX >"$work/xvfb.log" 2>&1 &
pids=($!)
cleanup() { kill "${pids[@]}" 2>/dev/null; }
trap cleanup EXIT INT TERM
for _ in $(seq 50); do xdpyinfo -display "$display" >/dev/null 2>&1 && break; sleep 0.1; done

base=(HOME="$HOME" PATH="$PATH" USER="$USER" LANG=C.UTF-8 SHELL=/bin/bash DISPLAY="$display"
      XDG_CONFIG_HOME="$work/config" XDG_DATA_HOME="$work/data" XDG_CACHE_HOME="$work/cache"
      XDG_RUNTIME_DIR="$work/run" XDG_CURRENT_DESKTOP=GNOME XDG_SESSION_TYPE=wayland
      XDG_DATA_DIRS=/usr/local/share:/usr/share MUTTER_DEBUG_DUMMY_MODE_SPECS=1920x1080)
bus=$(env -i "${base[@]}" dbus-daemon --session --fork --print-address=1 --print-pid=1 | head -2)
address=$(sed -n 1p <<<"$bus")
pids+=("$(sed -n 2p <<<"$bus")")
base+=(DBUS_SESSION_BUS_ADDRESS="$address" WAYLAND_DISPLAY=wayland-nested)
printf '%s\n' "${base[@]}" >"$work/env"

run() { env -i "${base[@]}" "$@"; }
run gsettings set org.gnome.desktop.background picture-uri "file://$here/testpattern.png"
run gsettings set org.gnome.desktop.background picture-uri-dark "file://$here/testpattern.png"
run gsettings set org.gnome.desktop.background picture-options stretched
run gsettings set org.gnome.desktop.interface enable-animations false
run gsettings set org.gnome.shell welcome-dialog-last-shown-version 999

run gnome-shell --nested --wayland --wayland-display=wayland-nested >"$work/shell.log" 2>&1 &
pids+=($!)
for _ in $(seq 300); do [[ -S "$work/run/wayland-nested" ]] && break; sleep 0.1; done
sleep 4
DISPLAY=$display xdotool key Escape  # leave the startup overview
run /usr/libexec/xdg-desktop-portal-gnome >"$work/xdp-gnome.log" 2>&1 &
pids+=($!)
run /usr/libexec/xdg-desktop-portal -r >"$work/xdp.log" 2>&1 &
pids+=($!)
run /usr/libexec/gsd-media-keys >"$work/gsd.log" 2>&1 &
pids+=($!)
sleep 2
echo ready >"$work/ready"
wait
