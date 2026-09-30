# Contributing to Lupka

Thanks for looking at the code. Lupka is a small project maintained in spare time, so a few minutes spent on the points below saves everyone a round trip. Everyone taking part is expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

Contributions are accepted under the project licence, GPL-3.0-or-later. There is no contributor agreement to sign.

## Before you start

Lupka copies Sysinternals ZoomIt, so behaviour follows ZoomIt. When you are unsure what a key, step size, timing or edge case should do, [`docs/research/zoomit-reference.md`](docs/research/zoomit-reference.md) is the reference. The deliberate differences are listed in the README under "Differences from ZoomIt". A change that adds another one needs a reason.

The rest of [`docs/research/`](docs/research/) is background reading:

- `landscape.md`: the survey of existing Linux tools.
- `platform-apis.md`: notes on the portals, KWin, GNOME and X11 interfaces Lupka uses.
- `naming.md`: the naming shortlist.
- `zoomit-reference.md`: ZoomIt's behaviour and bindings, taken from the PowerToys source.

For anything bigger than a bug fix, open an issue or a discussion first. A feature that ZoomIt does not have is a harder sell than one it does.

## Setting up

Lupka is C++17 and Qt 6 (6.2 or newer). On Ubuntu 24.04 or newer:

```sh
sudo apt install cmake ninja-build g++ pkg-config dpkg-dev \
    qt6-base-dev qt6-wayland \
    libxcb1-dev libxcb-keysyms1-dev libxcb-xtest0-dev libglib2.0-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-pipewire gstreamer1.0-plugins-good
```

On Fedora:

```sh
sudo dnf install cmake ninja-build gcc-c++ pkgconf-pkg-config \
    qt6-qtbase-devel qt6-qtwayland libxcb-devel xcb-util-keysyms-devel \
    glib2-devel gstreamer1-devel gstreamer1-plugins-base-devel pipewire-gstreamer
```

The README lists the optional run-time packages (H.264 and AAC encoders, `gstreamer1.0-x` for X11 recording, `wl-clipboard`, `grim`).

## Building

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
build/lupka --background
```

Directories named `build*/` are ignored by git, so you can keep several (the test scripts use `build-kde` and `build-gnome`). The core library is compiled with `-Wall -Wextra`; please do not add warnings.

To see what the daemon is doing, turn the `app.*` logging categories on:

```sh
QT_LOGGING_RULES="app.*=true" build/lupka --background
```

Quit an installed or older Lupka first (`lupka quit`). While one is running, `build/lupka zoom` hands the action to that instance instead of starting yours. A development build registers its hotkeys with your real desktop; `build/lupka --unregister-shortcuts` removes them again, along with the login entry and, on KDE, the desktop entry Lupka wrote.

## Unit tests

```sh
ctest --test-dir build --output-on-failure
```

The unit tests use Qt Test with `QT_QPA_PLATFORM=offscreen`, so they need no display. They cover the pure logic: zoom maths, key names, annotations, settings, the capture fallback chain, the break timer and the DemoType script parser. To add one, create `tests/test_<name>.cpp` and add `<name>` to the list at the top of `tests/CMakeLists.txt`.

## End-to-end tests

The end-to-end scripts start the app on a virtual display and compare screenshots with `scripts/testpattern.png`. Each one uses its own X server, D-Bus session and config directory, so none of them touches your running desktop or your settings. Each prints `PASS` or `FAIL` for every check, a final count, and (for the host scripts) an `artifacts:` line naming a directory with the screenshots and logs. The exit status is non-zero if anything failed.

### X11 (Xvfb)

```sh
scripts/e2e-x11.sh [path/to/binary] [--wm mutter]
```

The binary defaults to `build/lupka`. Needs `xvfb`, `x11-utils` (for `xdpyinfo`), `xdotool`, `feh`, `imagemagick`, `xclip`, `zenity`, `ffmpeg` (for `ffprobe`), `dbus-daemon` (for `dbus-run-session`), `gstreamer1.0-plugins-good`, `gstreamer1.0-plugins-ugly` (x264), `gstreamer1.0-x` (X11 recording) and a font such as `fonts-dejavu-core`. `--wm mutter` also needs `mutter`.

### Nested GNOME Shell (GNOME 46)

```sh
scripts/e2e-gnome-wayland.sh [path/to/binary]
```

Runs `gnome-shell --nested` as a Wayland compositor inside Xvfb, with its own D-Bus session, PipeWire, portals and media-keys daemon. Needs a host whose `gnome-shell` still has `--nested`, such as Ubuntu 24.04 (GNOME 46), with `gnome-shell`, `xdg-desktop-portal`, `xdg-desktop-portal-gnome`, `gnome-settings-daemon`, `pipewire`, `wireplumber`, `wl-clipboard`, `libglib2.0-bin` (`gdbus`, `gsettings`), plus the X11 suite's tools. It expects the Ubuntu paths under `/usr/libexec`. `scripts/nested-gnome.sh WORKDIR` starts the same nested session and leaves it running, which is handy for trying things by hand.

### KDE Plasma 6 and GNOME 50 (podman)

These run in Fedora 44 containers with rootless podman. The container images hold everything the tests need; the source is mounted read-only, and the app is built inside the container (so these runs also check the code against current Qt). Builds go to `build-kde/` and `build-gnome/` in your checkout.

```sh
podman build -t lupka-kde -f scripts/containers/kde.Containerfile scripts/containers
KDE_MODE=single scripts/e2e-kde-wayland.sh     # also: dual, hidpi

podman build -t lupka-gnome -f scripts/containers/gnome.Containerfile scripts/containers
scripts/e2e-gnome50.sh
```

The KDE run uses KWin's Wayland backend nested in Xvfb, and KWin only takes screenshots with OpenGL compositing, so pass a GPU through (the scripts add `--device /dev/dri` when `/dev/dri/renderD128` exists). `KDE_MODE` picks one monitor, two monitors side by side, or a HiDPI monitor. The GNOME 50 run shows `gnome-shell --devkit` in Xvfb and sends input through Mutter's RemoteDesktop API (`scripts/containers/rdinput.py`).

## Code style

Follow what is already in `src/`.

- C++17, Qt 6. No new dependencies without discussing them first: each one lands in the Debian package metadata and in the README.
- 4-space indent, no tabs, lines up to about 120 columns.
- The opening brace goes on its own line for functions and classes, and on the same line for `if`, `for`, `while`, `switch` and namespaces.
- Logging goes through Qt logging categories named `app.*`, declared with `Q_LOGGING_CATEGORY(lcName, "app.name")`. Use `qCInfo` for facts a bug report needs (which backend was chosen, recording started), `qCDebug` for chatty detail and `qCWarning` for failures. Do not add `qDebug()` or `std::cerr` output.
- Comments explain why, not what. If a workaround exists for a particular compositor or portal version, say which.
- Match the naming around you: `m_` for members, `kName` for constants, anonymous namespaces for file-local helpers, `#pragma once` in headers.
- New source files start with the two SPDX lines the existing ones use (`SPDX-FileCopyrightText` with the year and your name, and `SPDX-License-Identifier: GPL-3.0-or-later`).

## Commit messages

- A short summary line of about 72 characters or fewer, in the imperative where it reads naturally (Fix, Add, Drop), with no full stop.
- A blank line.
- A body, wrapped at about 72 columns, that says why the change is needed and what behaviour changes. The diff already shows what changed. Bullet lists are fine for several related fixes.

`git log` has plenty of examples. Keep unrelated changes in separate commits.

## Pull requests

- One topic per pull request. Do not mix a fix with a reformat or an unrelated cleanup.
- Say what you tested, on which desktop and session type. Every desktop or backend you touched needs either a test or a short manual test note (for example "GNOME 46, Wayland: zoomed on two monitors, Esc closed both overlays"). The pull request template has a checklist. If you cannot test a desktop that your change touches, say so and mention it in the description.
- Add or update a unit test when the change is in logic that the unit tests cover, and an end-to-end check when the change affects what ends up on the screen and the suites can see it.
- Update the README when behaviour, hotkeys or install steps change, and add a line under `[Unreleased]` in `CHANGELOG.md`.
- Build without new warnings and make sure `ctest` passes before you push.
- Expect review comments, and expect to be asked for small changes before a merge.

## Reporting desktop-specific bugs

Most Lupka bugs depend on the desktop, so the bug report form asks for details that matter:

- Lupka's version (`lupka --version`) and how you installed it.
- The distribution and version.
- The desktop and its version: `gnome-shell --version`, `plasmashell --version`, or the compositor's name and version.
- The session type: `echo $XDG_SESSION_TYPE`. Wayland and X11 use different capture and hotkey paths, so this is the first thing needed.
- A log with the `app.*` categories on. Quit Lupka, start it again from a terminal and reproduce the problem:

  ```sh
  lupka quit; QT_LOGGING_RULES="app.*=true" lupka --background
  ```

  The log says which hotkey backend and which capture backend were used, which is usually the quickest clue.
- For multi-monitor and HiDPI problems, the monitor layout and scale factors.

Please search the existing issues first. Security problems go through the process in [SECURITY.md](SECURITY.md), not a public issue.
