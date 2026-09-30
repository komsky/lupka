# Lupka

Zoom, draw and snip on the screen, the ZoomIt way, on Linux.

Lupka ("magnifying glass" in Polish) copies Sysinternals ZoomIt closely enough that ZoomIt habits carry over. Hotkeys, zoom steps, the zoom animation and the drawing keys all match. It runs on GNOME and KDE Plasma under Wayland, on any X11 desktop, and on wlroots compositors with `grim`.

## What it does

| Hotkey | ZoomIt name | What happens |
|---|---|---|
| Ctrl+1 | Zoom | Freezes the screen and zooms in 2x around the pointer. Move the mouse to pan, scroll or press Up/Down to change the zoom (1x to 256x). Left-click to start drawing on the frozen image. Esc or right-click leaves. |
| Ctrl+2 | Draw | Freezes the screen at 1:1 and goes straight into drawing. Press Ctrl+2 again to leave. |
| Ctrl+Shift+4 | LiveDraw | Draws over the live desktop, without freezing it. |
| Ctrl+3 | Break | Full-screen countdown timer. Arrow keys or the wheel change the time. |
| Ctrl+4 | LiveZoom | Toggles the desktop's own magnifier (GNOME or KWin). Ctrl+Up/Down change its zoom while it is on. |
| Ctrl+5 | Record | Records the monitor under the pointer. Press any record hotkey again to stop. |
| Ctrl+Shift+5 | Record region | Drag a rectangle, then recording starts. |
| Ctrl+Alt+5 | Record window | Records the window under the pointer. On Wayland you pick the window in the share dialog. |
| Ctrl+6, Super+Shift+S | Snip | Drag a rectangle; the image goes to the clipboard. |
| Ctrl+Shift+6 | Snip to file | Same, but asks where to save the image. |
| Ctrl+7 | DemoType | Types the next snippet of your script into the focused window. Esc stops it. |
| Ctrl+Shift+7 | DemoType back | Steps back one snippet. |

You can change or turn off any hotkey in Settings. When a zoom is active, Snip crops what you see at the current magnification.

### Drawing keys (inside Zoom, Draw and LiveDraw)

| Key | Action |
|---|---|
| R G B O Y P W K | Red, green, blue, orange, yellow, pink, white, black pen |
| Shift + colour | Highlighter in that colour |
| X / Shift+X | Blur pen / strong blur pen |
| Hold Shift while dragging | Straight line |
| Hold Ctrl | Rectangle |
| Hold Tab | Ellipse |
| Hold Ctrl+Shift | Arrow (the head goes where you started) |
| Ctrl+wheel, Ctrl+Up/Down | Pen width (2 to 40) |
| T / Shift+T | Type text, left or right aligned. The wheel or Up/Down changes the font size. Esc ends typing. |
| E | Erase all drawings |
| Ctrl+Z | Undo (32 steps) |
| Ctrl+W / Ctrl+K | Switch to a whiteboard / blackboard |
| Space | Centre the pointer (X11 only) |
| Ctrl+C / Ctrl+S | Copy / save the whole view |
| Ctrl+Shift+C / Ctrl+Shift+S | Drag a rectangle, then copy / save it |
| Right-click | Leave drawing mode, back to panning |
| Esc | Close |

Lupka keeps drawings in screen coordinates, so they stay in place when you zoom or pan afterwards. The save dialog offers the zoomed view or the actual-size region, as PNG or JPEG.

### DemoType

Point Settings → DemoType at a text file in ZoomIt's format. Snippets are separated by `[end]`. A snippet wrapped in `[paste]` ... `[/paste]` is pasted instead of typed. Each Ctrl+7 types the next snippet into whatever window has focus. On X11 the keys go through XTest; on Wayland through the RemoteDesktop portal, which asks for permission once. Characters that can't be typed on the current keyboard layout are pasted.

### Recording

Recordings go to `~/Videos` (configurable) as MP4 (H.264) when `x264enc` is installed, otherwise WebM (VP8). System audio is recorded from the default output when "Record system sound" is ticked. On Wayland the frames come from the ScreenCast portal, so the desktop shows its screen-sharing indicator while recording.

## Install

### Debian and Ubuntu package

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build-release
(cd build-release && cpack -G DEB)
sudo apt install ./build-release/lupka_0.1.0_amd64.deb
```

Start **Lupka** from the application menu once. It sits in the tray, registers its hotkeys and starts at login (turn that off in Settings → General).

To remove it cleanly:

```sh
lupka --unregister-shortcuts
sudo apt remove lupka
```

### Build dependencies

Ubuntu 24.04 or newer:

```sh
sudo apt install cmake ninja-build g++ pkg-config \
    qt6-base-dev qt6-wayland \
    libxcb1-dev libxcb-keysyms1-dev libxcb-xtest0-dev libglib2.0-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-pipewire gstreamer1.0-plugins-good
```

Optional at run time: `gstreamer1.0-plugins-ugly` (H.264 recording), `gstreamer1.0-x` (X11 recording), `wl-clipboard` (DemoType paste fallback on Wayland), `grim` (capture on wlroots compositors).

Fedora: `qt6-qtbase-devel qt6-qtwayland libxcb-devel xcb-util-keysyms-devel glib2-devel gstreamer1-devel gstreamer1-plugins-base-devel pipewire-gstreamer`.

## Desktop notes

**GNOME (Wayland).** Hotkeys are GNOME custom shortcuts, so they show up under Settings → Keyboard → Custom Shortcuts and GNOME runs `lupka <action>` when you press them. The screen image comes from a ScreenCast portal session. The first zoom asks which screen to share; press Share once and Lupka keeps the permission, so later zooms don't ask. Each capture briefly lights the screen-sharing indicator in the top bar. If the ScreenCast portal isn't available, Lupka falls back to the Screenshot portal, which flashes the screen and plays the shutter sound.

**KDE Plasma (Wayland).** Hotkeys are registered with KDE's global shortcuts and appear in System Settings → Shortcuts → Lupka. Captures use KWin's screenshot interface with no dialog and no flash. KWin only allows this for applications with an installed desktop file, so Lupka writes `~/.local/share/applications/io.github.komsky.Lupka.desktop` on first start if the package's copy isn't there.

**X11 (any desktop).** Hotkeys are grabbed directly and the screen is read from the X server. Nothing asks for permission.

**Other Wayland compositors.** Hotkeys go through the GlobalShortcuts portal where the compositor has one; otherwise bind `lupka zoom`, `lupka draw` and so on to keys in your compositor's config. Captures use `grim` if it is installed, then the Screenshot portal.

## Command line

```
lupka                 start in the background, or open Settings if already running
lupka zoom            same as pressing Ctrl+1 (also draw, livedraw, snip, snip-save,
                      break, livezoom, record, record-region, record-window,
                      demotype, demotype-back, settings, quit)
lupka --unregister-shortcuts
```

Settings live in `~/.config/lupka/lupka.ini`.

## Differences from ZoomIt

Not implemented: panorama screenshots, DemoMirror, webcam overlay, the recording trim editor, and DemoType's "user-driven" mode (where each key you press types the next character). OCR snip is not there either.

Deliberate changes: Ctrl+2 closes the draw overlay when pressed again. Super+Shift+S is bound to Snip by default, as the Windows screenshot shortcut. On GNOME Wayland the first capture needs the share dialog described above, and the pointer can't be moved by the app, so Space (centre pointer) only works on X11.

## Development

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
ctest --test-dir build                 # unit tests
scripts/e2e-x11.sh                     # Xvfb, drives the app with xdotool
scripts/e2e-x11.sh build/lupka --wm mutter
scripts/e2e-gnome-wayland.sh           # nested gnome-shell (host GNOME) in Xvfb
```

The end-to-end scripts never touch the running desktop: each one uses its own X server, D-Bus session and config directory, and checks the result by comparing screenshots.

KDE Plasma 6 and GNOME 50 run in Fedora containers (podman, rootless):

```sh
podman build -t lupka-kde -f scripts/containers/kde.Containerfile scripts/containers
KDE_MODE=single scripts/e2e-kde-wayland.sh     # also: dual, hidpi
podman build -t lupka-gnome -f scripts/containers/gnome.Containerfile scripts/containers
scripts/e2e-gnome50.sh
```

`docs/research/` has the survey of existing Linux tools, the ZoomIt behaviour reference (taken from the PowerToys source), the platform API notes and the naming shortlist.

## Licence

MIT. See `LICENSE`.
