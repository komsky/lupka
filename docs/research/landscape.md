# Landscape: open-source Linux tools that overlap with ZoomIt

Research date: 2026-09-30. Star counts, dates and licences come from the GitHub API (or Codeberg API) on that day. Everything else comes from each project's README and source, fetched the same day. Nothing here was installed or run, so behaviour claims are from documentation and code, and "not found" means I did not find it in the README or source, not that it is proven absent.

Scope: a Linux ZoomIt clone that has to work on Ubuntu, GNOME, KDE Plasma, X11 and Wayland. ZoomIt means static zoom, live zoom, draw, text, highlighter, blur, snip, break timer, recording and DemoType (see `zoomit-reference.md`).

Context that shapes everything below:
- GNOME 50 (March 2026) removed the X11 session, and Ubuntu 26.04 LTS ships it Wayland-only. KDE Plasma 6.8 (October 2026) drops its X11 session too. X11-only tools now only run through XWayland on current desktops. Sources: https://www.theregister.com/software/2026/03/19/gnome-50-debuts-with-x11-axed-wayland-front-and-center/5224877 and https://www.omgubuntu.co.uk/2025/11/kde-plasma-6-8-no-x11-wayland-only
- Mutter (GNOME) implements neither `wlr-layer-shell` nor `wlr-screencopy` nor `ext-image-copy-capture`. KWin implements layer-shell but neither screencopy protocol. Sway and COSMIC implement `ext-image-copy-capture`. Source: wayland.app protocol pages, https://wayland.app/protocols/wlr-layer-shell-unstable-v1, https://wayland.app/protocols/wlr-screencopy-unstable-v1, https://wayland.app/protocols/ext-image-copy-capture-v1 (versions shown there: Mutter 51, KWin 6.7, Sway 1.11, Hyprland 0.52.1, niri 26.04).

## 1. Comparison tables

Legend for status: Active = commit in the last 3 months. Slow = 3 to 12 months. Stale = over 12 months. Archived = GitHub archive flag. "Last commit" is the newest commit on the default branch.

### 1.1 Identity and health

| # | Tool | Repo | Licence | Language / toolkit | Stars | Last commit | Latest release | Status |
|---|---|---|---|---|---|---|---|---|
| 1 | wayscriber | github.com/devmobasa/wayscriber | MIT | Rust, smithay-client-toolkit, Cairo/Pango, optional GTK4 toolbars | 751 | 2026-09-29 | v0.9.25 (2026-09-10) | Active (created 2025-10-13) |
| 2 | Gromit-MPX | github.com/bk138/gromit-mpx | GPL-2.0 | C, GTK3, Cairo, XInput2 | 1,398 | 2026-09-01 | 1.9.0 (2026-03-18) | Active |
| 3 | Ardesia | github.com/pilollipietro/ardesia (fork: gfreeau/ardesia, 37 stars, 2020) | GPL-3.0 | C, GTK3, Cairo | 2 | 2026-03-05 (comment only) | v2.0 (2022-08-26) | Low activity |
| 4 | boomer | github.com/tsoding/boomer | MIT | Nim, Xlib, GLX/OpenGL | 852 | 2026-05-24 | none | Slow |
| 5 | woomer | github.com/coffeeispower/woomer | MIT | Rust, raylib (GLFW), libwayshot | 272 | 2026-06-28 | 0.2.0 (2025-07-07) | Slow |
| 6 | wooz | github.com/negrel/wooz | MIT | C, raw Wayland protocols, meson | 119 | 2025-12-27 | v0.1.0 (2025-07-09) | Archived |
| 7 | zooma | github.com/phreshbrread/zooma | MIT | Rust, GLFW | 0 | 2026-09-18 | none | New (created 2026-08-16) |
| 8 | Draw On Gnome | github.com/daveprowse/Draw-On-Gnome | GPL-3.0 | GJS, St, Clutter, Cairo | 156 | 2026-09-08 | v11.1 (2026-05-14) | Active |
| 9 | Draw On Your Screen 2 | github.com/zhrexl/DrawOnYourScreen2 | GPL-3.0 | GJS | 251 | 2024-06-18 | v12 (2022-03-26) | Archived 2024-07-27 |
| 9b | Draw On Your Screen (original, abakkk) | codeberg.org/som/DrawOnYourScreen | GPL-3.0 (per forks) | GJS | 8 (Codeberg) | 2024-12 (Codeberg updated_at) | n/a | Unmaintained; the extensions.gnome.org page points to Draw On Gnome |
| 10 | KWin Zoom effect | github.com/KDE/kwin, `src/plugins/zoom/` | GPL-2.0-or-later | C++, Qt, OpenGL (in the compositor) | 696 (KWin mirror) | 2026-09-30 | tag v6.7.91 | Active |
| 11 | KWin Mouse Mark effect | same repo, `src/plugins/mousemark/` | GPL-2.0-or-later | C++, Qt | (same) | (same) | (same) | Active |
| 12 | GNOME Magnifier | gitlab.gnome.org/GNOME/gnome-shell, `js/ui/magnifier.js` | GPL-2.0-or-later | GJS, Clutter | 966 (mirror) | 2026-09-30 | GNOME 50 | Active |
| 13 | Better Desktop Zoom | github.com/popov895/better-desktop-zoom | MIT | GJS | 12 | 2026-04-08 | v5.0 (2026-04-08) | Slow |
| 14 | KMag | github.com/KDE/kmag | GPL-2.0-or-later | C++, Qt | 20 (mirror) | 2026-09-29 | KDE Gear | Maintained, but no output on Wayland |
| 15 | Magnus | github.com/stuartlangridge/magnus | MIT | Python, GTK3, libkeybinder | 49 | 2024-10-14 | 1.0.3 (2019-09-22) | Stale |
| 16 | xzoom (fork) / xm | github.com/ToyKeeper/xzoom, github.com/c-blake/xm | xzoom: no licence file found; xm: MIT | C, Xlib+XShm / Nim | 1 / 6 | 2026-01-09 / 2026-07-21 | xzoom 0.3 (1990s) | Legacy / small |
| 17 | Flameshot | github.com/flameshot-org/flameshot | GPL-3.0 | C++, Qt | 31,026 | 2026-09-17 | v14.0.0 (2026-06-19) | Active |
| 18 | ksnip | github.com/ksnip/ksnip | GPL-3.0 | C++, Qt, kImageAnnotator | 3,341 | 2026-09-07 | v1.10.1 (2023-03-15) | Commits active, last release old |
| 19 | Satty | github.com/Satty-org/Satty | MPL-2.0 | Rust, GTK4, OpenGL | 2,427 | 2026-09-29 | v0.22.0 (2026-08-03) | Active |
| 20 | swappy | github.com/jtheoof/swappy | MIT | C, GTK3, Cairo | 1,506 | 2025-12-16 | v1.8.0 (2025-08-27) | Slow |
| 21 | Spectacle | github.com/KDE/spectacle | mixed GPL-2.0-or-later, LGPL, BSD-3 (per file) | C++, Qt, QML | 405 (mirror) | 2026-09-29 | KDE Gear | Active |
| 22 | Pensela | github.com/weiameili/Pensela | ISC | Electron | 545 | 2022-08-11 | v1.2.5 (2021-12-21) | Archived |
| 23 | DrawPen | github.com/DmytroVasin/DrawPen | MIT | Electron | 1,035 | 2026-09-26 | v0.0.58 (2026-09-26) | Active |
| 24 | ScreenPen | github.com/rsusik/screenpen | GPL-3.0 | Python, PyQt5, matplotlib | 134 | 2025-01-12 | PyPI | Stale |
| 25 | Zoomix | github.com/JohnMThompson/Zoomix | MIT | Rust, GTK3, Xlib | 2 | 2026-07-04 | v0.2.2 (2026-07-04) | New (created 2026-06-23) |
| 26 | Anatomico | github.com/charly-vibes/canticos, `apps/anatomico` | MIT | TypeScript compiled to GJS | 0 | 2026-09-24 | none | New; README says all code is LLM-generated |
| 27 | MagicScribe | github.com/Cartaz/Magicscribe | MIT | Python, PySide6, Qt Quick, layer-shell-qt | 0 | 2026-09-10 | none | New (created 2026-07-23) |
| 28 | wayland-screen-annotator | github.com/ukewea/wayland-screen-annotator | none stated | Rust, GTK4, gtk4-layer-shell | 1 | 2025-11-09 | none | Abandoned after day one |
| 29 | presenter-overlay | github.com/andreas-bylund/presenter-overlay | MIT | QML (Omarchy shell plugin) | 1 | 2026-08-19 | none | New |
| 30 | hati | github.com/szymonwilczek/hati | GPL-3.0 | GJS | 22 | 2026-06-01 | 1.2.2 (2026-01-31) | Slow |
| 31 | zoomme | github.com/zdo/zoomme | NOASSERTION | C++, Qt | 9 | 2010-09-05 | none | Dead |
| 32 | ttran134/ZoomIt | github.com/ttran134/ZoomIt | none | C++, OpenGL | 1 | 2023-10-01 | none | Dead prototype |
| ref | ZoomIt for Mac (not Linux) | github.com/microsoft/ZoomitForMac | MIT | Swift, ScreenCaptureKit | 1,059 | 2026-09-18 | 12.3.0 (2026-08-19) | Active; Microsoft |
| ref | markeron (no Linux build) | github.com/ifer47/markeron | MIT | Tauri v2 (Rust, Vue) | 1,071 | 2026-09-29 | v2.11.0 | Windows and macOS only |

Searches for "zoomit linux", "zoomit clone", "zoomit alternative linux" and the `zoomit` and `screen-annotation` GitHub topics found only these ZoomIt-positioned Linux projects: wayscriber (README: "A ZoomIt-like real-time screen annotation tool for Linux/Wayland"), Zoomix, zoomme, ttran134/ZoomIt, Anatomico, and a fork `kaljjuta/wayscriber-zoomit` (0 stars, no changes of note). The AlternativeTo list for ZoomIt on Linux names KMag, Wayscriber, Virtual Magnifying Glass, Gromit-MPX, Magnifiqus and Magnus (https://alternativeto.net/software/zoomit/?platform=linux).

### 1.2 Display-server support

Yes = works natively. XW = works only as an XWayland client. Part = partial. No = does not work or not designed for it. "?" = not established.

| Tool | X11 | GNOME (Mutter) Wayland | KDE (KWin) Wayland | wlroots / Sway | Hyprland |
|---|---|---|---|---|---|
| wayscriber | No | Part (fullscreen xdg window; freeze via portal; no click-through) | Yes (layer-shell; freeze and zoom via portal) | Yes | Yes |
| Gromit-MPX | Yes | XW (hotkey via GNOME custom keybinding) | XW | XW | XW |
| Ardesia | Yes (needs compositing) | XW | XW | XW | XW |
| boomer | Yes | No (X root capture; inferred) | No (inferred) | No (inferred) | No (inferred) |
| woomer | No | No (needs wlr-screencopy) | No | Yes | Yes |
| wooz | No | No | No | Yes | Yes |
| zooma | Yes (scrot) | Part (shells out to flameshot) | Part (shells out to spectacle) | Yes (grim) | Yes (grim) |
| Draw On Gnome | Yes (GNOME 46 to 49 X11 session) | Yes (Shell extension) | No | No | No |
| KWin Zoom, Mouse Mark | Yes (KWin X11 session) | No | Yes | No | No |
| GNOME Magnifier | Yes | Yes | No | No | No |
| Better Desktop Zoom | Yes | Yes | No | No | No |
| KMag | Yes | ? | No (no output, KDE bug 438912) | ? | ? |
| Magnus | Yes | ? | ? | ? | ? |
| xzoom, xm | Yes | XW (root capture limits, inferred) | XW | XW | XW |
| Flameshot | Yes | Part (experimental, portal) | Part (experimental, portal) | Part (grim adapter, portal) | Part (documented) |
| ksnip | Yes | Part (portal only, GNOME 41+) | Yes (KWin D-Bus) | Part (portal) | Part (portal) |
| Satty, swappy | Editor window, needs an image from elsewhere | Editor window (image from portal or file) | same | Yes (grim + slurp) | Yes |
| Spectacle | Yes (xcb) | No (README: outside KDE it is an X11 tool) | Yes (KWin D-Bus, PipeWire) | No | No |
| DrawPen | Yes | Part (Electron; segfaults reported, workaround `--ozone-platform=x11`) | Part (same) | ? | ? |
| Zoomix | Yes (Cinnamon) | No | No | No | No |
| Anatomico | Yes (GNOME 45 to 49) | Yes | No | No | No |
| MagicScribe | No | No | Yes (Plasma only) | No | No |
| presenter-overlay | No | No | No | No | Yes (Omarchy only) |

### 1.3 Feature coverage against ZoomIt

Y = present. P = partial or indirect. - = not found. Columns: static zoom (frozen screenshot you zoom and draw on), live zoom, freehand draw, shapes (line, rectangle, ellipse, arrow), text, highlighter, blur, snip to clipboard or file, break timer, recording, DemoType.

| Tool | Static zoom | Live zoom | Freehand | Shapes | Text | Highlighter | Blur | Snip | Timer | Record | DemoType |
|---|---|---|---|---|---|---|---|---|---|---|---|
| wayscriber | Y | - | Y | Y | Y | Y | Y | Y (plus OCR) | - | - | - |
| Gromit-MPX | - | - | Y | Y | - | P | - | - | - | - | - |
| Ardesia | - | - | Y | Y | Y | Y | - | P | - | P (VLC) | - |
| boomer | Y | P (`-d:live`) | - | - | - | - | - | - | - | - | - |
| woomer | Y | - | - | - | - | - | - | - | - | - | - |
| wooz | Y | - | - | - | - | - | - | - | - | - | - |
| zooma | Y | - | - | - | - | - | - | - | - | - | - |
| Draw On Gnome | - | - | Y | Y | Y | Y | - | P (saves drawing) | - | - | - |
| KWin Zoom | - | Y | - | - | - | - | - | - | - | - | - |
| KWin Mouse Mark | - | - | Y | P (arrow) | - | - | - | - | - | - | - |
| GNOME Magnifier | - | Y | - | - | - | - | - | - | - | - | - |
| Better Desktop Zoom | - | Y | - | - | - | - | - | - | - | - | - |
| KMag, Magnus, xzoom | - | P (window magnifier) | - | - | - | - | - | - | - | - | - |
| Flameshot | - | - | Y | Y | Y | Y | P (pixelate) | Y | - | - | - |
| ksnip | - | - | Y | Y | Y | Y | Y | Y | - | - | - |
| Satty | - | - | Y | Y | Y | Y | Y | P (editor) | - | - | - |
| swappy | - | - | Y | Y | Y | P | Y | P (editor) | - | - | - |
| Spectacle | - | - | P | P | P | P | P | Y | - | Y | - |
| Zoomix | Y | Y (Cinnamon magnifier) | Y | Y | Y | Y | - | Y | - | - | - |
| Anatomico | - | P (GNOME magnifier) | Y | Y | Y | - | - | - | - | - | - |
| MagicScribe | - | - | Y | Y | - | - | - | - | - | - | - |
| ZoomIt for Mac (ref) | Y | Y | Y | Y | Y | Y | - | Y | Y | Y | Y |

No Linux tool has a break timer, DemoType or a full recorder tied to the overlay. Section 4 explains what that adds up to.

## 2. Tool notes

Each entry: facts, display-server behaviour, coverage against ZoomIt, techniques worth borrowing. Numbers match table 1.1.

### 2.1 Closest to ZoomIt

#### wayscriber (1)
- Facts: https://github.com/devmobasa/wayscriber, MIT, Rust, 751 stars, active, v0.9.25. Site https://wayscriber.com/docs/.
- Positioning: "A ZoomIt-like real-time screen annotation tool for Linux/Wayland". README has a "Comparison with ZoomIt" table and marks break timer as missing.
- Display servers: Wayland only. README table: layer-shell compositors (Hyprland, Sway, River, Wayfire, Niri, COSMIC) supported; Plasma/KWin supported with layer-shell, but Freeze and Zoom need `xdg-desktop-portal-kde`'s screenshot portal and may prompt; GNOME partial (ordinary overlay and Freeze via portal, "light passthrough" unavailable); X11 not supported. Tested on Ubuntu 25.10 GNOME, Fedora 43 KDE and GNOME, Debian 13 KDE and GNOME, CachyOS KDE, Hyprland, Niri.
- Coverage: freehand, highlighter, eraser, line, rectangle, ellipse (Tab), arrow (Ctrl+Shift), polygons, blur (soften, pixelate, black out), spotlight with magnification, multiline text and sticky notes, whiteboard and blackboard (Ctrl+W, Ctrl+B), boards and pages, undo and redo, colour keys R G B Y O P W K, laser pointer, click highlights, presenter mode, keystroke HUD, freeze (Ctrl+Shift+F), region capture to clipboard or file, OCR through tesseract (Ctrl+Shift+X), PDF export. Zoom is scroll-wheel zoom at the cursor plus pan by middle-drag or arrows (Ctrl+wheel, Ctrl+Alt +/-), applied to a frozen capture. It does not use ZoomIt's mouse-follows-view mapping. No live zoom, no recording, no DemoType, no break timer.
- Techniques:
  - Overlay: `wlr-layer-shell` surface per output through smithay-client-toolkit (`src/backend/wayland/handlers/layer.rs`); on GNOME a fullscreen `xdg_toplevel` fallback (`src/backend/wayland/handlers/xdg.rs`).
  - Click-through "light passthrough": empty input region on the layer surface (`src/backend/wayland/overlay_passthrough.rs`), with `--light-toggle` and `--light-draw-on/off` CLI commands meant to be bound to compositor shortcuts.
  - Capture: `zwlr_screencopy` or `ext-image-copy-capture` when the compositor has them (`src/backend/wayland/frozen/capture.rs`, `src/backend/wayland/zoom/capture.rs`), otherwise the xdg-desktop-portal Screenshot call (`src/backend/wayland/zoom/portal.rs`, `src/capture/portal.rs`, worker in `portal_task.rs`, crop and resample in `portal_raster.rs`). External tools `grim`, `slurp`, `wl-clipboard` for region capture where present.
  - Hotkeys: daemon mode with a systemd user unit and `wayscriber --daemon-toggle`. The user binds that command in the compositor or GNOME custom shortcuts. On KDE, an optional xdg-desktop-portal GlobalShortcuts session over zbus (`src/daemon/global_shortcuts.rs`, preferred trigger `<Ctrl><Shift>g`, id `toggle-overlay`, env `WAYSCRIBER_PORTAL_SHORTCUT`). The configurator can write a GNOME custom shortcut.
  - Tray: StatusNotifierItem via `ksni`. Optional libinput/udev keystroke capture needs `/dev/input` access (build feature `input-monitor`).
  - Daemon control protocol: `docs/daemon-protocol-v2.md`, `src/daemon/protocol_v2/`.
- Weak points seen: a very large code base for one maintainer; X11 unsupported; GNOME experience degraded (no click-through, portal prompts); zoom model differs from ZoomIt.

#### Zoomix (25)
- Facts: https://github.com/JohnMThompson/Zoomix, MIT, Rust + GTK3, 2 stars, v0.2.2 (2026-07-04).
- Positioning: "Linux Mint Cinnamon/X11 screen zoom, annotation, and image snip utility inspired by Sysinternals ZoomIt". Version 0.2 is X11 only; video capture, timer, DemoType, OCR and Wayland are declared out of scope.
- Hotkeys mirror ZoomIt with a Shift added: Ctrl+Shift+1 static zoom, Ctrl+Shift+2 draw, Ctrl+Shift+3 snip, Ctrl+Shift+4 live zoom. Modes combine (zoom then draw then snip keeps zoom and annotations). Tool keys 1 to 7 and letters for pen, rectangle, arrow, line, ellipse, highlight, eraser; colour keys Shift+R red, g, b, y, k, w.
- Technique: live zoom is handed to Cinnamon's compositor magnifier; static zoom, draw and snip open on the monitor holding the pointer. Only useful as a UX reference.

#### Anatomico (26)
- Facts: https://github.com/charly-vibes/canticos (`apps/anatomico`), MIT, 0 stars. README states the code was written by an LLM.
- A GNOME Shell extension (45 to 49) in TypeScript, "native Wayland, no XWayland". Pen, arrow, rectangle, ellipse, text, 5 colours, undo, clear; zoom is GNOME's own magnifier. Toggle Super+Z. Useful only as a minimal example of the extension route.

#### zoomme (31) and ttran134/ZoomIt (32)
- https://github.com/zdo/zoomme (Qt, 2010, keys R G B C M Y W, digits for width, Z/X for line or rectangle) and https://github.com/ttran134/ZoomIt (C++, OpenGL shaders, no README). Both dead. Nothing to borrow.

#### ZoomIt for Mac (ref)
- Facts: https://github.com/microsoft/ZoomitForMac, MIT, Swift, 1,059 stars, 12.3.0 (2026-08-19), created 2026-07-02.
- Not Linux, but it is Microsoft's own port of every ZoomIt mode (static and live zoom, draw, type, break timer, MP4 recording with webcam, region and OCR snip, panorama, DemoType, DemoMirror) onto a non-Win32 stack, so it shows how to split the design. It mirrors the viewport maths (`Sources/ZoomItMacCore/Overlay/ZoomViewportController.swift`: `sourceRect(for:cursorLocation:)`, `adjustToMoveBoundary`, `moveRegions = 8`, `stepIn 1.1`, `stepOut 0.8`) and uses a mode state machine (`Core/ModeCoordinator.swift`, `Core/AppCommand.swift`).

### 2.2 Zoom-only tools

#### boomer (4)
- Facts: https://github.com/tsoding/boomer, MIT, Nim, 852 stars, no releases, last commit 2026-05-24.
- X11 only. Controls: wheel or `=`/`-` zoom, drag to pan, `f` flashlight, Ctrl+wheel flashlight radius, `m` mirror, `0` reset, Esc or `q` quit. Config file with `min_scale`, `scroll_speed`, `drag_friction`, `scale_friction`. Experimental flags: `-d:live` (live update), `-d:mitshm`, `-d:select` (track one window).
- Techniques: screenshot with `XGetImage`, or `XShmGetImage` for the live flag (`src/screenshot.nim`, `newScreenshot`); an override-redirect window covering the monitor with a GLX context (`src/boomer.nim`, `main`); zoom and pan with velocity and friction rather than fixed steps (`src/navigation.nim`, `Camera.update`); flashlight as a fragment shader (`src/frag.glsl`); XRandR for monitor geometry. It shows the "screenshot then a GL window" pattern that ZoomIt uses on Windows.

#### woomer (5)
- Facts: https://github.com/coffeeispower/woomer, MIT, Rust, 272 stars, 0.2.0.
- Wayland, wlroots family. Wheel zoom, drag pan, `F` flashlight, Ctrl+Shift+wheel radius, right click or Esc/A/Q quits. Options `--monitor`, `--output`, `--radius`, `--show-cursor`.
- Techniques: `libwayshot` (a fork branch `freeze-feat-andreas`) captures through wlr-screencopy (`src/main.rs`: `WayshotConnection::screenshot_single_output` / `screenshot_all`); raylib with the GLFW Wayland backend opens a transparent fullscreen window (`ToggleFullscreen`, `SetWindowMonitor`). GNOME and KWin lack wlr-screencopy, so it cannot run there. README notes HiDPI needs `GDK_SCALE` and Hyprland XWayland settings.

#### wooz (6)
- Facts: https://github.com/negrel/wooz, MIT, C, 119 stars, archived, v0.1.0.
- Wayland, wlroots family (`wlr-screencopy`, `wp_viewporter`, `xdg-shell`). Wheel zoom at pointer, drag pan, `+`/`-`, arrows, `0`, right click or Esc quits, `--mouse-track` (view follows pointer without clicking), `--zoom-in`, `--output`.
- Techniques: one screencopy frame per output into a shm buffer (`main.c`: `screencopy_frame_handle_buffer`, `screencopy_frame_handle_ready`); zoom is done by the compositor. `render_window()` only calls `wp_viewport_set_source(x, y, w, h)` on the fullscreen surface, so the client never rescales pixels. `apply_zoom()` keeps the view centred on the pointer ratio. This is the cheapest static-zoom design on Wayland.

#### zooma (7)
- Facts: https://github.com/phreshbrread/zooma, MIT, Rust, 0 stars, created 2026-08-16.
- "Screen zoomer for X11 and Wayland", inspired by boomer. Wheel zoom, drag pan, Ctrl for spotlight, `R` reset. Config `~/.config/zooma/settings.toml`.
- Technique: shells out to a per-desktop screenshot tool: `scrot` (X11), `grim` (Wayland), optional `spectacle` (KDE Wayland) and `flameshot` (GNOME Wayland); GLFW window. Pragmatic but relies on Flameshot's portal prompt on GNOME. Very new, unproven.

#### xzoom and xm (16)
- https://github.com/ToyKeeper/xzoom (fork of Itai Nahshon's xzoom 0.3, C, Xlib + XShm, memory-leak fix documented in its README) and https://github.com/c-blake/xm (Nim rewrite, MIT, config file). A follow-the-pointer magnifier window on X11 only. Not a ZoomIt substitute.

#### KMag (14) and Magnus (15)
- KMag: https://github.com/KDE/kmag, C++/Qt, GPL-2.0-or-later, maintained. A window that shows a magnified region, follows mouse or focus, 25 fps `QTimer` calling `QScreen::grabWindow` (`src/kmagzoomview.cpp`: `grabFrame`). On Wayland the grab returns nothing, so the window is empty (KDE bug 438912, first reported on Plasma 5.22.1). Not an overlay.
- Magnus: https://github.com/stuartlangridge/magnus, Python + GTK3 + libkeybinder (an X11 key-grab library), last commit 2024-10-14. X11 window magnifier.

### 2.3 Draw-over-desktop tools

#### Gromit-MPX (2)
- Facts: https://github.com/bk138/gromit-mpx, GPL-2.0, C + GTK3 + Cairo, 1,398 stars, 1.9.0 (2026-03-18), active. Flatpak on Flathub. Port of Simon Budig's Gromit.
- Display servers: X11 fully (XInput2 multi-pointer, XComposite, tablets with pressure and eraser end). On Wayland only through XWayland: README says "with a Wayland session using XWayland (most desktop environments)". A screenshot in the repo shows it on GNOME Wayland.
- Coverage: freehand pen, tools defined in `gromit-mpx.cfg` (colour, size, `rgba()` transparency, arrow heads, modifier and button mappings such as Shift for a blue pen and Button3 for the eraser), line, rectangle, circle (1.8.0), smooth and orthogonal tools (1.7.0), length label for lines (1.9.0), undo and redo (100 steps per NEWS since 1.7.0, compressed; the README text still says 4), canvas opacity. No text, no zoom, no blur, no snip.
- Hotkeys: F9 toggles painting, Shift+F9 clears, Ctrl+F9 hides, Alt+F9 quits, F8 undo, Shift+F8 redo. On X11 it grabs the keys. It also has a remote-control CLI (`gromit-mpx --toggle`, `--clear`, `--undo`, `--line ...`).
- Techniques worth borrowing:
  - Wayland hotkeys without a portal: `src/input.c`, `add_hotkeys_to_compositor()` and `remove_hotkeys_from_compositor()`. Its comment says that under (X)Wayland it is "per design not possible to listen for a certain hotkey press", so on GNOME it writes a custom keybinding through GSettings (`org.gnome.settings-daemon.plugins.media-keys` `custom-keybindings`) that runs the `gromit-mpx --toggle` command, named with a fixed prefix so it can be removed on exit.
  - Overlay: one fullscreen undecorated `GTK_WINDOW_POPUP` with an RGBA visual and a Cairo backbuffer (`src/main.c`, `main()` around line 1247). Click-through when inactive by giving the window an empty input shape (`release_grab()` in `src/input.c`), and, without compositing, by shaping the window to the drawn pixels (`gtk_widget_shape_combine_region` in `src/main.c`).
  - Undo with delta compression (`undo_compress`, `undo_decompress` in `src/main.c`).
- Weak points: XWayland limits (cannot see or grab input for native Wayland surfaces without workarounds, hotkey grabs do not work); GNOME 50 has no X11 session.

#### Ardesia (3)
- Facts: https://github.com/pilollipietro/ardesia, GPL-3.0, C + GTK3 + Cairo, 2 stars, v2.0 (2022-08-26), last commit 2026-03-05 (comment). Older fork https://github.com/gfreeau/ardesia (37 stars, 2020, from code.google.com). Status of the current fork is unclear.
- Display servers: README says X11 with a composite manager, or Wayland via XWayland. GNOME/KDE/wlroots behaviour otherwise not documented.
- Coverage: freehand ink, shape recogniser, pen, highlighter, eraser, text, arrow, fill, colour palette, undo, export to image or multi-page PDF, screen recording and streaming through VLC, IWB import and export. No zoom.
- Borrowable idea: the shape recogniser (also in wayscriber's Shape Pen). Otherwise legacy.

#### Draw On Gnome (8), Draw On Your Screen 2 (9), Draw On Your Screen (9b)
- Facts: https://github.com/daveprowse/Draw-On-Gnome (GPL-3.0, GJS, 156 stars, v11.1, active; docs https://daveprowse.github.io/Draw-On-Gnome/); archived https://github.com/zhrexl/DrawOnYourScreen2 (251 stars); origin https://codeberg.org/som/DrawOnYourScreen by abakkk. Draw On Gnome is the maintained fork and the extensions.gnome.org page of the original points to it. It targets GNOME 46 to 50 (main branch `metadata.json` lists 50; release channels for 46 to 49).
- Display servers: GNOME Shell only. X11 and Wayland behave the same because it runs inside the shell.
- Coverage: freehand, rectangle, circle, ellipse, line, polygon, polyline, arrow, text, images, transforms (move, rotate, resize, mirror), laser pointer, highlighter, blackboard, grid, persistence on the desktop background, multi-monitor, Ctrl+S to save PNG or SVG. No zoom, no blur, no timer, no recording. Toggle with Super+Alt+D.
- Techniques worth borrowing:
  - A `St.DrawingArea` per monitor placed in `Main.layoutManager.uiGroup`, with Cairo drawing in `vfunc_repaint()` (`area.js`, `class DrawingArea extends St.DrawingArea`, sized with a `Clutter.BindConstraint`).
  - Global shortcuts inside the compositor with `Main.wm.addKeybinding(...)` (`areamanager.js`, `AreaManager`).
  - Input capture with `Main.pushModal(activeArea, {actionMode})` and release with `Main.popModal(grab)`; toggling between drawing (modal) and "persist on background" by re-parenting the actor between `uiGroup` and the background group (`areamanager.js` around lines 345 to 380).
  - Colour pick with `Shell.Screenshot().pick_color()` (`area.js`).
  - Cost: one extension per GNOME Shell version (the code notes `Meta.Cursor` and `display.set_cursor()` were removed in GNOME 50).

#### Others in this group
- DrawPen (23): https://github.com/DmytroVasin/DrawPen, MIT, Electron, 1,035 stars, v0.0.58 (2026-09-26). Global shortcut Ctrl+Shift+A. README warns of segfaults on some Wayland setups (Fedora KDE, Zorin); workaround `drawpen --ozone-platform=x11` or the `drawpen-x11` package. Electron's global shortcuts do not solve Wayland hotkeys.
- Pensela (22): https://github.com/weiameili/Pensela, ISC, Electron, archived 2022. Shapes, stickers, highlighter, laser pointer, text, screenshot tool via ImageMagick "not guaranteed on Wayland".
- ScreenPen (24): https://github.com/rsusik/screenpen, GPL-3.0, Python. If the window system supports live transparency it draws over the live desktop, else it takes a screenshot and draws on that; `-t` forces transparent mode. Lines, rectangles, charts.
- MagicScribe (27): https://github.com/Cartaz/Magicscribe, MIT, PySide6/Qt Quick, KDE Plasma native Wayland only. One Top-layer `layer-shell-qt` surface per `QScreen` sharing a global canvas coordinate space, click-through with `WindowTransparentForInput` (input region committed with `requestUpdate()`), and global drawing shortcuts through xdg-desktop-portal GlobalShortcuts via Jeepney on a worker thread. Pen, smooth stroke, line, rectangle, circle, eraser. New, 0 stars, but a clean recipe for Qt on KDE.
- wayland-screen-annotator (28): https://github.com/ukewea/wayland-screen-annotator, Rust + GTK4 + gtk4-layer-shell, KDE primary target. Single commit.
- presenter-overlay (29): https://github.com/andreas-bylund/presenter-overlay, MIT, QML plugin for the Omarchy shell (Hyprland based). Pen, arrows, rectangles, spotlight, click ripples.
- hati (30): https://github.com/szymonwilczek/hati, GPL-3.0, GNOME Shell extension. Cursor highlight ring with click ripples, spotlight, and a "GPU magnifier". Shows a compositor-side cursor overlay.
- markeron (ref): https://github.com/ifer47/markeron, MIT, Tauri v2. Windows and macOS builds only; skip.

### 2.4 Compositor built-ins (zoom and mark inside the shell)

#### KWin Zoom effect (10)
- Facts: https://github.com/KDE/kwin, `src/plugins/zoom/` (`zoom.cpp`, `zoom.h`, `zoom.kcfg`), GPL-2.0-or-later, active.
- Behaviour: live magnification of the whole desktop, drawn inside the compositor. Default global shortcuts Meta+= or Meta++ zoom in, Meta+- zoom out, Meta+0 actual size (`KGlobalAccel::setGlobalShortcut` in the constructor). Zoom step `ZoomFactor` default 1.2. Mouse tracking modes (`zoom.h` enum): Proportional (default), Centered, Push, Disabled, CenteredStrict. Optional focus tracking and text-caret tracking (`FocusDelay` 350 ms), pointer-axis gesture with Meta+Control, "pattern upscaler" and a pixel grid at 15x. Keyboard panning actions exist with no default shortcut. Animation is linear over `150 ms x ZoomFactor`.
- Proportional tracking is the same mapping ZoomIt uses: `m_xTranslation = -int(trackPoint.x() * (m_zoom - 1.0))` in `ZoomEffect::prePaintScreen` (line 245). ZoomIt adds a one-eighth edge margin on top of it.
- Worth borrowing: the tracking-mode set and the `paintScreen`/`prePaintScreen` structure. It cannot draw, freeze or snip, and it only exists on KDE.

#### KWin Mouse Mark effect (11)
- Facts: same repo, `src/plugins/mousemark/mousemark.cpp` (358 lines), GPL-2.0-or-later.
- Behaviour: draw lines on the live desktop from inside KWin. Default modifiers: Shift+Meta plus drag draws freehand (`Freedrawmeta`, `Freedrawshift` true); Shift+Ctrl+Meta plus drag draws an arrow (`Arrowdrawmeta`, `Arrowdrawcontrol`, `Arrowdrawshift` true). Global shortcuts Shift+Meta+F11 clears all marks and Shift+Meta+F12 clears the last one. Line width 3, colour red (config). Also handles touch (`touchDown`, `touchMotion`, `touchUp`).
- Technique: `MouseMarkEffect::paintScreen` draws GL line strips over the scene; `slotMouseChanged` receives pointer events while the modifiers are held, so the desktop stays interactive. Shows what a KWin effect can do without any overlay window. Not usable by an external application unless it ships as a KWin effect or script.

#### GNOME Magnifier (12) and Better Desktop Zoom (13)
- Facts: GNOME Shell `js/ui/magnifier.js` (https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/main/js/ui/magnifier.js), GPL-2.0-or-later. Settings schema `org.gnome.desktop.a11y.magnifier`: `mag-factor` default 2.0, `mouse-tracking` default proportional (values none, centered, push, proportional), `screen-position` default full-screen (plus lens mode), `lens-mode`, cross-hairs, colour effects. Default shortcuts in `org.gnome.settings-daemon.plugins.media-keys`: Alt+Super+8 toggle, Alt+Super+= zoom in, Alt+Super+- zoom out.
- Technique: `class ZoomRegion` builds an `St.Bin` on `global.stage` holding a `Clutter.Clone` of `Main.uiGroup` (`_createActors()`), scales it, and moves the viewport (`_setViewPort()`, `scrollToMousePos()`); a second clone draws the cursor. Because it clones the shell's own actor tree there is no screen capture and no permission prompt.
- Better Desktop Zoom (https://github.com/popov895/better-desktop-zoom, MIT, v5.0) is an extension that makes this magnifier behave like KWin's zoom: Meta+= to start or increase, Meta+- to decrease or stop, Ctrl+Super+scroll (Wayland only).
- Controllable from outside: the GSettings keys `org.gnome.desktop.a11y.applications screen-magnifier-enabled` and `org.gnome.desktop.a11y.magnifier mag-factor` (range 0.1 to 32) switch the native magnifier on and set its level. Anatomico's README says its zoom is GNOME's built-in magnifier; Zoomix hands live zoom to Cinnamon's equivalent. I did not check how either one drives it (UNCERTAIN).

#### Hyprland zoom, and hati
- Hyprland has a built-in cursor zoom: config `cursor:zoom_factor` (float, 1.0 to 10, 1.0 means off), `cursor:zoom_rigid` (cursor always centred versus loose), `cursor:zoom_detached_camera` (default true), `cursor:zoom_disable_aa`. Source: hyprland-wiki `content/configuring/core/config-options.md`. Scripts set it with `hyprctl keyword cursor:zoom_factor`. Wiki example binds SUPER+Z and SUPER+keypad plus. It is the live-zoom path on Hyprland.
- Sway and other wlroots compositors have no built-in magnifier that I found.

### 2.5 Screenshot and annotation tools

#### Flameshot (17)
- Facts: https://github.com/flameshot-org/flameshot, GPL-3.0, C++/Qt, 31,026 stars, v14.0.0 (2026-06-19, GitHub API), active.
- Display servers: X11; on Wayland it uses xdg-desktop-portal ("the only way to screenshot on Wayland" per the v14 notes), with an optional grim adapter (`useGrimAdapter=true`). Flameshot's own docs call GNOME and Plasma Wayland "experimental". GNOME shows a permission dialog. Multi-monitor was redesigned in v14: you pick one monitor to capture.
- Coverage: region selection then annotate in place: pen, line, arrow, rectangle, circle, marker (highlighter), text, pixelate, counter, undo; copy or save; upload. (Its README hotkey table lists pixelate; I did not see a separate blur tool.) No zoom, timer or recording.
- Hotkeys: none global on Wayland. The docs tell users to add a GNOME custom shortcut running `flameshot gui` (sometimes wrapped as `QT_QPA_PLATFORM=wayland flameshot gui`), or to use a D-Bus call (v14 adds a D-Bus capture method). Docs: https://flameshot.org/docs/guide/wayland-help/.
- Worth borrowing: in-place annotate-on-selection UX (the same idea as ZoomIt's snip plus draw), and the portal call pattern.

#### ksnip (18)
- Facts: https://github.com/ksnip/ksnip, GPL-3.0, C++/Qt + kImageAnnotator, 3,341 stars, last release 2023-03-15 but commits continue (2026-09-07).
- Display servers per README: X11; Plasma Wayland via KWin; GNOME Wayland via portal only from GNOME 41 (GNOME restricted `org.gnome.Shell.Screenshot` to allow-listed callers); other compositors through the portal. Backends are separate classes: `src/backend/imageGrabber/KdeWaylandImageGrabber`, `GnomeWaylandImageGrabber`, `WaylandImageGrabber` (portal), `BaseX11ImageGrabber`, chosen by `ImageGrabberFactory`. README: "Global hotkeys for capturing screenshots (currently only for Windows and X11)". Known Wayland issues listed in README section "Wayland".
- Coverage: annotate with pen, marker, arrow, rectangle, ellipse, text, numbers, blur and pixelate, stickers; OCR plugin. No zoom, timer or recording.
- Worth borrowing: the per-desktop grabber abstraction and the honest list of what fails on each desktop.

#### Satty (19) and swappy (20)
- Satty: https://github.com/Satty-org/Satty, MPL-2.0, Rust + GTK4 + OpenGL, 2,427 stars, v0.22.0. Takes a file or stdin and annotates: pointer, crop, brush, line, arrow, rectangle, ellipse, text, numbered marker, blur, pixelate, highlight; Enter or Ctrl+C copies via a configurable command such as `wl-copy`; `--fullscreen [all|current-screen]`; keyboard tool keys p c b i z r e t m u x g; Ctrl+wheel zoom and pan (0.22.0). Stated goal: working on wlroots compositors. It does not capture.
- swappy: https://github.com/jtheoof/swappy, MIT, C + GTK3, 1,506 stars. "A Wayland native snapshot and editor tool" fed by `grim -g "$(slurp)" - | swappy -f -`.
- Worth borrowing: the tool set and the pipe-based composition (capture tool, editor, clipboard tool). Neither owns the screen, so neither is a ZoomIt substitute.

#### Spectacle (21)
- Facts: https://github.com/KDE/spectacle, mixed GPL/LGPL/BSD per file, C++/Qt/QML, active.
- Wayland capture: `org.kde.KWin.ScreenShot2` over D-Bus, results returned through a pipe file descriptor (`src/Platforms/ImagePlatformKWin.cpp`). That interface is restricted: Spectacle's desktop file declares `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2` and `X-KDE-Wayland-Interfaces=org_kde_plasma_window_management,zkde_screencast_unstable_v1` (`desktop/org.kde.spectacle.desktop.cmake`). Developers can bypass the check with `KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1` (README).
- Wayland recording: `zkde_screencast_unstable_v1` gives a PipeWire node, encoded through PipeWire encoders (`src/Platforms/screencasting.cpp`, `VideoPlatformWayland.cpp`), with screen, region and window modes and microphone and system audio.
- Shortcuts are declared in the desktop file: `X-KDE-Shortcuts=Print,Meta+Shift+S` on the main action, `Shift+Print`, `Meta+Print` on others, so KDE registers them at install time.
- Annotation: `src/Gui/AnnotationEditor.qml` and toolbars exist; I did not enumerate the tools. UNCERTAIN.
- Worth borrowing: the desktop-file approach to authorised capture and declared global shortcuts on KDE.

### 2.6 Adjacent tools for the missing ZoomIt modes

| Mode | Tools | Notes |
|---|---|---|
| Recording | OBS Studio (76,803 stars, GPL-2.0), Kooha (3,519, GPL-3.0, Rust, GNOME, portal + PipeWire + GStreamer), wf-recorder (1,311, MIT, wlroots), wl-screenrec (629, Apache-2.0, wlroots, hardware encoding), GNOME's built-in screencast, KDE Spectacle | Portal ScreenCast plus PipeWire is the generic route; wlroots tools use screencopy directly. Kooha's README lists `pipewire`, `gstreamer-plugin-pipewire` and `xdg-desktop-portal` as requirements. |
| Break timer | Stretchly (6,567, BSD-2-Clause, Electron), Workrave (1,822, GPL-3.0, C++), Safe Eyes (1,761, GPL-3.0, Python) | Reminder apps, not ZoomIt-style full-screen countdown you show an audience. Nothing to reuse except concepts. |
| DemoType | demo-magic (1,897, MIT, shell), VHS (21,026, MIT, Go, terminal only), wtype (563, MIT, `virtual-keyboard` on wlroots), ydotool (2,363, AGPL-3.0, `/dev/uinput`) | None types a script into an arbitrary GUI window with per-keystroke pacing and a key block. Wayland input injection has three routes: virtual-keyboard protocol (wlroots), uinput (needs privileges), and libei through the RemoteDesktop portal (GNOME, KDE). |
| Screen grab helpers | grim (MIT; the GitHub mirror is archived, last push 2022, so development lives elsewhere), slurp (region picker), wayshot/libwayshot (BSD-2-Clause), wl-clipboard | wlroots only. |
| Portals | xdg-desktop-portal-gnome, -kde, -wlr, -hyprland | GlobalShortcuts is implemented by GNOME (48+), KDE and Hyprland backends; `xdg-desktop-portal-wlr` ships none. Sources: https://discourse.gnome.org/t/how-do-you-enable-disable-global-shortcuts-in-gnome-48/29119, https://wiki.hypr.land/Hypr-Ecosystem/xdg-desktop-portal-hyprland/ |

## 3. Techniques by problem

### 3.1 Getting pixels for freeze, zoom and snip

| Environment | Mechanism | Prompt | Seen in |
|---|---|---|---|
| X11 | `XGetImage`, or `XShmGetImage` for speed | none | boomer `src/screenshot.nim`, xzoom |
| wlroots, Sway, Hyprland, river, Wayfire | `zwlr_screencopy_manager_v1` (Hyprland and niri list wlr-screencopy only, Sway lists both, COSMIC lists `ext-image-copy-capture`) | none | wooz `main.c`, woomer via libwayshot, wayscriber `frozen/capture.rs` |
| Any compositor with `ext-image-copy-capture-v1` | `ExtImageCopyCaptureManagerV1` sessions | none | wayscriber (both protocols); Sway 1.11, COSMIC. Mutter, KWin, Hyprland, niri and Weston lacked it in the wayland.app tables used here |
| KDE Plasma | D-Bus `org.kde.KWin.ScreenShot2` (image comes back on a pipe fd). Restricted: the app's `.desktop` must list `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2`. Streams through `zkde_screencast_unstable_v1` (also declared in the desktop file with `X-KDE-Wayland-Interfaces`) | none once authorised | Spectacle `ImagePlatformKWin.cpp`, `VideoPlatformWayland.cpp`; ksnip `KdeWaylandImageGrabber` |
| Any desktop with a portal | `org.freedesktop.portal.Screenshot.Screenshot` (returns a file URI), `ScreenCast` + PipeWire for streams | GNOME shows a permission dialog; KDE may | Flameshot `ScreenGrabber::freeDesktopPortal` in `src/utils/screengrabber.cpp`; wayscriber `capture/portal.rs`; ksnip on GNOME 41+ |
| Inside the shell (GNOME extension) | `Shell.Screenshot` API, or clone the actor tree with `Clutter.Clone` of `Main.uiGroup` | none | Draw On Gnome `area.js` (`pick_color`), GNOME `magnifier.js` |
| Shelling out | `scrot`, `grim`, `spectacle`, `flameshot` | as the tool | zooma |

Gotchas found in the sources:
- Flameshot passes a non-empty `parent_window` to the portal because xdg-desktop-portal-gnome 46 and later rejects an empty string ("Failed to associate portal window with parent window ''"); on Wayland it sends `"wayland:"`, on X11 an `x11:0x...` handle from a hidden widget.
- GNOME restricted the private `org.gnome.Shell.Screenshot` D-Bus interface to allow-listed callers from GNOME 41, which is why ksnip's native GNOME Wayland path only works below 41 (ksnip README).
- wayscriber keeps a retry-and-settle policy for portal captures (`src/backend/wayland/zoom/retry.rs`) and validates that the captured size and aspect match the output (`finalize_capture_image` in `zoom/capture.rs`), which is needed on mixed-scale multi-monitor setups (its newest commit fixes KDE portal capture on mixed scales, #400).
- Flameshot v14 dropped cross-monitor selection because Linux desktops disagree about mixed scaling; it now asks for one monitor.

### 3.2 Full-screen overlay windows

| Environment | Mechanism | Click-through when idle | Seen in |
|---|---|---|---|
| X11 | Undecorated popup or override-redirect window with an RGBA visual, fullscreen | XShape input region (empty region) | Gromit-MPX `main.c` (`GTK_WINDOW_POPUP`, `gtk_window_fullscreen`); boomer (`override_redirect = 1`, GLX) |
| wlroots and KWin | `wlr-layer-shell` surface on the overlay layer, anchored to all edges, per output | Empty `wl_surface` input region | wayscriber `handlers/layer.rs`, `overlay_passthrough.rs`; MagicScribe `ui/native/layer_shell.py` (layer-shell-qt, one Top-layer surface per `QScreen`, `WindowTransparentForInput`) |
| Any Wayland compositor | Fullscreen `xdg_toplevel` | Not possible in general; ordinary windows cannot pass input through | woomer (raylib), wooz, wayscriber's GNOME fallback `handlers/xdg.rs` |
| GNOME Shell | Actors in the shell's scene: `St.DrawingArea` in `Main.layoutManager.uiGroup`, input grab with `Main.pushModal` | Yes, by removing the actor or modal grab | Draw On Gnome `areamanager.js`, `area.js`; hati; Anatomico |
| KWin | Draw from inside a compositor effect | Yes | Mouse Mark `paintScreen` |

wayscriber states the consequence for GNOME: "Stock GNOME Wayland does not support this mode. Regular app windows cannot provide the required click-through shell overlay." Its own suggestion is a GNOME Shell extension.

### 3.3 Global hotkeys

| Route | Where it works | Seen in |
|---|---|---|
| `XGrabKey` | X11 only; does not work for XWayland clients on Wayland. A third-party note says Mutter 49+ also stopped honouring XWayland-side grabs (https://github.com/aaddrick/claude-desktop-debian/blob/main/docs/learnings/wayland-global-shortcuts-portal.md; not verified here) | Gromit-MPX on X11 |
| xdg-desktop-portal `GlobalShortcuts` (`CreateSession`, `BindShortcuts` with `preferred_trigger`, `Activated` signal) | GNOME 48 and later (xdg-desktop-portal-gnome), KDE, Hyprland. Not `xdg-desktop-portal-wlr`, so not Sway or river. Ubuntu 24.04 LTS ships GNOME 46, before it existed; Ubuntu 26.04 LTS ships GNOME 50. GNOME keys shortcuts by app id rather than session id, unlike the spec | wayscriber `src/daemon/global_shortcuts.rs` (Rust, zbus); MagicScribe `ui/native/global_shortcuts.py` (Python, Jeepney) |
| GNOME custom keybinding written to GSettings, running a CLI command | GNOME, any version | Gromit-MPX `add_hotkeys_to_compositor()`; wayscriber configurator; Flameshot docs |
| Shell extension `Main.wm.addKeybinding` | GNOME Shell only | Draw On Gnome `areamanager.js` |
| KGlobalAccel, or a `.desktop` file `X-KDE-Shortcuts=` entry | KDE only | KWin effects; Spectacle desktop file |
| Compositor config line `bind = SUPER, D, exec, app --toggle` | Hyprland, Sway, river and the like | wayscriber docs, daemon mode with `--daemon-toggle` |
| Daemon plus CLI (`--toggle`, `--clear`, `--undo`) | Everywhere | Gromit-MPX, wayscriber |

Keys pressed while the overlay has focus are ordinary key events, so ZoomIt's in-mode keys (R G B, T, E, Ctrl+Z, Space and so on) need no global registration. Only the mode hotkeys do.

### 3.4 Live zoom

No third-party Wayland tool zooms the live desktop with its own code. The working routes all use the compositor:

| Route | Control from outside |
|---|---|
| KWin Zoom effect | Global shortcuts Meta+=, Meta+-, Meta+0 (KGlobalAccel actions in `zoom.cpp`). Mouse tracking modes proportional, centred, push, disabled, strictly centred |
| GNOME Magnifier | GSettings `org.gnome.desktop.a11y.applications screen-magnifier-enabled`, `org.gnome.desktop.a11y.magnifier mag-factor` (range 0.1 to 32), mouse tracking none, centred, push, proportional (default) |
| Hyprland | `cursor:zoom_factor` (1.0 to 10) with `zoom_rigid` |
| Cinnamon | Its own magnifier (Zoomix) |
| Sway and other wlroots | Nothing built in that I found |

A generic implementation would need a PipeWire ScreenCast stream rendered in a click-through overlay. I found no tool that does it, and the ScreenCast portal adds a permission prompt each session unless a restore token is used (not verified in code here).

### 3.5 Pointer confinement and input injection

ZoomIt's pen mode confines the pointer with `ClipCursor` and warps it with `SetCursorPos`, and DemoType installs a low-level keyboard hook and injects with `SendInput`. Wayland has no global equivalents:
- Pointer: Wayland now has the two pieces ZoomIt's `ClipCursor` and `SetCursorPos` need. `zwp_pointer_constraints_v1` (`confine_pointer` to a region, `lock_pointer`) is listed as supported by Mutter 51, KWin 6.7, Sway 1.11, Hyprland 0.52.1, niri 26.04, COSMIC and Weston 15.0.1. The staging protocol `wp-pointer-warp-v1` (request the compositor to move the pointer) is listed for Mutter 51, KWin 6.7, Sway 1.11 and Hyprland 0.52.1, partial in niri, COSMIC and Weston (https://wayland.app/protocols/pointer-constraints-unstable-v1, https://wayland.app/protocols/pointer-warp-v1). Older releases, such as the Mutter in Ubuntu 24.04, probably lack the warp protocol; I did not check. None of the surveyed tools uses either protocol for a zoomed pen mode; that is an open design point.
- Keystrokes for DemoType: `virtual-keyboard` (wlroots, used by wtype), `/dev/uinput` (ydotool, needs privileges), or libei through the RemoteDesktop portal (GNOME, KDE). These three routes are from general knowledge of the stack and the tools' descriptions; I did not verify the libei route in this pass. Blocking real keystrokes while typing has no portable route; an overlay with exclusive keyboard focus (layer-shell `keyboard_interactivity = exclusive`, or a modal grab in a GNOME extension) can swallow them while the overlay is up.
- wayscriber's optional keystroke HUD reads `/dev/input` through libinput and udev and needs the `input` group (Cargo feature `input-monitor`).

### 3.6 Recording

Portal ScreenCast plus PipeWire is the generic source. Kooha (GStreamer) and OBS use it. Spectacle uses KWin's `zkde_screencast_unstable_v1` and PipeWire encoders. wlroots tools (wf-recorder, wl-screenrec) use screencopy directly. None of them composite ZoomIt-style annotations, because the overlay is just another surface in the captured output. No tool I found offers a post-record trim editor or a webcam picture-in-picture on Linux.

### 3.7 Tray

wayscriber uses StatusNotifierItem through `ksni`. GNOME hides tray icons unless the AppIndicator extension is installed (Gromit-MPX NEWS mentions `gnome-shell-extension-appindicator`).

## 4. Gap analysis

### 4.1 Why nothing covers ZoomIt one to one across GNOME, KDE, X11 and Wayland

1. Only one ZoomIt-shaped tool is alive on Wayland, and it covers part of the feature set. wayscriber has static zoom, draw, text, highlighter, blur, snip and OCR. It has no live zoom, no break timer, no recording and no DemoType, and it says so (its own comparison table lists the break timer as missing). Its zoom is wheel-at-cursor plus drag-pan on a frozen image, not ZoomIt's cursor-follows-view mapping. Zoomix has the closest keys but is X11 and Cinnamon only. The rest are zoom-only, draw-only or screenshot editors (tables 1.2 and 1.3).

2. GNOME, KDE and wlroots offer three different Wayland surfaces, and each surveyed tool picked one:
   - `wlr-layer-shell` exists in KWin and wlroots compositors but not Mutter. `wlr-screencopy` exists in wlroots-family compositors (Sway, Hyprland, niri, river and others) and `ext-image-copy-capture` in some of them (Sway 1.11, COSMIC), but neither exists in Mutter or KWin (wayland.app tables). So the cheap route (layer surface + screencopy) that woomer, wooz and wayscriber use works on Sway, Hyprland and their relatives only.
   - GNOME apps therefore either fall back to a fullscreen `xdg_toplevel` (wayscriber, no click-through, portal prompts for capture) or become a GNOME Shell extension (Draw On Gnome, Anatomico, hati), which runs nowhere else and needs a build per Shell version.
   - KDE has a private route (`KWin.ScreenShot2`, `zkde_screencast`, KGlobalAccel, effects) that only KDE apps and effects use. MagicScribe is KDE-only for that reason.

3. Capture semantics differ from ZoomIt's. ZoomIt takes one silent snapshot at the hotkey. On GNOME a third-party app gets a portal screenshot with a permission prompt and extra latency; on KDE it needs the restricted D-Bus interface or the portal; only wlroots allows silent capture. A shell extension can avoid the prompt but then cannot be shared with KDE.

4. Live zoom is compositor-only. KWin Zoom, GNOME Magnifier, Hyprland `cursor:zoom_factor` and Cinnamon's magnifier each have their own keys and tracking modes, and none can draw. An app-level live zoom would need a PipeWire ScreenCast stream rendered in a click-through overlay, which no surveyed tool does, and which GNOME cannot host as a plain app window.

5. Global hotkeys and click-through are the second GNOME gap. The GlobalShortcuts portal exists on GNOME 48 and later, KDE and Hyprland but not `xdg-desktop-portal-wlr`. Ubuntu 24.04 LTS (GNOME 46) predates it, so tools fall back to writing a GNOME custom keybinding (Gromit-MPX) or asking the user to bind a CLI command (wayscriber, Flameshot). ZoomIt's mode chords (Ctrl+1 to Ctrl+9) therefore cannot be assumed to register. Click-through overlays (LiveDraw, LiveZoom cursor pass-through, presenter draw-while-app-runs) are impossible for a plain GNOME window; wayscriber states this and points at an extension.

6. Input control has partial answers only. `pointer-constraints` and the staging `pointer-warp-v1` cover ZoomIt's ClipCursor and SetCursorPos on current Mutter, KWin, Sway and Hyprland, but nobody uses them yet. DemoType needs keystroke injection plus blocking of real keys; injection has three incompatible routes (virtual-keyboard, uinput, libei) and there is no portable global keyboard hook. No surveyed tool types a script into another window.

7. Recording, trim editor and webcam overlay exist separately (OBS, Kooha, Spectacle, wf-recorder) but not tied to the overlay, and I found none with ZoomIt's trim-on-save or 16:9 region lock. Break timer apps (Stretchly, Workrave, Safe Eyes) are reminders, not a presenter countdown.

8. Fragmentation and upkeep. Draw On Your Screen 2 and Pensela are archived, wooz is archived, Magnus and ScreenPen are stale, Ardesia's status is unclear, Electron tools segfault on some Wayland setups, and the X11-only tools (boomer, Zoomix, xzoom, KMag) run only through XWayland on GNOME 50 and Plasma 6.8 (where the session no longer exists). Gromit-MPX, the strongest X11 tool, cannot grab hotkeys under XWayland and draws over native Wayland windows only in a limited way.

Net: a 1:1 clone needs a per-desktop backend layer (capture, overlay, hotkey, live zoom, input) with a common core for drawing, zoom maths, timer, snip UI and settings. No existing project has that layer, and the closest (wayscriber) drops GNOME to a degraded mode.

### 4.2 Things the survey shows are feasible, so they should not be assumed impossible

- Silent static zoom on wlroots (wooz, woomer) with the compositor scaling through `wp_viewporter`.
- Frozen-image zoom and draw on GNOME and KDE through the portal (wayscriber, with prompts).
- GNOME live zoom and click-through drawing from inside the shell (GNOME Magnifier, Draw On Gnome).
- KDE live zoom and freehand marking inside KWin (Zoom and Mouse Mark effects), plus authorised capture and recording through the `.desktop` declarations Spectacle uses.
- Global shortcuts through the portal on GNOME 48+, KDE and Hyprland (wayscriber, MagicScribe), and through custom keybindings elsewhere (Gromit-MPX).
- Optional OCR of a region (wayscriber uses tesseract; ZoomIt uses Windows OCR).

## 5. Repos worth studying, with where to look

Ordered by how much a ZoomIt clone would reuse.

1. https://github.com/microsoft/PowerToys, `src/modules/ZoomIt/ZoomIt/` (MIT). Behaviour spec, not Linux code. `Zoomit.cpp`: `GetZoomedTopLeftCoordinates`, `GetAnimatedZoomSourceCoordinates`, the `WM_MOUSEWHEEL` and `WM_KEYDOWN` handlers, `DrawShape`, `BlendColors`; `ZoomAnimation.cpp`; `DemoType.cpp`; `SelectRectangle.cpp`; `PanoramaCapture.cpp` (algorithm comment). Full notes in `zoomit-reference.md`.
2. https://github.com/microsoft/ZoomitForMac (MIT, Swift). `Sources/ZoomItMacCore/Core/ModeCoordinator.swift` and `AppCommand.swift` (mode state machine), `Overlay/ZoomViewportController.swift` (`sourceRect(for:cursorLocation:)`, `adjustToMoveBoundary`), `Overlay/BreakTimerController.swift`, `App/DemoTypeController.swift`, `Capture/PanoramaStitcher.swift`, `Capture/DemoMirrorController.swift`. Shows how ZoomIt was split into portable and platform layers.
3. https://github.com/devmobasa/wayscriber (MIT, Rust). Capture: `src/backend/wayland/zoom/capture.rs` (`CaptureSession`, `finalize_capture_image`), `frozen/capture.rs`, `zoom/portal.rs`, `zoom/retry.rs`, `src/capture/portal.rs`. Overlay: `handlers/layer.rs`, `handlers/xdg.rs`, `overlay_passthrough.rs`. Hotkeys and daemon: `src/daemon/global_shortcuts.rs`, `src/daemon/protocol_v2/`, `docs/daemon-protocol-v2.md`. Zoom model: `zoom/view.rs` (`zoom_at_screen_point`, `clamp_offsets`). Region capture and OCR: `src/backend/wayland/state/region_capture/`.
4. https://github.com/bk138/gromit-mpx (GPL-2.0, C). `src/input.c`: `add_hotkeys_to_compositor()`, `remove_hotkeys_from_compositor()`, `release_grab()`; `src/main.c`: window setup near `main()` line 1247, `undo_compress()`, `undo_decompress()`, shape-combine fallback for non-composited X11.
5. https://github.com/daveprowse/Draw-On-Gnome (GPL-3.0, GJS). `areamanager.js` (`Main.wm.addKeybinding`, `Main.pushModal`/`popModal`, per-monitor areas, background versus modal parenting), `area.js` (`DrawingArea`, `vfunc_repaint`, `Shell.Screenshot` colour pick), `elements.js` (shape model), `files.js` (SVG and PNG save). Note licence is GPL-3.0.
6. https://gitlab.gnome.org/GNOME/gnome-shell, `js/ui/magnifier.js` (GPL-2.0-or-later): `class ZoomRegion`, `_createActors()` (Clutter clone of `Main.uiGroup`), `_setViewPort()`, `scrollToMousePos()`, and the mouse-tracking modes.
7. https://github.com/KDE/kwin, `src/plugins/zoom/zoom.cpp` (`prePaintScreen` tracking modes, `zoomTo`, `setTargetZoom`, KGlobalAccel actions) and `src/plugins/mousemark/mousemark.cpp` (`paintScreen`, `slotMouseChanged`, `createArrow`). KWin is GPL-2.0-or-later.
8. https://github.com/KDE/spectacle (mixed GPL and LGPL). `src/Platforms/ImagePlatformKWin.cpp` (ScreenShot2 with pipe fd), `VideoPlatformWayland.cpp` and `screencasting.cpp` (zkde_screencast, PipeWire encoders), `desktop/org.kde.spectacle.desktop.cmake` (`X-KDE-DBUS-Restricted-Interfaces`, `X-KDE-Wayland-Interfaces`, `X-KDE-Shortcuts`).
9. https://github.com/negrel/wooz (MIT, C, archived). `main.c`: `apply_zoom()`, `render_window()` (`wp_viewport_set_source`), `screencopy_frame_handle_*`. Smallest working Wayland static zoom.
10. https://github.com/tsoding/boomer (MIT, Nim). `src/screenshot.nim` (`newScreenshot`, XShm), `src/boomer.nim` (`main`, override-redirect GL window), `src/navigation.nim` (`Camera.update`, inertia), `src/frag.glsl` (flashlight).
11. https://github.com/Cartaz/Magicscribe (MIT, Python). `ui/native/global_shortcuts.py` (`PortalGlobalShortcutBackend`, `CreateSession`, `BindShortcuts`), `ui/native/layer_shell.py`, `ui/native/window_coordinator.py` (per-screen surfaces, input masks). A compact Qt recipe for KDE.
12. https://github.com/flameshot-org/flameshot (GPL-3.0). `src/utils/screengrabber.cpp`, `ScreenGrabber::freeDesktopPortal` (portal Screenshot call and the GNOME parent-window workaround), `src/utils/desktopinfo.cpp` (desktop detection). https://github.com/ksnip/ksnip: `src/backend/imageGrabber/ImageGrabberFactory.cpp` and the per-desktop grabbers.
13. https://github.com/Satty-org/Satty (MPL-2.0, Rust, GTK4) for a modern annotation tool set and clipboard handling; https://github.com/SeaDve/Kooha (GPL-3.0, Rust) for portal ScreenCast plus GStreamer recording on GNOME.

Licence note for reuse: MIT sources (PowerToys ZoomIt, ZoomitForMac, wayscriber, wooz, boomer, MagicScribe) can be reused if the licence notice is kept. GPL sources (Gromit-MPX, Draw On Gnome, KWin, Spectacle, Flameshot) are for study unless the new app is also GPL-compatible.
