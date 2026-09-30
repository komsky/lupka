# Platform APIs for a ZoomIt clone on Linux (research notes)

Date: 2026-09-30. Scope: C++17 / Qt 6 host (unsandboxed) daemon, GNOME 46 to 51, KDE Plasma 5.27 and 6.x,
X11 and Wayland, plus wlroots (sway) and Hyprland.

Method: I read the upstream sources at release tags (blobless git clones of xdg-desktop-portal,
xdg-desktop-portal-gnome, gnome-shell, mutter, gnome-settings-daemon, gnome-control-center,
gnome-session, gsettings-desktop-schemas, kwin, xdg-desktop-portal-kde, kglobalaccel, kglobalacceld,
kservice, layer-shell-qt, xdg-desktop-portal-wlr, xdg-desktop-portal-hyprland, qtbase, qtwayland,
pipewire) and checked Ubuntu package versions on packages.ubuntu.com. Links below point at the exact
tag I read. Anything I could not confirm from source or a primary document is marked **UNVERIFIED**.

## Target distro matrix

| Release | gnome-shell | xdg-desktop-portal (frontend) | xdp-gnome | Qt 6 | Notes |
|---|---|---|---|---|---|
| Ubuntu 24.04 (noble) | 46.0 | 1.18.4 | 46.2 | 6.4.2 | No host Registry, no GlobalShortcuts on GNOME. `liblayershellqtinterface-dev` is 5.27.11 (Qt 5 only). |
| Ubuntu 25.10 (questing) | 49.0 | 1.20.3 | 49.0 | 6.9.2 | Registry available; GNOME GlobalShortcuts available. |
| Ubuntu 26.04 (resolute) | 50.1 | 1.21.1 | 50.0 | 6.10.2 | GlobalShortcuts v2 + `activation_token`; frontend refuses GlobalShortcuts for app id `""`. layer-shell-qt 6.6.4 (Qt 6). Qt auto-registers with the host Registry. |
| Kubuntu 24.04 | Plasma 5.27.11 | 1.18.4 | (kde 5.27) | 6.4.2 | kglobalaccel 5 daemon, KWin ScreenShot2 v4. |
| Plasma 6.x distros | 6.0 to 6.7 | varies | (kde 6.x) | 6.6+ | ScreenShot2 v4 (6.0 to 6.6), v5 (6.7+). |

Mutter 50 no longer contains the X11 compositing-manager backend (`src/backends/x11/cm/` is gone at tag 50.0),
so a GNOME X11 session only matters for GNOME 46 to 49. KDE still ships an X11 session.

---

## 1. Screen capture on GNOME Wayland

### 1a. Does the portal frontend show a permission dialog for host apps? Yes, once.

There is **no host-app exemption** in `src/screenshot.c` of xdg-desktop-portal 1.18.x, 1.20.x or 1.22.x.
For a non-interactive request the frontend looks up table `screenshot`, id `screenshot`, keyed by the app id.
Host apps without an identifiable app id all share the key `""`.

[xdg-desktop-portal 1.18.4 `src/screenshot.c` L214-L297](https://github.com/flatpak/xdg-desktop-portal/blob/1.18.4/src/screenshot.c#L214-L297):

```c
  if (xdp_dbus_impl_screenshot_get_version (impl) < 2)
    goto query_impl;

  permission = get_permission_sync (app_id, PERMISSION_TABLE, PERMISSION_ID);
  ...
  if (!interactive && permission != PERMISSION_YES)
    {
      ...
      else
        {
          /* Note: this will set the wallpaper permission for all unsandboxed
           * apps for which an app ID can't be determined.
           */
          g_assert (xdp_app_info_is_host (request->app_info));
          title = g_strdup (_("Allow Applications to Take Screenshots?"));
          subtitle = g_strdup (_("An application wants to be able to take screenshots at any time."));
        }
      ...
      if (permission == PERMISSION_UNSET)
        set_permission_sync (app_id, PERMISSION_TABLE, PERMISSION_ID, access_response == 0 ? PERMISSION_YES : PERMISSION_NO);
```

1.20.3 is the same logic. 1.21.0+ moved it into `check_non_interactive_permission_in_thread()`
([1.22.1 L246](https://github.com/flatpak/xdg-desktop-portal/blob/1.22.1/src/screenshot.c#L246)) and added an
`ask` state (dialog each time, nothing stored) plus the `modal` option. Still no host exemption.

What this means in practice:

- The first non-interactive call shows an Access dialog ("Allow Applications to Take Screenshots?", or
  "Allow <Name> to Take Screenshots?" when the app id resolves to a `.desktop` file). After "Allow",
  later calls are silent at the portal level. "Deny" is stored too and every later call returns response 2.
- With app id `""` the grant is shared by **every** unidentified host app, in both directions.
- The check is skipped entirely when the backend's `version` property is < 2 (xdp-wlr is version 1).
- The permission can be pre-seeded by any unsandboxed process (only do this after an explicit in-app opt-in):
  - `flatpak permission-set screenshot screenshot <app_id-or-empty> yes`, or
  - D-Bus `org.freedesktop.impl.portal.PermissionStore` at `/org/freedesktop/impl/portal/PermissionStore`,
    method `SetPermission(s table, b create, s id, s app, as permissions)` with
    `("screenshot", true, "screenshot", "<app_id>", ["yes"])`
    ([interface XML](https://github.com/flatpak/xdg-desktop-portal/blob/1.18.4/data/org.freedesktop.impl.portal.PermissionStore.xml)).

How the frontend decides a host app's id:

- 1.18.x: from the systemd user unit of the caller's PID. `app[-<launcher>]-<ApplicationID>-<RANDOM>.scope`
  or `app[-<launcher>]-<ApplicationID>[@<RANDOM>].service`; anything else gives `""`
  ([1.18.4 `src/xdp-utils.c` L153-L230](https://github.com/flatpak/xdg-desktop-portal/blob/1.18.4/src/xdp-utils.c#L153-L230)).
  gnome-shell and GNOME 46 autostart create `app-gnome-<desktop-id>-<pid>.scope`
  ([gnome-session 46 `gsm-autostart-app.c` L953](https://gitlab.gnome.org/GNOME/gnome-session/-/blob/46.0/gnome-session/gsm-autostart-app.c#L953)).
  Launched from a terminal you get `""`, or worse, the terminal's own id if the terminal does not create a
  per-child scope.
- 1.19.3+ (first tag containing `src/registry.c`): the host app can register itself, see §4a.

### 1b. What xdp-gnome does for `interactive=false`: flash, shutter sound, file in ~/Pictures

xdp-gnome (46.2 through 51.0, unchanged) creates a `ScreenshotDialog` that immediately calls
`org.gnome.Shell.Screenshot.Screenshot(include_cursor, flash=TRUE, filename="Screenshot")`. The
"Share this screenshot?" dialog is skipped when the frontend passed `permission_store_checked=true`
(which it always does after the permission check above).

[xdp-gnome 46.2 `src/screenshotdialog.c` L95-L99, L247-L256, L443-L446](https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/blob/46.2/src/screenshotdialog.c#L247-L256):

```c
static GActionEntry entries[] = {
  { "grab", NULL, "s", "'screen'", change_grab },
  { "pointer", NULL, NULL, "false", NULL },
  ...
      org_gnome_shell_screenshot_call_screenshot (dialog->shell,
                                                  include_pointer,
                                                  TRUE,
                                                  "Screenshot",
  ...
  dialog->skip_dialog = permission_store_checked;
```

gnome-shell then resolves the relative name into the XDG Pictures dir (falls back to `$HOME`), flashes
and plays the `screen-capture` sound:

[gnome-shell 46.0 `js/ui/screenshot.js` L2497-L2560](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/46.0/js/ui/screenshot.js#L2497-L2560):

```js
    *_resolveRelativeFilename(filename) {
        filename = filename.replace(/\.png$/, '');
        let path = [
            GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_PICTURES),
            GLib.get_home_dir(),
        ].find(p => p && GLib.file_test(p, GLib.FileTest.EXISTS));
        ...
        yield Gio.File.new_for_path(GLib.build_filenamev([path, `${filename}.png`]));
        for (let idx = 1; ; idx++) {
            yield Gio.File.new_for_path(GLib.build_filenamev([path, `${filename}-${idx}.png`]));
    ...
    _flashAsync(shooter) {
        return new Promise((resolve, _reject) => {
            shooter.connect('screenshot_taken', (s, area) => {
                const flashspot = new Flashspot(area);
                flashspot.fire(resolve);
                global.display.get_sound_player().play_from_theme(
                    'screen-capture', _('Screenshot taken'), null);
```

Same in 47 to 50; 51 plays `${global.datadir}/sounds/screen-capture.oga` via `play_from_file` instead.

Summary for GNOME:

| Question | Answer |
|---|---|
| Flash? | Yes, always (flash is hard-coded `TRUE`). The flash fires on `screenshot_taken`, i.e. after pixels were read, so it is not in the image. |
| Sound? | Yes, via `MetaSoundPlayer`, which honours `org.gnome.desktop.sound event-sounds` ([mutter 50 `meta-sound-player.c` L28, L205](https://gitlab.gnome.org/GNOME/mutter/-/blob/50.0/src/core/meta-sound-player.c#L205)). |
| Cursor? | Not included (`pointer` action defaults to `false`). |
| File location | `$(xdg-user-dir PICTURES)/Screenshot.png`, or `Screenshot-1.png`, `-2`... if taken. Not the `Screenshots/` subfolder. |
| URI returned | For host apps the frontend returns the raw `file://` URI (no document portal) ([1.18.4 L119-L122](https://github.com/flatpak/xdg-desktop-portal/blob/1.18.4/src/screenshot.c#L119-L122)). |
| Delete it? | **Yes.** Nothing cleans it up; delete right after decoding. |
| Image geometry | Whole stage (all monitors) in one PNG, rendered at the maximum scale of the views it covers (`clutter_stage_get_capture_final_size`, [shell-screenshot.c L328-L331](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/50.0/src/shell-screenshot.c#L328-L331)). Crop per monitor as `logical_rect * scale`. |

### 1c. Latency

**UNVERIFIED (no published measurement found).** The path is: two D-Bus hops, gnome-shell paints the
stage into a buffer, encodes a full-desktop PNG on the compositor side, writes it to disk, then we decode
it. My estimate is 150 to 400 ms at 1080p and 400 ms to 1 s for 4K or dual 4K, dominated by PNG
encode and decode. Measure it on real hardware before committing to it as the only path.

### 1d. Can third parties call `org.gnome.Shell.Screenshot` directly? No.

Since GNOME 41 (`dd2cd6286 screenshot: Restrict callers`, 2021-06-16) the non-interactive methods check the
caller against a list of well-known bus names:

[gnome-shell 46.0 `js/ui/screenshot.js` L2431-L2436](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/46.0/js/ui/screenshot.js#L2431-L2436):

```js
        this._senderChecker = new DBusSenderChecker([
            'org.gnome.SettingsDaemon.MediaKeys',
            'org.freedesktop.impl.portal.desktop.gtk',
            'org.freedesktop.impl.portal.desktop.gnome',
            'org.gnome.Screenshot',
        ]);
```

GNOME 49 removed `org.gnome.Screenshot` (`c9944dbf9`) and the gtk portal (`c517b7e39`), leaving only
`org.gnome.SettingsDaemon.MediaKeys` and `org.freedesktop.impl.portal.desktop.gnome`
([49.0 L2447-L2450](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/49.0/js/ui/screenshot.js#L2447-L2450)).
The checker compares the caller's unique name with the current owners of those names, and is bypassed
only in unsafe mode ([`js/misc/util.js` L399-L404](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/50.0/js/misc/util.js#L399-L404)):

```js
    async checkInvocation(invocation) {
        if (global.context.unsafe_mode)
            return;
```

Unsafe mode is a developer toggle (Looking Glass). Owning `org.gnome.Screenshot` ourselves would pass the
check on 46 to 48 only, is a hack, and is gone in 49. Not a product path.

### 1e. ScreenCast portal with persistence as a flash-free alternative

Flow and keys are in §8. GNOME behaviour:

- **Dialog first time only.** xdp-gnome's `handle_start()` skips the dialog when restore data is valid
  ([xdp-gnome 46.2 `src/screencast.c` L630-L673](https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/blob/46.2/src/screencast.c#L664)):
  ```c
  if (!restore_stream_from_data (screen_cast_session))
    {
      ScreenCastDialogHandle *dialog_handle;
      dialog_handle = create_screen_cast_dialog (...);
  ```
  Monitors are matched by monitor match string; if the monitor is gone the dialog comes back.
- **Tokens are single use.** The frontend deletes the stored data when a token is consumed and returns a
  new `restore_token` in the `Start` response; persist the new one every time
  ([1.18.4 `src/restore-token.c`](https://github.com/flatpak/xdg-desktop-portal/blob/1.18.4/src/restore-token.c)).
  Works for app id `""` too (stored under that id).
- **Indicator: yes, and it lingers.** gnome-shell shows the orange screen-sharing indicator for every
  non-recording remote-access handle and keeps it visible for at least 5 s
  ([`js/ui/status/remoteAccess.js` L12](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/50.0/js/ui/status/remoteAccess.js#L12)):
  `const MIN_SHARED_INDICATOR_VISIBLE_TIME_US = 5 * GLib.TIME_SPAN_SECOND;`
  Keeping one session open permanently avoids the per-capture cost but shows the indicator permanently.
- Latency per capture (new session + first PipeWire frame): **UNVERIFIED**, estimate 100 to 300 ms.

Also possible, but not recommended: `org.gnome.Mutter.ScreenCast` (the private API behind the portal)
has no sender restriction in mutter 50 (`meta-dbus-session-manager.c` only checks an inhibit counter).
It needs no dialog but still triggers the indicator, and it is an unstable private interface. Mutter 48
added a `MetaDbusAccessChecker` helper (`1c34794b1`), currently used only for Orca's keyboard monitor, so
a lock-down later is plausible.

A GNOME Shell extension could take silent in-memory screenshots, read the cursor position and grab keys,
but it needs the user to install and enable it and breaks across shell versions. Optional power-user path.

### D-Bus reference (Screenshot portal)

- Bus `org.freedesktop.portal.Desktop`, path `/org/freedesktop/portal/desktop`, iface `org.freedesktop.portal.Screenshot`.
- `Screenshot(s parent_window, a{sv} options) -> o handle`. Options: `handle_token s`, `modal b`,
  `interactive b`, and `target u` (version 3, frontend 1.21.2+ with a v3 backend).
- Result: `org.freedesktop.portal.Request::Response(u response, a{sv} results)`, `results["uri"]` (s).
  Response 0 = ok, 1 = cancelled, 2 = error/denied.
- Request path is predictable: `/org/freedesktop/portal/desktop/request/<SENDER>/<handle_token>`, where
  `<SENDER>` is the unique name without the leading `:` and with `.` replaced by `_`. Subscribe to
  `Response` on that path **before** calling the method.

---

## 2. Screen capture on KDE

### KWin `org.kde.KWin.ScreenShot2`

- Service `org.kde.KWin.ScreenShot2`, path `/org/kde/KWin/ScreenShot2`, iface `org.kde.KWin.ScreenShot2`
  ([kwin v6.7.5 XML](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/plugins/screenshot/org.kde.KWin.ScreenShot2.xml);
  in 5.27 the same file lives under `src/effects/screenshot/`).
- Property `Version` (u): **4** in 5.27 and 6.0 to 6.6, **5** from 6.7.0 (adds `hide-caller-windows`).

| Method | Signature (in) | Out |
|---|---|---|
| `CaptureScreen` | `s name, a{sv} options, h pipe` | `a{sv} results` |
| `CaptureActiveScreen` | `a{sv} options, h pipe` | `a{sv}` |
| `CaptureWorkspace` | `a{sv} options, h pipe` | `a{sv}` |
| `CaptureArea` | `i x, i y, u width, u height, a{sv} options, h pipe` | `a{sv}` |
| `CaptureWindow` | `s handle, a{sv} options, h pipe` | `a{sv}` |
| `CaptureActiveWindow` | `a{sv} options, h pipe` | `a{sv}` |
| `CaptureInteractive` | `u kind (0 window, 1 screen), a{sv} options, h pipe` | `a{sv}` |

Options: `include-cursor` (b, default false), `native-resolution` (b, default false: result is scaled to
logical size), `hide-caller-windows` (b, default true, v5+), and for windows `include-decoration` (false),
`include-shadow` (true). `name` for `CaptureScreen` is the output name, the same as `QScreen::name()`.

Results: `type` (s, always `"raw"`), `width` (u), `height` (u), `stride` (u), `format` (u, a
`QImage::Format` value), `scale` (d, v4+), `screen` (s, v4+) or `windowId` (s, v4+).

Pipe mechanism: the caller creates `pipe2(fds, O_CLOEXEC)`, passes the write end as `h`, closes its copy of
the write end after the call, and reads the read end to EOF. KWin sends the reply first and then writes the
raw pixels from a thread pool
([v6.7.5 `screenshotdbusinterface2.cpp` L194-L207](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/plugins/screenshot/screenshotdbusinterface2.cpp#L194-L207)):

```cpp
    results.insert(QStringLiteral("type"), QStringLiteral("raw"));
    results.insert(QStringLiteral("format"), quint32(image.format()));
    results.insert(QStringLiteral("width"), quint32(image.width()));
    results.insert(QStringLiteral("height"), quint32(image.height()));
    results.insert(QStringLiteral("stride"), quint32(image.bytesPerLine()));
    results.insert(QStringLiteral("scale"), double(image.devicePixelRatio()));
    QDBusConnection::sessionBus().send(m_replyMessage.createReply(results));

    auto writer = new ScreenShotWriter2(std::move(m_fileDescriptor), image);
```

Build the image as `QImage(width, height, QImage::Format(format))` and copy `height*stride` bytes in, the way
xdg-desktop-portal-kde does ([v6.7.5 `screenshotdialog.cpp` L32-L111](https://invent.kde.org/plasma/xdg-desktop-portal-kde/-/blob/v6.7.5/src/screenshotdialog.cpp#L94-L111)).
No encoding, no flash, no sound, no file. Latency **UNVERIFIED**, expected tens of milliseconds.

### How KWin authorises callers

Every Capture* except `CaptureInteractive` calls `checkPermissions()`:

[kwin v6.7.5 L245-L262](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/plugins/screenshot/screenshotdbusinterface2.cpp#L245-L262):

```cpp
    static bool permissionCheckDisabled = qEnvironmentVariableIntValue("KWIN_SCREENSHOT_NO_PERMISSION_CHECKS") == 1;
    ...
    const auto interfaces = KWin::fetchRestrictedDBusInterfacesFromPid(*pid);
    if (!interfaces.contains(s_dbusInterface)) {
        sendErrorReply(s_errorNotAuthorized, s_errorNotAuthorizedMessage);
```

The PID comes from `GetConnectionUnixProcessID` of the caller. Matching is **by executable path, not by app id**
([kwin v5.27.11 `src/utils/serviceutils.h`](https://invent.kde.org/plasma/kwin/-/blob/v5.27.11/src/utils/serviceutils.h),
identical logic in [v6.7.5](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/utils/serviceutils.h)):

```cpp
    const auto servicesFound = KApplicationTrader::query([&executablePath](const KService::Ptr &service) {
        const auto splitCommandList = QProcess::splitCommand(service->exec());
        ...
        return QFileInfo(splitCommandList.first()).canonicalFilePath() == executablePath;
    });
    ...
static inline QStringList fetchRestrictedDBusInterfacesFromPid(const uint pid)
{
    const auto executablePath = QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid)).symLinkTarget();
```

Consequences:

- `/proc/<pid>/exe` must equal the canonical path of the **first token of `Exec=`**. Use an absolute
  `Exec=/usr/bin/ubzoom` (or `/usr/local/bin/...` for source installs). A bare `Exec=ubzoom` is resolved
  relative to KWin's CWD and will not match. Symlinks are fine (both sides are canonicalised).
  A wrapper script as `Exec=` will not match the real binary.
- Candidate files come from KSycoca's application list (`KApplicationTrader::query`), i.e. `applications/`
  under `$XDG_DATA_HOME` and every `$XDG_DATA_DIRS` entry. The filter drops only entries not shown in the
  current desktop (`OnlyShowIn`/`NotShowIn`); `NoDisplay=true` is fine
  ([kservice v5.115.0 `kapplicationtrader.cpp` L65-L91](https://invent.kde.org/frameworks/kservice/-/blob/v5.115.0/src/services/kapplicationtrader.cpp#L65-L91)).
  A newly installed file is picked up once the sycoca cache is rebuilt (KDE does it on directory change;
  `kbuildsycoca6` / `kbuildsycoca5` forces it).
- Required key: `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2`.
- `KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1` in KWin's environment disables the check (development only).

5.27 vs 6.x: same service, path, methods and matching rule. 5.27 hosts it in the screenshot *effect*
(so it needs the effect loaded, which is the default), 6.x in a plugin. Version 4 vs 5 as above.

### xdg-desktop-portal-kde non-interactive Screenshot

No dialog of its own. It calls `CaptureWorkspace` (default "Full Screen") with `native-resolution=true`,
saves `~/Pictures/Screenshot_yyyyMMdd_hhmmss.png` and returns its URI
([v6.7.5 `src/screenshot.cpp` L80-L118](https://invent.kde.org/plasma/xdg-desktop-portal-kde/-/blob/v6.7.5/src/screenshot.cpp#L80-L118)).
No flash or sound. The file is left behind.

The frontend's Access dialog does apply from **Plasma 6.4**: xdp-kde only started exporting `version = 2`
for its Screenshot impl in commit `1572150a` ("fix: set xdg-desktop-portal version", first in v6.3.90, so
6.4.0). With 5.27 to 6.3 the frontend sees version < 2 and skips the permission check.
For our own app the direct ScreenShot2 path is better anyway.

---

## 3. Screen capture on wlroots (sway) and Hyprland

Both portal backends shell out to `grim`:

- xdg-desktop-portal-wlr: runs `grim /tmp/out.png` (fixed path) and returns `file:///tmp/out.png`;
  interface version 1, so the frontend does not ask for permission
  ([v0.8.4 `src/screenshot/screenshot.c` L13-L27, L120-L126](https://github.com/emersion/xdg-desktop-portal-wlr/blob/v0.8.4/src/screenshot/screenshot.c#L120-L126)).
- xdg-desktop-portal-hyprland: runs `grim '<$XDG_RUNTIME_DIR/hypr/…>'`, deletes the previous file, version 2
  ([v1.4.1 `src/portals/Screenshot.cpp` L105-L139](https://github.com/hyprwm/xdg-desktop-portal-hyprland/blob/v1.4.1/src/portals/Screenshot.cpp#L124-L139)).
- Frontend 1.18 only exports the Screenshot portal at all when some backend provides
  `org.freedesktop.impl.portal.Access` ([1.18.4 `xdg-desktop-portal.c` L303-L327](https://github.com/flatpak/xdg-desktop-portal/blob/1.18.4/src/xdg-desktop-portal.c#L303-L327)),
  which on wlroots means xdg-desktop-portal-gtk must be installed.

Decision: call `grim` directly. `grim -o <output> -t ppm -` writes an uncompressed image to stdout (fast
to parse), `grim -t png -l 0 -` if PNG is wanted. Ubuntu ships grim 1.4.0 (noble and resolute), which uses
`wlr-screencopy`; sway and Hyprland support it. Output names match `QScreen::name()`.

---

## 4. Global hotkeys

### 4a. `org.freedesktop.portal.GlobalShortcuts`

Who implements it:

| Desktop | Implementation | Behaviour |
|---|---|---|
| GNOME 46, 47 | none (`globalshortcuts.c` first appears in xdp-gnome **48.0**) | not available |
| GNOME 48+ | xdp-gnome → `org.gnome.Settings.GlobalShortcutsProvider.BindShortcuts` (gnome-control-center) → `org.gnome.Shell.GrabAccelerators` | dialog from GNOME Settings for **new** shortcut ids only; stored in `org.gnome.settings-daemon.global-shortcuts` |
| GNOME 50+ | as above, plus `activation_token` in `Activated` | token usable for focus |
| Plasma 5.27, 6.0 | xdp-kde, but `BindShortcuts` **ignores its `shortcuts` argument** and just opens `systemsettings://kcm_keys/<component>` | effectively broken for new apps |
| Plasma 6.1 to 6.3 | `BindShortcuts` calls `session->setActions(shortcuts)` (commit `a748343e`) and still opens System Settings every time | works, annoying |
| Plasma 6.4+ | dialog only for new shortcut ids (`2fd2d093`, first in 6.3.91) | good |
| Hyprland | xdph registers via `hyprland_global_shortcuts_v1`; `preferred_trigger` is ignored, the user must add `bind = MODS, KEY, global, <appid>:<id>` to hyprland.conf | manual |
| sway | none | use sway config `bindsym … exec …` |

Evidence: [xdp-kde v5.27.11 `globalshortcuts.cpp` L95-L119](https://invent.kde.org/plasma/xdg-desktop-portal-kde/-/blob/v5.27.11/src/globalshortcuts.cpp#L95-L119),
[v6.7.5 L244-L304](https://invent.kde.org/plasma/xdg-desktop-portal-kde/-/blob/v6.7.5/src/globalshortcuts.cpp#L244-L304),
[xdp-gnome 50.0 `globalshortcuts.c` L598-L668](https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/blob/50.0/src/globalshortcuts.c#L598-L668),
[gnome-control-center 50.0 `cc-global-shortcut-dialog.c` L395-L402](https://gitlab.gnome.org/GNOME/gnome-control-center/-/blob/50.0/global-shortcuts-provider/cc-global-shortcut-dialog.c#L395-L402),
[xdph v1.4.1 `GlobalShortcuts.cpp` L57](https://github.com/hyprwm/xdg-desktop-portal-hyprland/blob/v1.4.1/src/portals/GlobalShortcuts.cpp#L57).

Interface (frontend version 1 up to 1.20.x, version 2 from 1.21.0):

- `CreateSession(a{sv} options) -> o handle`; options `handle_token s`, `session_handle_token s`;
  `Response` results carry `session_handle` (s).
- `BindShortcuts(o session_handle, a(sa{sv}) shortcuts, s parent_window, a{sv} options) -> o handle`.
  Each shortcut is `(id, {"description": s, "preferred_trigger": s})`. Response results:
  `shortcuts a(sa{sv})` with `description` and `trigger_description`.
- `ListShortcuts(o session_handle, a{sv} options) -> o handle`.
- `ConfigureShortcuts(o session_handle, s parent_window, a{sv} options)` (v2), option `activation_token s`.
- Signals: `Activated(o session_handle, s shortcut_id, t timestamp, a{sv} options)`,
  `Deactivated(…same…)`, `ShortcutsChanged(o session_handle, a(sa{sv}) shortcuts)`.
  `options["activation_token"]` exists from frontend 1.21.0 (`e6584de`), and xdp-gnome 50 fills it from
  gnome-shell's `activation-token` ([L952-L958](https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/blob/50.0/src/globalshortcuts.c#L952-L958)).
  xdp-kde 6.7 does not send one on `Activated`.
- `preferred_trigger` syntax is the XDG shortcuts spec: modifiers `CTRL`, `ALT`, `SHIFT`, `NUM`, `LOGO`,
  joined with `+`, key names from xkbcommon keysyms without `XKB_KEY_`. So `CTRL+1`, `LOGO+SHIFT+s`
  ([spec](https://specifications.freedesktop.org/shortcuts/latest/)).

**Host apps need an app id**:

- GNOME Settings rejects an invalid id outright
  ([cc-global-shortcuts-provider.c L105-L109](https://gitlab.gnome.org/GNOME/gnome-control-center/-/blob/50.0/global-shortcuts-provider/cc-global-shortcuts-provider.c#L105-L109)):
  `if (!g_application_id_is_valid (app_id)) { g_warning ("Discarded shortcut bind request …"); return G_DBUS_METHOD_INVOCATION_UNHANDLED; }`.
  `""` is not a valid GApplication id.
- Frontend 1.21.0+ refuses the session itself
  ([`38dd2c0`](https://github.com/flatpak/xdg-desktop-portal/commit/38dd2c03f244768130b3064b0b0ba50f10dda5b7),
  [1.22.1 `global-shortcuts.c` L259](https://github.com/flatpak/xdg-desktop-portal/blob/1.22.1/src/global-shortcuts.c#L259)):
  `"An app id is required"` (`NotAllowed`).
- KDE 6.x falls back to component name `token_<session token>` when the id is empty
  ([`globalshortcuts.h` L52-L55](https://invent.kde.org/plasma/xdg-desktop-portal-kde/-/blob/v6.7.5/src/globalshortcuts.h#L52-L55)).

Getting an app id as a host app:

1. **`org.freedesktop.host.portal.Registry.Register(s app_id, a{sv} options)`** on bus
   `org.freedesktop.portal.Desktop`, path `/org/freedesktop/portal/desktop`. First shipped in
   **xdp 1.19.3** (so 1.20.x on Ubuntu 25.10 and 1.21.1 on 26.04; not on 24.04).
   Rules from the [XML](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.3/data/org.freedesktop.host.portal.Registry.xml#L37-L59)
   and [`registry.c` L58-L90](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.3/src/registry.c#L58-L90):
   - once per D-Bus connection, and **before any portal method call on that connection**
     (else `"Connection already associated with an application ID"` or `"Registered too late"`);
   - `<app_id>.desktop` must exist in an `applications/` dir, because registered host apps carry
     `XDP_APP_INFO_FLAG_REQUIRE_GAPPINFO` ([`xdp-app-info-host.c` L189-L207](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.3/src/xdp-app-info-host.c#L189-L207),
     [`xdp-app-info.c` L107-L112](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.3/src/xdp-app-info.c#L107-L112));
   - re-register if the portal restarts (watch `NameOwnerChanged`).
   - Older frontends reply `UnknownInterface`/`UnknownMethod`; treat that as "not supported".
2. **Qt ≥ 6.10.1 registers automatically** with `QGuiApplication::desktopFileName()` on the shared session
   connection (`a6fce0a3c1`, [qtbase v6.10.2 `qdesktopunixservices.cpp` L376-L404](https://github.com/qt/qtbase/blob/v6.10.2/src/gui/platform/unix/qdesktopunixservices.cpp#L376-L404)).
   Call `QGuiApplication::setDesktopFileName("io.github.ubzoom")` before constructing the app.
3. Recommended for us on every Qt version: do **all our portal calls on a dedicated connection**
   (`QDBusConnection::connectToBus(QDBusConnection::SessionBus, "ubzoom-portal")`) and call `Register` on
   it first. That avoids racing Qt's own portal traffic (Settings portal reads, the automatic registration)
   on the shared connection.
4. Fallback on 1.18 (Ubuntu 24.04): the systemd scope. Launching from the app grid or GNOME 46 autostart gives
   `app-gnome-<id>-<pid>.scope` → id `<id>`. For GNOME 49+ autostart (gnome-session dropped its own
   autostart code in 49) the unit name is presumably the systemd XDG autostart generator's
   `app-<id>@autostart.service`, which the parser also accepts. **UNVERIFIED** for GNOME 49+.
5. No app id at all: GNOME GlobalShortcuts fails, the Screenshot permission is shared under `""`.

### 4b. GNOME custom keybindings through gsettings (works on GNOME 46 to 51, X11 and Wayland)

Schemas ([gsd 50.0 schema L146-L151, L653-L668](https://gitlab.gnome.org/GNOME/gnome-settings-daemon/-/blob/50.0/data/org.gnome.settings-daemon.plugins.media-keys.gschema.xml.in#L653-L668)):

- `org.gnome.settings-daemon.plugins.media-keys` key `custom-keybindings` (`as`): list of dconf paths.
- Relocatable `org.gnome.settings-daemon.plugins.media-keys.custom-keybinding` with keys `name` (s),
  `binding` (s), `command` (s).

```sh
P=/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/ubzoom-zoom/
# append $P to the existing list, do not overwrite other entries:
gsettings get org.gnome.settings-daemon.plugins.media-keys custom-keybindings
gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings "['…existing…', '$P']"
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$P name    'ubzoom: zoom'
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$P binding '<Primary>1'
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$P command \
  'gdbus call --session --dest io.github.ubzoom --object-path /io/github/ubzoom --method io.github.ubzoom.Control.Trigger zoom'
```

- The path must end with `/`. Accelerators use the GTK/mutter syntax: `<Primary>1`, `<Control>1`,
  `<Super><Shift>s` (GNOME Settings writes `<Shift><Super>s`; order does not matter), lowercase keysym names.
- gsd grabs through gnome-shell (`ShellKeyGrabber.GrabAccelerators`) on both X11 and Wayland, so the key is
  consumed by the shell and **is stolen from the focused app** (Ctrl+1 will no longer switch browser tabs).
- The command is spawned with a plain `GAppLaunchContext` plus the keyring environment
  ([gsd 50.0 `gsd-media-keys-manager.c` L998-L1050](https://gitlab.gnome.org/GNOME/gnome-settings-daemon/-/blob/50.0/plugins/media-keys/gsd-media-keys-manager.c#L998-L1050)).
  **No `XDG_ACTIVATION_TOKEN` or `DESKTOP_STARTUP_ID` is set.** The child gets a `app-gnome-<exe>-<pid>.scope`.
  Every press forks a process, so keep the command tiny (`gdbus call` from `libglib2.0-bin`, or a small C client).
- Default bindings: upstream GNOME 46 and 50 bind **nothing** to `<Super><Shift>s` or `<Primary>1..9`
  (checked `org.gnome.shell.keybindings`, `org.gnome.desktop.wm.keybindings`, mutter and gsd schemas).
  `<Super>s` is `toggle-quick-settings`, `<Super><Control>1..9` open new app windows, screenshots are
  `Print`, `<Shift>Print`, `<Alt>Print`. Ubuntu-specific overrides or extensions (Ubuntu Dock, Tiling
  Assistant): **UNVERIFIED**. Detect conflicts at runtime with
  `gsettings list-recursively | grep -iE "'<(shift|super)><(shift|super)>s'|<(primary|control)>1'"`.

### 4c. KDE: talk to kglobalaccel over D-Bus (no KF link needed)

This is exactly what `KGlobalAccel::setGlobalShortcut()` does under the hood
([kglobalaccel v6.30.0 `kglobalaccel.cpp` L213-L360, L685-L699](https://invent.kde.org/frameworks/kglobalaccel/-/blob/v6.30.0/src/kglobalaccel.cpp#L213-L360)):
`doRegister(actionId)`, then `setShortcutKeys(actionId, keys, SetPresent)` (active, autoloading), then
`setShortcutKeys(actionId, keys, IsDefault)` (default), then `getComponent()` and connect to its signals.

- Service `org.kde.kglobalaccel`, path `/kglobalaccel`, iface `org.kde.KGlobalAccel`.
- `actionId` is an `as` of four strings: `[componentUnique, actionUnique, componentFriendly, actionFriendly]`
  (`KGlobalAccel::actionIdFields`). Use the desktop-file id as `componentUnique` so System Settings shows the
  right name and icon.
- Methods:

| Method | Signature | Notes |
|---|---|---|
| `doRegister` | `(as actionId)` | creates the action |
| `setShortcutKeys` | `(as actionId, a(ai) keys, u flags) -> a(ai)` | since KF 5.90 (`d7f922b`); in both the KF5 daemon and Plasma 6 `kglobalacceld` |
| `setShortcut` | `(as actionId, ai keys, u flags) -> ai` | deprecated since 5.90; compiled only if the daemon keeps deprecated API. Avoid. |
| `getComponent` | `(s componentUnique) -> o` | path is `/component/<name with [^A-Za-z0-9_] → _>` |
| `setInactive` | `(as actionId)` | call on exit; the daemon does not track our bus name |
| `unregister` | `(s componentUnique, s actionUnique) -> b` | forget completely |
| signal `yourShortcutsChanged` | `(as actionId, a(ai) newKeys)` | user rebinds in System Settings |

- Flags ([`kglobalaccel_p.h` L23-L27](https://invent.kde.org/frameworks/kglobalaccel/-/blob/v6.30.0/src/kglobalaccel_p.h#L23-L27)):
  `SetPresent = 2`, `NoAutoloading = 4`, `IsDefault = 8`. Without `NoAutoloading`, a shortcut that already
  exists in `kglobalshortcutsrc` keeps the user's keys and the daemon returns those
  ([kglobalacceld v6.7.5 L481-L520](https://invent.kde.org/plasma/kglobalacceld/-/blob/v6.7.5/src/kglobalacceld.cpp#L481-L520)).
  If the key clashes with another component, the returned list is empty.
- Component iface `org.kde.kglobalaccel.Component` on the returned path:
  signal `globalShortcutPressed(s componentUnique, s actionUnique, x timestamp)`, `globalShortcutReleased`
  (same signature), `globalShortcutRepeated` (client XML since KF 6.17), method `invokeShortcut(s actionName)`.
- Key encoding: a `QKeySequence` goes on the wire as a struct holding an array of 4 ints, `(ai)`, so a list is
  `a(ai)` ([v5.115.0 `kglobalshortcutinfo_dbus.cpp` L10-L39](https://invent.kde.org/frameworks/kglobalaccel/-/blob/v5.115.0/src/kglobalshortcutinfo_dbus.cpp#L10-L39)):
  ```cpp
      argument.beginStructure();
      argument.beginArray(qMetaTypeId<int>());
      for (int i = 0; i < maxSequenceLength; i++) {
  #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
          argument << (i < sequence.count() ? sequence[i].toCombined() : 0);
  #else
          argument << (i < sequence.count() ? sequence[i] : 0);
  ```
  Qt 5 `int` keys and Qt 6 `QKeyCombination::toCombined()` have the same values, so the wire format did not
  change between 5.27 and 6.x. Examples: Ctrl+1 = `0x04000031`; Meta+Shift+S = `0x12000053`.
  Pad unused slots with 0. Define your own `struct { int k[4]; }` with `operator<<`/`>>` for QtDBus.
- Stability: same service, path, method names and signatures in the KF5 daemon (Plasma 5.27) and in
  `kglobalacceld` (Plasma 6). Works on KDE X11 as well (the daemon grabs X keys itself).

### 4d. X11: `xcb_grab_key`

```c
xcb_void_cookie_t c = xcb_grab_key_checked(conn, 1 /*owner_events*/, root,
                                           mods | extra, keycode,
                                           XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
xcb_generic_error_t *e = xcb_request_check(conn, c);   /* error_code 10 = BadAccess: someone else has it */
```

- Grab each combination four times: `extra ∈ {0, XCB_MOD_MASK_LOCK, numlock, XCB_MOD_MASK_LOCK|numlock}`.
  Find the NumLock mask with `xcb_get_modifier_mapping` + keycode of `XK_Num_Lock` (usually `Mod2`).
  Super is normally `Mod4` (`XCB_MOD_MASK_4`). Re-grab on `MappingNotify` / XKB `NewKeyboardNotify`.
  Resolve keycodes with `xcb-keysyms` (`libxcb-keysyms1-dev`); with non-Latin layouts look up the keysym in
  group 0 as well.
- On GNOME X11 (46 to 49) nothing grabs Ctrl+1 or Super+Shift+S by default, so the grab should succeed.
  While gnome-shell holds a modal grab (overview, menus, its own screenshot UI) passive grabs do not fire.
  A gsd custom keybinding with the same accelerator would cause `BadAccess`.
- Focus on X11 depends on `_NET_WM_USER_TIME`. Qt calls the native event filter **before** it records the
  event time ([qtbase v6.4.2 `qxcbconnection.cpp` L538-L642](https://github.com/qt/qtbase/blob/v6.4.2/src/plugins/platforms/xcb/qxcbconnection.cpp#L538-L642)),
  so grab on Qt's own connection (`QNativeInterface::QX11Application::connection()`) and return `false` from
  the filter for the key press, letting Qt run `setTime(keyPress->time)`. KDE's library does the equivalent
  with the kglobalaccel timestamp (`QX11Info::setAppUserTime`, private Qt API).
- XGrabKey from an Xwayland client under Wayland only sees keys while an X11 window has focus. Useless there.

---

## 5. Overlay windows on Wayland (Qt 6.4+)

### Fullscreen and target output

- qtwayland sends `xdg_toplevel.set_fullscreen(output)` using the window's current `QScreen`, and does it in
  the toplevel constructor, before the first commit
  ([qtwayland v6.4.2 `qwaylandxdgshell.cpp` L20-L31, L152-L156](https://github.com/qt/qtwayland/blob/v6.4.2/src/plugins/shellintegration/xdg-shell/qwaylandxdgshell.cpp#L152-L156)):
  ```cpp
        if (states & Qt::WindowFullScreen) {
            auto screen = m_xdgSurface->window()->waylandScreen();
            if (screen) {
                set_fullscreen(screen->output());
  ```
  So: one widget per `QScreen`, `setWindowFlags(Qt::FramelessWindowHint)`, `setScreen(screen)` (QWidget has
  it in 6.4.2), then `showFullScreen()`. `setGeometry()` and `Qt::WindowStaysOnTopHint` do nothing useful on
  Wayland.
- **GNOME top bar**: covered. Mutter marks a monitor `in_fullscreen` when an unobscured fullscreen window is on
  it, no focus needed ([mutter 50 `display.c` L2855-L2892](https://gitlab.gnome.org/GNOME/mutter/-/blob/50.0/src/core/display.c#L2835-L2896)),
  and gnome-shell hides `panelBox` because it was added with `trackFullscreen: true`
  ([`layout.js` L283-L286](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/50.0/js/ui/layout.js#L283-L286)).
- **KDE panels**: covered. A fullscreen window sits in `ActiveLayer` (above docks in `AboveLayer`) when it is
  active **or the active window is on another output**, so one overlay per monitor all go on top
  ([kwin v6.7.5 `window.cpp` L562-L582, L2280-L2291](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/window.cpp#L2280-L2291)).
- Qt 6.4.2 has no `wp_fractional_scale_v1` (added in 6.5), so at 125/150 % scaling the buffer is rendered at
  the next integer scale and downscaled by the compositor. Fine for a frozen image, slightly soft.

### Keyboard focus when a background process maps the window

- **Mutter**: a new Wayland toplevel without user-time information is treated as "no information" and takes
  focus, unless `org.gnome.desktop.wm.preferences focus-new-windows` is `'strict'`
  ([mutter 50 `window.c` L2049-L2055, L2109-L2141](https://gitlab.gnome.org/GNOME/mutter/-/blob/50.0/src/core/window.c#L2109-L2141)):
  ```c
    if (!(window->net_wm_user_time_set) && !(window->initial_timestamp_set))
      {
        meta_topic (META_DEBUG_STARTUP, "no information about window %s found", window->desc);
        return FALSE;   /* no intervening event -> takes_focus = TRUE */
      }
  ```
- **KWin**: new windows are activated when the focus-stealing-prevention level is ≤ Low, and the default level
  is 1 (Low) ([`workspace.cpp` L930-L951](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/workspace.cpp#L930-L951),
  [`kwin.kcfg` L112-L113](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/kwin.kcfg#L112-L113)).
- `QWidget::hide()` destroys the xdg_toplevel (`QWaylandWindow::setVisible(false)` → `reset()`,
  [qwaylandwindow.cpp L443-L461](https://github.com/qt/qtwayland/blob/v6.4.2/src/client/qwaylandwindow.cpp#L443-L461)),
  so hide/show gives a *new* toplevel each time, which is what gets focus. Do not rely on
  `requestActivate()` for an already mapped window: without a token mutter shows "… is ready" instead.
- Activation tokens: Qt 6.4.2 uses `XDG_ACTIVATION_TOKEN` from the environment in `requestActivate()` and
  then unsets it ([`qwaylandxdgshell.cpp` L488-L515](https://github.com/qt/qtwayland/blob/v6.4.2/src/plugins/shellintegration/xdg-shell/qwaylandxdgshell.cpp#L488-L515)).
  If a token arrives (GlobalShortcuts `Activated` on GNOME 50+), `qputenv("XDG_ACTIVATION_TOKEN", token)`
  right before `requestActivate()`. gsd custom keybindings do not provide one (§4b).

### Cursor position before showing

- `QCursor::pos()` on Wayland returns the last position Qt saw on **its own** surfaces
  ([qwaylandcursor.cpp L242-L251](https://github.com/qt/qtwayland/blob/v6.4.2/src/client/qwaylandcursor.cpp#L242-L251));
  it is stale until one of our surfaces gets `wl_pointer.enter`.
- Workaround that works everywhere: map the overlays on every output, then use the first enter/motion event to
  learn which output holds the pointer and where. Only then pick the zoom target.
- KDE: `org.kde.KWin` `/KWin` `org.kde.KWin.activeOutputName() -> s` returns the active output (the one with the
  cursor under the default "active screen follows mouse") in 5.27 and 6.x
  ([dbusinterface.cpp L79-L82](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/dbusinterface.cpp#L79-L82)).
  Exact coordinates are only reachable through a KWin script (`workspace.cursorPos`) loaded via
  `org.kde.KWin /Scripting`; heavy for a hotkey path.
- Hyprland: `hyprctl cursorpos`. sway: no cursor query; `swaymsg -t get_outputs` gives the focused output.
- GNOME: no public API (Shell.Eval is unsafe-mode only).

### LayerShellQt

- Better on Plasma 6, sway and Hyprland: `LayerOverlay` is above panels and notifications, anchors to one
  output (`setScreen()`, 6.6+; `setScreenConfiguration()` before that), no task-switcher entry. KWin also
  explicitly allows activation for an Overlay/Top layer surface anchored on all four edges
  ([`activation.cpp` L618-L636](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/activation.cpp#L618-L636)).
  Set `setKeyboardInteractivity(KeyboardInteractivityExclusive)` and `setExclusiveZone(-1)`.
- Not usable on GNOME (mutter has no layer-shell), and not with our Qt 6 build on Plasma 5.27 because
  distros ship the Qt 5 build (noble: 5.27.11 linked against qtbase5). Per-window use needs Qt ≥ 6.5
  ("Calling useLayerShell is not needed since Qt 6.5", [`shell.h`](https://invent.kde.org/plasma/layer-shell-qt/-/blob/v6.7.5/src/interfaces/shell.h)).
  Plan: optional backend, enabled when `zwlr_layer_shell_v1` is advertised and the Qt 6 library is found.

---

## 6. Clipboard on Wayland

- **Set it while the overlay still has keyboard focus, then hide.** Mutter silently cancels a selection from a
  client that is not the keyboard-focus client
  ([mutter 50 `meta-wayland-data-device.c` L1147-L1152](https://gitlab.gnome.org/GNOME/mutter/-/blob/50.0/src/wayland/meta-wayland-data-device.c#L1147-L1152)):
  ```c
    if (wl_resource_get_client (resource) != data_device->focus_client)
      {
        if (source)
          meta_wayland_data_source_cancel (source);
        return;
      }
  ```
  KWin only checks that the serial is not older than the current selection's
  ([kwin `seat.cpp` L311-L320](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/wayland/seat.cpp#L311-L320)).
- After hiding, the selection stays valid: Qt keeps the `QWaylandDataSource` alive
  ([qwaylandclipboard.cpp L57-L89](https://github.com/qt/qtwayland/blob/v6.4.2/src/client/qwaylandclipboard.cpp#L57-L89))
  and our process answers `send` requests as long as its event loop runs.
- GNOME also copies it immediately: mutter's clipboard manager transfers the best MIME type
  (`image/png` preferred, up to 200 MB; text up to 4 MB) when a new owner appears and restores it if the owner
  goes away ([`meta-clipboard-manager.c` L26-L42](https://gitlab.gnome.org/GNOME/mutter/-/blob/50.0/src/core/meta-clipboard-manager.c#L26-L42)).
  On KDE, whether Klipper keeps images depends on its settings (**UNVERIFIED** defaults).
- `wl-copy --type image/png < shot.png` works on compositors with the data-control protocol (KWin has it,
  sway/Hyprland have it). Mutter 51 has no data-control implementation, so wl-copy falls back to a
  focus-grabbing surface there (**UNVERIFIED** how reliable). Use Qt's clipboard as the primary path.

---

## 7. Live zoom through the compositor

### GNOME (X11 and Wayland; the magnifier lives in gnome-shell `js/ui/magnifier.js`)

```sh
gsettings set org.gnome.desktop.a11y.magnifier mag-factor 2.0            # double, 0.1 .. 32.0
gsettings set org.gnome.desktop.a11y.magnifier mouse-tracking 'proportional'  # none | centered | proportional | push
gsettings set org.gnome.desktop.a11y.magnifier screen-position 'full-screen'
gsettings set org.gnome.desktop.a11y.applications screen-magnifier-enabled true
# … later
gsettings set org.gnome.desktop.a11y.applications screen-magnifier-enabled false
```

The shell listens to `changed::mag-factor` and `changed::mouse-tracking` live
([magnifier.js L577-L590](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/50.0/js/ui/magnifier.js#L577-L590)).
Default gsd shortcuts: toggle `<Alt><Super>8`, zoom `<Alt><Super>equal` / `<Alt><Super>minus`.
Save and restore the user's previous values.

### KWin (X11 with compositing, and Wayland; Plasma 5.27 and 6.x)

The Zoom effect is enabled by default and registers `KStandardAction` zoom actions under component `kwin`
([v6.7.5 `zoom.cpp` L50-L60](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/plugins/zoom/zoom.cpp#L50-L60);
5.27 uses `KStandardAction::zoomIn/zoomOut/actualSize` the same way):

```sh
dbus-send --session --type=method_call --dest=org.kde.kglobalaccel /component/kwin \
  org.kde.kglobalaccel.Component.invokeShortcut string:view_zoom_in
# view_zoom_out, view_actual_size ; qdbus6 / qdbus equivalents:
qdbus6 org.kde.kglobalaccel /component/kwin invokeShortcut view_zoom_in
# make sure the effect is loaded:
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded zoom
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect zoom
```

There is no D-Bus call to set an absolute factor; each step multiplies by `[Effect-zoom] ZoomFactor`
in kwinrc (default 1.2, **UNVERIFIED** default). The action names are the `KStandardAction` object names
`view_zoom_in`, `view_zoom_out`, `view_actual_size`.

---

## 8. Screen recording from a host app

### Wayland: ScreenCast portal + PipeWire

Interface `org.freedesktop.portal.ScreenCast` (version 5 in xdp 1.18)
([XML](https://github.com/flatpak/xdg-desktop-portal/blob/1.18.4/data/org.freedesktop.portal.ScreenCast.xml)):

1. `CreateSession(a{sv} {handle_token, session_handle_token}) -> o` → Response `session_handle` (s).
2. `SelectSources(o session, a{sv} options) -> o`. Options: `handle_token s`, `types u` (1 MONITOR,
   2 WINDOW, 4 VIRTUAL; default MONITOR), `multiple b`, `cursor_mode u` (1 hidden, 2 embedded, 4 metadata;
   must be in `AvailableCursorModes`), `persist_mode u` (0 none, 1 while running, 2 until revoked),
   `restore_token s`.
3. `Start(o session, s parent_window, a{sv} {handle_token}) -> o` → Response `streams a(ua{sv})`
   (node id + `position (ii)`, `size (ii)`, `source_type u`, `mapping_id s` in v5) and `restore_token s`.
4. `OpenPipeWireRemote(o session, a{sv} {}) -> h fd` (a UNIX fd). In QtDBus:
   `QDBusReply<QDBusUnixFileDescriptor>`; `dup()` the fd before the reply object dies.
5. Close with `org.freedesktop.portal.Session.Close()` on the session path.

In-process GStreamer (recommended; `pipewiresrc` properties from
[pipewire 1.0.5 `gstpipewiresrc.c` L290-L370](https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/1.0.5/src/gst/gstpipewiresrc.c#L290-L370):
`fd` (int), `path` (deprecated but still accepted; the portal node id), `target-object` (name/serial),
`keepalive-time`, `resend-last`, `always-copy`):

```cpp
gst_init(nullptr, nullptr);
const QString desc = QStringLiteral(
    "pipewiresrc fd=%1 path=%2 do-timestamp=true keepalive-time=1000 resend-last=true "
    "! videoconvert ! videorate ! video/x-raw,framerate=30/1 ! queue "
    "! x264enc tune=zerolatency speed-preset=veryfast bitrate=8000 ! h264parse "
    "! mp4mux ! filesink location=%3").arg(dupFd).arg(nodeId).arg(outPath);
GError *err = nullptr;
GstElement *pipe = gst_parse_launch(desc.toUtf8().constData(), &err);
gst_element_set_state(pipe, GST_STATE_PLAYING);
// stop: send EOS and wait for GST_MESSAGE_EOS on the bus before GST_STATE_NULL, otherwise mp4 is unfinished
gst_element_send_event(pipe, gst_event_new_eos());
```

Out-of-process alternative: `QProcess::setChildProcessModifier([fd]{ dup2(fd, 3); })` (Qt 6.0+) and run
`gst-launch-1.0 -e pipewiresrc fd=3 path=<node> ! …`. Stop it with `kill(pid, SIGINT)`: `-e` turns SIGINT
into EOS, while `QProcess::terminate()` sends SIGTERM and leaves a broken MP4. Received fds are CLOEXEC,
hence the `dup2`.

The same flow with `num-buffers=1 ! videoconvert ! video/x-raw,format=BGRx ! appsink` gives the
single-frame GNOME capture of §1e.

### X11

```sh
ffmpeg -f x11grab -framerate 30 -video_size 1920x1080 -i :0.0+0,0 \
       -f pulse -i default -c:v libx264 -preset veryfast -pix_fmt yuv420p -c:a aac out.mp4
# stop gracefully by writing "q" to ffmpeg's stdin
```

wlroots: `wf-recorder -o <output> -f out.mp4` (0.4.1 on noble) or the ScreenCast portal via xdp-wlr / xdph.

### Ubuntu 24.04 packages

- Build: `qt6-base-dev qt6-base-private-dev qt6-wayland-dev qt6-wayland-private-dev libgstreamer1.0-dev
  libgstreamer-plugins-base1.0-dev libpipewire-0.3-dev libxcb1-dev libxcb-keysyms1-dev libxcb-xtest0-dev`.
- Runtime: `qt6-wayland gstreamer1.0-pipewire` (1.0.5 on noble) `gstreamer1.0-plugins-base
  gstreamer1.0-plugins-good` (vp8enc, webmmux, mp4mux, matroskamux) `gstreamer1.0-plugins-ugly` (x264enc)
  `gstreamer1.0-plugins-bad` (openh264enc, va encoders) `gstreamer1.0-libav gstreamer1.0-tools`
  (gst-launch-1.0) `ffmpeg` (X11 path) `libglib2.0-bin` (gdbus, gsettings).
- Optional: `grim` 1.4.0, `wf-recorder`, `wl-clipboard`, `wtype`, `ydotool` (0.1.8 on noble, 1.0.4 on resolute;
  different CLIs).

---

## 9. Typing injection ("DemoType")

### RemoteDesktop portal

`org.freedesktop.portal.RemoteDesktop` (version 2 in xdp 1.18 through 1.22):

1. `CreateSession(a{sv} {handle_token, session_handle_token}) -> o`.
2. `SelectDevices(o session, a{sv} {handle_token, types u (1 KEYBOARD, 2 POINTER, 4 TOUCHSCREEN),
   restore_token s, persist_mode u}) -> o`. (For persistent sessions the token goes here, not in ScreenCast.)
3. `Start(o session, s parent_window, a{sv} {handle_token}) -> o` → results `devices u`, `restore_token s`.
4. `NotifyKeyboardKeysym(o session, a{sv} options, i keysym, u state)` with state 1 press, 0 release;
   `NotifyKeyboardKeycode(o session, a{sv}, i keycode, u state)` takes evdev codes.

Behaviour:

- GNOME: dialog on first `Start`; restore supported by xdp-gnome 46+ so later sessions are silent. The
  remote-access indicator shows while the session runs (same 5 s minimum as §1e). Mutter only injects keysyms
  that exist in the **current layout group** and adds the needed level modifiers itself
  ([mutter 50 `meta-virtual-input-device-native.c` L583-L588](https://gitlab.gnome.org/GNOME/mutter/-/blob/50.0/src/backends/native/meta-virtual-input-device-native.c#L583-L588)):
  `g_warning ("No keycode found for keyval %x in current group", event->key);`
- KDE 5.27: RemoteDesktop v1 in xdp-kde, **no persistence**, so a dialog every time. Persistence came with
  `c9bf8979` (v5.90.0, i.e. Plasma 6.0) and restore fixes in 6.2. From Plasma 6.3 (`2d4a4dfe`) the user can
  pre-authorise non-interactively: `flatpak permission-set kde-authorized remote-desktop <app_id> yes`
  (use `""` for host apps without an id), see [`remotedesktop.cpp` L42-L72](https://invent.kde.org/plasma/xdg-desktop-portal-kde/-/blob/v6.7.5/src/remotedesktop.cpp#L42-L72).
  KWin also resolves keysyms through the active keymap only
  ([`fakeinputbackend.cpp` L282-L300](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/backends/fakeinput/fakeinputbackend.cpp#L282-L300)).
- KDE alternative without a dialog: `org_kde_kwin_fake_input` is granted to executables whose `.desktop`
  lists it in `X-KDE-Wayland-Interfaces` (same exe-path matching as §2,
  [`wayland_server.cpp` L138-L195](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/wayland_server.cpp#L138-L195)).
  Needs our own Wayland protocol client code.

### Other tools

- `wtype` needs `zwp_virtual_keyboard_manager_v1`: sway and Hyprland yes; mutter no; KWin v6.7.5 has no such
  implementation in its tree. It uploads its own keymap, so any Unicode character works.
- `ydotool` writes to `/dev/uinput`: works on every compositor but needs uinput access (root, or a udev rule
  plus group) and, for 1.x, the `ydotoold` daemon. Layout-dependent (it sends keycodes). Poor fit for a
  desktop app install.
- X11: XTest, `xcb_test_fake_input(conn, XCB_KEY_PRESS / XCB_KEY_RELEASE, keycode, XCB_CURRENT_TIME, root, 0, 0, 0)`.
  For characters missing from the keymap, temporarily remap a spare keycode with
  `xcb_change_keyboard_mapping` (the xdotool technique).

---

## Decisions this implies

| Area | GNOME Wayland | KDE (5.27, 6.x) | X11 | wlroots / Hyprland |
|---|---|---|---|---|
| Freeze-frame capture | Screenshot portal, `interactive=false`; expect a one-time Access dialog, a flash and a shutter sound; delete `~/Pictures/Screenshot*.png` after reading. Optional mode: ScreenCast with `persist_mode=2` (no flash, 5 s indicator). | KWin ScreenShot2 `CaptureScreen(name, {native-resolution: true})` per output; ship a `.desktop` with absolute `Exec=` and `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2`. | `QScreen::grabWindow(0)` or XShm | `grim -o <out> -t ppm -` |
| Hotkeys | GNOME 48+: GlobalShortcuts portal (needs app id). GNOME 46/47 and fallback: gsd custom keybinding running `gdbus call` into the daemon. | kglobalaccel D-Bus: `doRegister` + `setShortcutKeys(a(ai))` + `globalShortcutPressed`. Skip the portal (broken on 5.27/6.0, noisy until 6.4). | `xcb_grab_key` × lock-mask variants (non-KDE); kglobalaccel on KDE X11 | Hyprland: portal + user `bind … global`; sway: config `bindsym` → CLI |
| App identity | Install `io.github.ubzoom.desktop`; `setDesktopFileName()` before app construction; `Registry.Register` on a dedicated portal D-Bus connection (xdp ≥ 1.19.3); on 24.04 rely on the launch scope. | Same `.desktop` doubles as the KWin authorisation file. | n/a | same as GNOME for the portal |
| Overlay | One frameless fullscreen `QWidget` per `QScreen`, `setScreen()` before `showFullScreen()`, fresh map on each activation (gets focus). Use `activation_token` when GNOME 50 provides it. | Same; optional LayerShellQt (Plasma 6, Qt ≥ 6.5). | Same (fullscreen, optional override-redirect later) | LayerShellQt or fullscreen |
| Which monitor | Map on all outputs, wait for pointer enter | `org.kde.KWin.activeOutputName()` | `QCursor::pos()` | `hyprctl cursorpos` / focused output |
| Clipboard | `QClipboard::setImage` while overlay focused, then hide; keep the process alive | same | same | same |
| Live zoom | gsettings magnifier keys | `invokeShortcut view_zoom_in/out/actual_size` on `/component/kwin` | same per desktop | none built in |
| Recording | ScreenCast portal → in-process `pipewiresrc fd= path=` pipeline | same | `ffmpeg -f x11grab` | same portal, or wf-recorder |
| DemoType | RemoteDesktop portal, `persist_mode=2`, keysyms (layout-limited) | same on 6.x; 5.27 prompts each time; 6.3+ can be pre-authorised | XTest | wtype |
