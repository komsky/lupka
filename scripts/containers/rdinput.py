#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komsky
# SPDX-License-Identifier: GPL-3.0-or-later

"""Inject input into a GNOME (mutter) session through org.gnome.Mutter.RemoteDesktop.

Test helper for headless/devkit GNOME, where no real input device exists.
Reads commands from stdin, one per line:
    key <keysym-name>[+<keysym-name>...]   press the keys in order, release in reverse
    move <x> <y>                           move the pointer to (x, y) (absolute)
    down | up | click                      left button
    rclick                                 right button
    sleep <seconds>
"""
import sys
import time

import dbus

KEYSYMS = {
    "ctrl": 0xFFE3, "shift": 0xFFE1, "alt": 0xFFE9, "super": 0xFFEB,
    "escape": 0xFF1B, "return": 0xFF0D, "tab": 0xFF09, "up": 0xFF52, "down": 0xFF54,
}


def keysym(name):
    name = name.lower()
    if name in KEYSYMS:
        return KEYSYMS[name]
    if len(name) == 1:
        return ord(name)
    raise ValueError(name)


def main():
    bus = dbus.SessionBus()
    rd = dbus.Interface(bus.get_object("org.gnome.Mutter.RemoteDesktop", "/org/gnome/Mutter/RemoteDesktop"),
                        "org.gnome.Mutter.RemoteDesktop")
    session_path = rd.CreateSession()
    session = dbus.Interface(bus.get_object("org.gnome.Mutter.RemoteDesktop", session_path),
                             "org.gnome.Mutter.RemoteDesktop.Session")
    session_id = bus.get_object("org.gnome.Mutter.RemoteDesktop", session_path).Get(
        "org.gnome.Mutter.RemoteDesktop.Session", "SessionId", dbus_interface="org.freedesktop.DBus.Properties")

    # Absolute pointer motion needs a stream: record the monitor.
    sc = dbus.Interface(bus.get_object("org.gnome.Mutter.ScreenCast", "/org/gnome/Mutter/ScreenCast"),
                        "org.gnome.Mutter.ScreenCast")
    cast_path = sc.CreateSession({"remote-desktop-session-id": session_id})
    cast = dbus.Interface(bus.get_object("org.gnome.Mutter.ScreenCast", cast_path),
                          "org.gnome.Mutter.ScreenCast.Session")
    stream_path = cast.RecordMonitor("", {"cursor-mode": dbus.UInt32(1)})
    session.Start()
    time.sleep(0.5)

    for line in sys.stdin:
        parts = line.split()
        if not parts:
            continue
        cmd = parts[0]
        if cmd == "key":
            names = parts[1].split("+")
            for n in names:
                session.NotifyKeyboardKeysym(dbus.UInt32(keysym(n)), True)
            for n in reversed(names):
                session.NotifyKeyboardKeysym(dbus.UInt32(keysym(n)), False)
        elif cmd == "move":
            session.NotifyPointerMotionAbsolute(stream_path, float(parts[1]), float(parts[2]))
        elif cmd in ("down", "up", "click", "rclick"):
            button = 0x113 if cmd == "rclick" else 0x110  # BTN_RIGHT / BTN_LEFT
            if cmd in ("down", "click", "rclick"):
                session.NotifyPointerButton(button, True)
            if cmd in ("up", "click", "rclick"):
                session.NotifyPointerButton(button, False)
        elif cmd == "sleep":
            time.sleep(float(parts[1]))
        time.sleep(0.05)
    session.Stop()


if __name__ == "__main__":
    main()
