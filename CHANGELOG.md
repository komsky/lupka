# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Releases ship a package for Ubuntu (24.04 and newer) and a separate one for Debian (13 and newer). Debian cannot install the Ubuntu package because the two distributions name the Qt libraries differently. The 0.1.0 release has both packages too.

## [0.1.0] - 2026-09-30

First release.

### Added

- Zoom (Ctrl+1): freezes the screen and zooms in around the pointer. The 2x steps, the proportional panning with an edge margin, the 1x to 256x range and the animation timings follow ZoomIt's source.
- Draw on the frozen screen, either from a zoom (left-click) or straight at 1:1 (Ctrl+2). Pens in eight colours, a marker-style highlighter, blur and strong blur pens, straight lines, rectangles, ellipses and arrows (chosen with modifier keys, as in ZoomIt), left or right aligned text, pen width, erase all, 32 steps of undo, and whiteboard and blackboard backgrounds. Drawings stay in place when you zoom or pan afterwards.
- Copy or save the drawn view, or a region of it, as PNG or JPEG.
- LiveDraw (Ctrl+Shift+4): draw over the live desktop without freezing it.
- Snip (Ctrl+6, Super+Shift+S): drag a rectangle and the image goes to the clipboard. Snip to file (Ctrl+Shift+6) asks where to save it. While a zoom is active, a snip crops what is on screen at the current magnification.
- Break timer (Ctrl+3): a full-screen countdown, adjusted with the arrow keys or the wheel.
- Live zoom (Ctrl+4): toggles the magnifier built into GNOME or KWin.
- Screen recording to MP4 (H.264) or WebM (VP8), with system audio when a sound server is available. Record a whole monitor (Ctrl+5), a region (Ctrl+Shift+5) or a window (Ctrl+Alt+5). The tray icon shows a red dot while recording.
- DemoType (Ctrl+7, Ctrl+Shift+7 to step back): types the next snippet of a ZoomIt-format script into the focused window, with `[end]`, `[pause:n]`, `[enter]`, arrow keys and `[paste]` blocks, scripts taken from the clipboard when they start with `[start]`, and a speed setting.
- Settings window and tray icon. Every hotkey can be changed or turned off.
- Support for GNOME and KDE Plasma under Wayland, for X11, and for other Wayland compositors, with a capture backend and a hotkey backend for each:
  - GNOME: custom shortcuts, and screen capture through the ScreenCast portal (no flash or shutter sound), falling back to the Screenshot portal.
  - KDE Plasma: global shortcuts through kglobalaccel, and screen capture through KWin's screenshot interface.
  - X11: key grabs (or the GNOME and KDE shortcut services where they exist), and capture from the X server.
  - Other Wayland compositors: the GlobalShortcuts portal, and capture with `grim` or the Screenshot portal.
- Command line: `lupka <action>` sends an action to the running instance, plus `--background`, `--unregister-shortcuts`, `--version` and `--help`. The running instance exposes `io.github.komsky.Lupka` on the session D-Bus.
- Debian package (`cpack -G DEB`) with desktop entry, AppStream metadata and icons.
- Unit tests, and end-to-end suites for X11 (Xvfb), a nested GNOME 46 session, and KDE Plasma 6 and GNOME 50 in Fedora containers.

[Unreleased]: https://github.com/komsky/lupka/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/komsky/lupka/releases/tag/v0.1.0
