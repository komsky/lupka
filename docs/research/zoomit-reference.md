# ZoomIt behaviour and bindings reference

Research date: 2026-09-30. No code was written for this document.

## 0. Scope, sources, conventions

Target: the latest Sysinternals ZoomIt and the PowerToys ZoomIt module. Both are built from the same source, which lives in `src/modules/ZoomIt/` of the PowerToys repo (MIT licence).

Version note: ZoomIt is at v12.22 (Sysinternals blog, 2026-09-10), not the v9.x the request assumed. The source's About dialog string still reads "ZoomIt v12.11". Release history is in section 0.2.

Sources used, in order of authority:

| Tag | What | URL |
|---|---|---|
| SRC | `Zoomit.cpp` (12,913 lines), last changed in commit 7fb3480 (2026-09-09, "add smooth zoom animations", #50444). PowerToys `main` was at 58d63e4 when fetched. | https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp |
| SRC-H | `ZoomIt.h` (constants), `ZoomItSettings.h` (defaults), `ZoomAnimation.cpp`, `DemoType.cpp/.h`, `SelectRectangle.cpp/.h`, `MirrorWindow.h`, `PanoramaCapture.cpp` (header comment), `ZoomIt.rc` (dialog text) | https://github.com/microsoft/PowerToys/tree/main/src/modules/ZoomIt/ZoomIt |
| DOC-SYS | Sysinternals page, shortcut table | https://learn.microsoft.com/en-us/sysinternals/downloads/zoomit |
| DOC-PT | PowerToys page, settings list and defaults | https://learn.microsoft.com/en-us/windows/powertoys/zoomit |
| BLOG | Sysinternals blog release notes | https://techcommunity.microsoft.com/category/Windows/blog/Sysinternals-Blog/all-posts |

Conventions:
- "Line N" means a line in the file as fetched on 2026-09-30. Line numbers drift; function and message names are given so they can be searched.
- UNCERTAIN marks anything I could not confirm from source, or where source and docs disagree.
- Key names follow Windows. `Ctrl+2` means Ctrl held with the digit-row 2 key.
- Win32 message names (WM_KEYDOWN and so on) are used only to say where in the source a behaviour lives.

### 0.1 Where the docs and the source disagree

| Topic | DOC-SYS says | Source says | Treat as |
|---|---|---|---|
| Whiteboard and blackboard | `W` and `K` | `Ctrl+W` and `Ctrl+K` blank the screen; plain `W` and `K` select white and black pens (`WM_KEYDOWN`, line 9522 onward). The Draw tab text in `ZoomIt.rc` also says Ctrl+W / Ctrl+K. | Source |
| Increase/decrease font size | "Ctrl + wheel or arrows" | In type mode, plain wheel or Up/Down changes font size; no Ctrl needed (`WM_MOUSEWHEEL`, line 9247 onward). The Type tab text says the same. | Source |
| Reactivate break timer | "left-click the tray icon" | Single click opens the tray menu. Double-click calls `SetForegroundWindow` on the timer window if a timer is running (`WM_USER_TRAY_ACTIVATE`, around line 10553). The menu item "Break Timer" re-runs `IDC_BREAK` (line 11349), which resets the countdown. | UNCERTAIN, source reading |
| Save format | "Save as PNG" | Save dialog offers Zoomed/Actual-size PNG, WebP, JPG (six filters). | Source |

### 0.2 Release history (Sysinternals blog RSS)

Source: https://techcommunity.microsoft.com/t5/s/gxcuf89792/rss/board?board.id=Sysinternals-Blog (individual posts, for example https://techcommunity.microsoft.com/t5/sysinternals-blog/livekd-v5-65-procdump-v12-01-and-zoomit-v12-11/ba-p/4535408).

| Version | Date | Change |
|---|---|---|
| 9.01 | 2025-09-17 | Fixes drawing vanishing after snip; mouse click draws a dot again; consecutive record fix |
| 9.10 | 2025-10-13 | Image smoothing option |
| 9.20 | 2025-11-11 | Record as MP4 or GIF |
| 9.21 | 2025-11-17 | Bug fixes |
| 10.0 | 2026-02-06 | Video trim editor; recording with system audio |
| 11.0 | 2026-03-26 | Panorama (scrolling screenshot), text extraction (OCR) snip, break timer improvements, trim editor for existing MP4 |
| 12.0 | 2026-05-07 | Webcam overlay in recordings; append clips in trim editor |
| 12.1 | 2026-06-19 | Image backgrounds, webcam background blur, mic noise cancellation |
| 12.11 | 2026-07-09 | JPEG and WebP snips |
| 12.2 | 2026-08-12 | DemoMirror |
| 12.21 | 2026-08-19 | Fix hang while recording |
| 12.22 | 2026-09-10 | Smooth zooming |

Microsoft also ships ZoomIt for macOS (https://github.com/microsoft/ZoomitForMac, MIT, Swift, v12.3.0 on 2026-08-19). It reimplements the same viewport maths, so it is a second reference for the behaviours below.

## 1. Global hotkeys

Source: https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L7860-L7990 (registration), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L7991-L8160 (`WM_HOTKEY`), https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomItSettings.h (defaults), https://learn.microsoft.com/en-us/sysinternals/downloads/zoomit, https://learn.microsoft.com/en-us/windows/powertoys/zoomit.

Registered with `RegisterHotKey` on a hidden main window (lines 7876 onward). All are user-configurable in Options.

| Function | Default | Notes |
|---|---|---|
| Zoom (static) | Ctrl+1 | Toggle. Not registered with MOD_NOREPEAT. |
| Draw without zoom | Ctrl+2 | |
| Break timer | Ctrl+3 | |
| LiveZoom | Ctrl+4 | Toggle. |
| LiveDraw | Ctrl+Shift+4 | Derived: the LiveZoom key with Shift XORed in (`LIVE_DRAW_HOTKEY`). If the user's LiveZoom key already uses Shift, LiveDraw is the same key without Shift. |
| Record (full monitor) | Ctrl+5 | Toggle: press again to stop. |
| Record region | Ctrl+Shift+5 | Derived by XOR with Shift. |
| Record window under cursor | Ctrl+Alt+5 | Derived by XOR with Alt. |
| Snip to clipboard | Ctrl+6 | |
| Snip to file | Ctrl+Shift+6 | Separate setting (`SnipSaveToggleKey`). |
| Snip text (OCR) to clipboard | Ctrl+Alt+6 | Separate setting. |
| DemoType start | Ctrl+7 | |
| DemoType step back | Ctrl+Shift+7 | Derived by XOR with Shift (`DEMOTYPE_RESET_HOTKEY`). |
| Panorama to clipboard | Ctrl+8 | Press again to finish. |
| Panorama to file | Ctrl+Shift+8 | Separate setting. |
| DemoMirror whole screen | Ctrl+9 | |
| DemoMirror region | Ctrl+Shift+9 | Derived by XOR with Shift. |
| DemoMirror window | Ctrl+Alt+9 | Derived by XOR with Alt. |

Hotkeys that exist only in a mode (registered and unregistered on entry and exit):

| Hotkey | Active while | Effect |
|---|---|---|
| Ctrl+C, Ctrl+Shift+C, Ctrl+S, Ctrl+Shift+S | Static zoom (`g_Zoomed`) | Copy or save, full or cropped. Registered at line 8939, removed at line 7741. |
| Ctrl+C | Recording | Toggle webcam overlay on and off. Line 7237. |
| Ctrl+Up, Ctrl+Down | LiveZoom window visible | Zoom in or out. Line 11958. |

If a hotkey cannot be registered the app shows "already in use" and opens Options.

Tray menu (right or left click; lines 10530 onward): Options, Break Timer, Draw, Zoom, Record, Exit. Double-click opens Options (or focuses a running timer). The menu item Zoom sends the zoom hotkey with lParam 1, which is the "real hotkey" path (starts the telescoping zoom-in animation).

## 2. Zoom mode (static zoom, Ctrl+1)

Source: https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L8881-L9135 (entry and exit), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L9247-L9410 (wheel), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L1493-L1525 (mapping), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L11599-L11700 (paint), https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomAnimation.cpp, https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomIt.h, https://learn.microsoft.com/en-us/windows/powertoys/zoomit.

### 2.1 Entry (`ZOOM_HOTKEY`, line 8881 onward)

1. If Options is open, ignore. If a break timer is active, the hotkey ends the timer instead.
2. Choose the monitor containing the cursor at the moment of the hotkey. Everything (capture, overlay, drawing) is limited to that monitor.
3. Take a 1:1 snapshot of that monitor with GDI `BitBlt(... SRCCOPY|CAPTUREBLT)`. If a recording is running it uses Windows Graphics Capture instead, so the recording border is not baked into the snapshot. Two copies are kept: a working bitmap and a pristine copy used by Erase.
4. Show a borderless topmost full-monitor window. The zoom is drawn by that window stretching the snapshot; the real desktop keeps running underneath but is not visible.
5. Start the zoom-in animation from 1.0 to the initial magnification (only when the hotkey or tray menu started zoom; the internal "enter draw without zoom" path skips it).

Sticky Keys is switched off while pen mode is active and restored when pen mode ends (`EnableDisableStickyKeys`, lines 10337 and 10664).

The window loses zoom if it loses focus (`WM_KILLFOCUS` posts the zoom hotkey again), unless the loss is to the webcam preview, a crop selection, a save dialog, or LiveDraw.

### 2.2 Magnification values

| Item | Value | Source |
|---|---|---|
| Initial magnification choices (Options slider, 6 stops) | 1.25, 1.5, 1.75, 2.0, 3.0, 4.0 | `g_ZoomLevels`, line 137 |
| Default initial magnification | 2.0 (slider index 3) | `g_SliderZoomLevel = 3` |
| Minimum zoom | 1 | `ZOOM_LEVEL_MIN` |
| Maximum zoom | 256 | `ZOOM_LEVEL_MAX` |
| `ZOOM_LEVEL_INIT` | 2 (defined; I did not find it used for the initial level) | UNCERTAIN |

Step per wheel notch or Up/Down key (`WM_MOUSEWHEEL`, lines 9247-9330). Up/Down send a synthetic wheel event of one notch (`WHEEL_DELTA`).

| Direction | Rule |
|---|---|
| Zoom in | If target < 2, target = 2. Otherwise target = target x 2. Only applied while target < 256. |
| Zoom out | If target <= 2, target = target x 0.75, clamped to 1. Otherwise target = target / 2. Only applied while target > 1. |

Consequences, derived from the rules:
- From 1.0 the in-sequence is 2, 4, 8, ... 256.
- From the 3.0 initial level the in-sequence is 6, 12, 24 ... 192, 384. The `< 256` guard runs before doubling, so 384 is reachable. UNCERTAIN whether that is intended.
- Out-sequence from 2.0 is 1.5, 1.125, 1.0.
- Wheel delta is rounded away from zero to whole notches, so a high-resolution wheel event of any size counts as at least one step. Multi-notch deltas loop once per notch.
- Each notch immediately changes the target and retargets the running animation (see 2.3), so fast scrolling compounds.
- Wheel zoom is ignored in LiveDraw (window has `WS_EX_LAYERED`) and in type mode (there the wheel changes font size).

### 2.3 Zoom animation

Files: `ZoomAnimation.cpp/.h`, added in #50444 (v12.22). Frame timer 10 ms (`ZOOM_ANIMATION_FRAME_TIME`).

- Animation runs in log-zoom space: `s = ln(zoom)`, cubic Hermite interpolation between `ln(start)` and `ln(target)`, `zoom = exp(s(t))`.
- Duration = `OriginalDuration(...) x 3 / 2`, where `OriginalDuration` is the number of legacy steps times 40 ms. A legacy step multiplies by 1.1 going in and by 0.8 going out, counted until the target is passed. When the first step is applied immediately (zoom in from the hotkey or a wheel notch) one step is subtracted.
- Worked values: 1 to 1.25 = 120 ms; 1 to 2 = 420 ms; 1 to 3 = 660 ms; 1 to 4 = 840 ms; 2 to 4 = 420 ms; 2 to 1.5 = 60 ms; leaving zoom from 2.0 = 240 ms; 1 to 256 = 3,480 ms.
- Retargeting (a second wheel notch while animating) starts a new Hermite segment from the current zoom, carrying the current log-velocity, clamped to 3 x |distance| / duration and only if it points toward the new target.
- "Animate zoom in and zoom out" off (`AnimnateZoom`, spelling is the real registry name): no animation, jump straight to the target.
- "Telescope zoom out" (`TelescopeZoomOut`, default on, no UI in Options): on exit, animate back to 1.0 before hiding. Skipped when leaving from a LiveZoom-originated zoom.
- While `zoomAnimation.IsActive()`, the stretch mode is HALFTONE and the viewport is drawn at fractional coordinates. When settled the user's "Smooth zoomed image" setting applies (HALFTONE if on, COLORONCOLOR if off).

### 2.4 How the zoomed view follows the mouse (exact mapping)

`GetZoomedTopLeftCoordinates` (line 1493), also used for fractional drawing by `GetAnimatedZoomSourceCoordinates` (line 1504). Coordinates are in monitor-local pixels of the 1:1 snapshot. `W`,`H` = monitor size, `z` = current zoom, `(cx, cy)` = cursor.

```
sw = W / z            sh = H / z              // size of the visible source region
x  = clamp( cx - (cx / W) * sw , 0 , W - sw ) // proportional mapping
y  = clamp( cy - (cy / H) * sh , 0 , H - sh )
m  = sw / 8   (horizontal margin), n = sh / 8 (vertical margin)   // LIVEZOOM_MOVE_REGIONS = 8
if (cx - x) < m           then x = max(0, cx - m)
else if (x + sw - cx) < m then x = min(cx + m - sw, W - sw)
// same for y with n
```

Meaning: with the cursor at the left edge the view shows the left edge; at the right edge it shows the right edge; in between the view slides linearly. Before the margin step, the content drawn under the cursor is exactly the content that was under it at 1x (`x + cx / z = cx`), so the pointer keeps pointing at the same item as it moves. The margin step breaks that identity slightly near the edges. The extra 1/8 margin then keeps the cursor at least `sw/8` (or `sh/8`) away from the visible edge, so the view does not sit exactly on the cursor at the extremes. The visible region is drawn scaled to fill the whole monitor: `StretchBlt(0,0,W,H <- x,y,sw,sh)`.

Equivalent in screen terms: translation = -cursor x (z - 1). KWin's Zoom effect uses the same expression in its "Proportional" tracking mode (`m_xTranslation = -int(trackPoint.x() * (m_zoom - 1.0))`), and ZoomIt for Mac ports the same margin (`moveRegions = 8`, `adjustToMoveBoundary`).

This mapping is used in static zoom, in LiveZoom while the zoom level is animating, and in save/crop maths.

### 2.5 Keys and mouse in zoom mode (before entering pen mode)

| Input | Effect |
|---|---|
| Mouse move | Pans the view by the mapping above. The system cursor stays visible. |
| Wheel up or Up arrow | Zoom in (2.2). |
| Wheel down or Down arrow | Zoom out. |
| Left click | Enter drawing mode (section 3). Ignored while the zoom animation is still running (`zoomTelescopeTarget == zoomLevel` required). |
| Right click | Exit zoom (`WM_USER_EXIT_MODE`: not drawing, so post the zoom hotkey). |
| Esc | Exit zoom. |
| `T` or `Shift+T` | Enter type mode; also enters pen mode first (`WM_KEYUP` for `T`, with lParam 0 path). Acts on key up. |
| `R G B O Y P` (`Shift` for highlighter) `W K` `X` | Change pen colour even before entering pen mode. |
| Ctrl+K / Ctrl+W | Blank the screen black or white (section 3.7). This also enters pen mode. |
| Ctrl+C, Ctrl+Shift+C, Ctrl+S, Ctrl+Shift+S | Copy or save (section 5). |
| Zoom hotkey again | Exit zoom. |
| Left/Right arrows | No effect in zoom or draw mode (they only change the break timer). |

Exiting zoom moves the real cursor to where the pointer was in the zoomed view (`SetCursorPos(monitor.left + cursorPos.x, ...)` after the animation) so the unzoomed screen does not jump.

## 3. Draw mode

Source: https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L9522-L9870 (keys), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L9872-L10190 (mouse move), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L10192-L10500 (buttons), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L10625-L10685 (exit mode), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L1130-L1330 (blur and highlight), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L6080-L6290 (arrow and shapes), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L6498-L6540 (pen width), https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomIt.h (constants), https://learn.microsoft.com/en-us/sysinternals/downloads/zoomit.

Two ways in, one state machine. Draw mode is the `g_Drawing` flag inside the same full-screen window as zoom.

- From zoom: left click (2.5).
- Draw without zoom, Ctrl+2 (`DRAW_HOTKEY`, line 8112): sends itself the zoom hotkey with lParam 0 (no zoom-in animation), forces `zoomLevel = zoomTelescopeTarget = 1`, then sends a left-button-down to enter pen mode at once. The result is a frozen 1:1 screenshot you draw on. The wheel still zooms it (2.2) because zoom and draw share one window.

### 3.1 Entering and leaving pen mode

| Action | Effect |
|---|---|
| Left click (not yet in pen mode) | Pen mode on. The system cursor is hidden and replaced by a drawn pen cursor. The real cursor is confined to the visible region with `ClipCursor(boundRc)` where `boundRc` is the source rectangle from 2.4. The first click does not draw. |
| Left press while in pen mode | Starts a stroke or shape. Left release ends it. A click without movement draws a dot (restored in v9.01). |
| Right click, in pen mode | Leaves pen mode (back to plain zoom, view stays), puts the real cursor back at the position it had in the zoomed view when zoom is above 1, releases the clip, turns Sticky Keys handling back on. Also ends a stroke in progress. |
| Right click, not in pen mode | Exits zoom or draw entirely. |
| Right click during type mode | Leaves type mode only. |
| Esc, any state except type mode | Exits zoom or draw entirely (`VK_ESCAPE` posts the zoom hotkey). Esc during type mode leaves type mode only. |
| Space | If in pen mode and not mid-stroke, warp the cursor to the centre of the visible region. |

So, from the Ctrl+2 draw-only state: first right click leaves pen mode, second right click exits. Esc exits in one step. DOC-SYS lists "Exit: Esc or Right-Click". UNCERTAIN whether the two-step right click is intended for the draw-only case; it follows directly from `WM_USER_EXIT_MODE` (line 10625).

Esc and right click by state (source: `WM_KEYDOWN` `VK_ESCAPE`, `WM_RBUTTONDOWN`, `WM_USER_EXIT_MODE`):

| State | Esc | Right click | Same hotkey again |
|---|---|---|---|
| Static zoom, not in pen mode | Exit zoom | Exit zoom | Ctrl+1 exits |
| Pen mode entered from zoom | Exit zoom and draw | Leave pen mode, stay zoomed; a second right click exits | Ctrl+1 exits |
| Draw without zoom (Ctrl+2), in pen mode | Exit | Leave pen mode, screen stays frozen at 1x; second right click exits | Ctrl+2 does nothing while already zoomed (`DRAW_HOTKEY` only acts when `!g_Zoomed`); Ctrl+1 exits |
| Type mode | Leave type mode only | Leave type mode only | none |
| LiveDraw | Remove annotations and leave LiveDraw | Leave pen mode first (UNCERTAIN what happens next) | LiveDraw key or LiveZoom key ends both |
| Break timer | Exit | Exit | Ctrl+3 again re-runs the start code and resets the countdown (UNCERTAIN, see 6) |
| Region selection (`SelectRectangle`) | Cancel | not handled | any other ZoomIt hotkey cancels the selection |
| DemoType typing | Physical Esc kills typing | none | Ctrl+7 while typing is ignored (`StartDemoType` returns early if active) |
| LiveZoom (no draw) | Input never reaches ZoomIt | Same | Ctrl+4 exits |

While in pen mode the mouse moves at 1:1 in source pixels (the cursor is clipped to the source rectangle), so on-screen pen speed is `z` times the source speed. Changing zoom in pen mode re-computes the clip (`BoundMouse`).

### 3.2 Pen width

| Item | Value |
|---|---|
| Default | 5 (`PEN_WIDTH`) |
| Range in normal use | 2 to 40 (`MIN_PEN_WIDTH`, `MAX_PEN_WIDTH`) |
| Range when drawing on a zoom that came from LiveZoom | up to 40 x live zoom level, hard cap 600 (`MAX_LIVE_PEN_WIDTH`); the user's delta is multiplied by the live zoom level |
| Change | Ctrl + wheel, or Ctrl + Up/Down, one unit per notch (`ResizePen`, line 6498) |
| Persistence | Written to the registry (`PenWidth`) on every change |
| Options dialog | No pen-width control in this version's Draw tab (code still refers to `IDC_PEN_WIDTH`, spin range 1-19; the dialog template has no such control). UNCERTAIN. |

Ctrl + wheel changes pen width only in pen mode. In plain zoom mode (not in pen mode) Ctrl + wheel zooms.

Changing the width ends the current pen-mode state and re-enters it (`ResizePen` sends `WM_LBUTTONDOWN` with wParam -1), which redraws the cursor at the new size and does not push an undo snapshot.

### 3.3 Colours

Keys are handled on `WM_KEYDOWN` while in zoom or break-timer mode and not in type mode. Values from `ZoomIt.h`:

| Key | Colour | RGB |
|---|---|---|
| R | Red (default) | 255, 0, 0 |
| G | Green | 0, 255, 0 |
| B | Blue | 0, 0, 255 |
| O | Orange | 255, 128, 0 |
| Y | Yellow | 255, 255, 0 |
| P | Pink | 255, 128, 255 |
| W | White | 255, 255, 255 |
| K | Black | 0, 0, 0 |
| X | Blur (sentinel colour, not drawn) | 112, 112, 112 (`COLOR_BLUR`) |

The colour is saved to the registry on each change. A pen colour of exactly (112,112,112) is treated as blur. Colour also colours typed text (section 4).

Break timer uses the same keys for the timer text colour (own setting `BreakPenColor`); Ctrl+W and Ctrl+K there set the background (section 6).

### 3.4 Highlighter (Shift + colour key)

Shift + R/G/B/O/Y/P/W/K selects a highlighter variant of that colour. It is a "marker" effect, not alpha blending:

- The pen colour is flagged as highlighter by its alpha byte (`PEN_COLOR_HIGHLIGHT`: alpha != 0xFF). The stored alpha is `g_AlphaBlend = 0x80`.
- Drawing renders the stroke into an off-screen ARGB bitmap, then for every pixel where that bitmap's alpha is non-zero the destination pixel is replaced by `highlight AND pixel`, per channel (`BlendColors`, line 1278). The alpha value is not used in that path (`alpha2 = 0`).
- `highlight` is the pen colour lightened per channel by `AdjustHighlighterColor` (line 1262): `c' = c ? min(255, c + 0x40) : 0x80`.
- Effect: white and light backgrounds take on the colour, dark text stays dark, black stays black. Overlapping strokes darken further because the AND is applied to the already-tinted pixel.
- Highlighted rectangles and ellipses are filled with that same operation over the shape interior (brush alpha `g_AlphaBlend / 2` is used only to find covered pixels). Highlighted lines use the pen width as the band thickness.
- Arrows have no highlight branch in `DrawShape`. With a highlighter colour selected they are drawn through GDI+ with the colour's alpha of 0x80, so roughly half opaque. UNCERTAIN, read from code only.
- There is a red/blue channel swap between `COLORREF` and `Gdiplus::Color` in `BlendColors` (`blue2 = GetRed()`, `red2 = GetBlue()`). UNCERTAIN whether this is deliberate compensation; I did not test the visible result.
- Highlighter and blur are disabled in LiveDraw (keys are ignored).
- The pen cursor for highlighter and blur is a small cross (arm length 4) instead of a filled circle (`DrawCursor`, line 6424).

### 3.5 Blur (X, Shift+X)

- `X` selects blur with radius 20 (`NORMAL_BLUR_RADIUS`). `Shift+X` selects strong blur, radius 40 (`STRONG_BLUR_RADIUS`).
- Implementation: for each stroke segment, copy the pixels under the segment's bounding box into a GDI+ bitmap, run `Gdiplus::Blur` with `radius = g_BlurRadius`, `expandEdge = FALSE` (`BitmapBlur`, line 1130), then write the blurred pixels back only where the stroke mask is non-zero (`BlurScreen`). Blur is therefore applied to whatever is on the canvas at that moment, including earlier ink, and is baked into the bitmap (no live re-blur).
- Works with freehand, line, rectangle and ellipse (`DrawBlurredShape`). The arrow case has no blur branch, so an arrow drawn with blur selected comes out as a plain grey (112,112,112) arrow. UNCERTAIN, read from code only.
- Not available in LiveDraw.

### 3.6 Shapes and modifiers

The modifier is read at mouse-button-down (`WM_LBUTTONDOWN`, line 10192). It is `MK_SHIFT`, `MK_CONTROL` from the message, plus `GetKeyState(VK_TAB)` for Tab. Hold the modifier, press left, drag, release.

| Held at button-down | Shape |
|---|---|
| nothing | Freehand |
| Shift | Straight line |
| Ctrl | Rectangle |
| Tab | Ellipse |
| Ctrl + Shift | Arrow |

Rendering details:
- While dragging, shapes are drawn as an XOR ("rubber band", `R2_NOT`) outline and erased by redrawing; highlight and blur shapes restore the pre-drag bitmap each move.
- On release, the final shape is drawn with GDI+ anti-aliasing (not in LiveDraw). Freehand uses round caps and round joins. Rectangles and ellipses are outlines with no fill (unless highlighter or blur).
- Rectangle and ellipse are normalised so the drag can go in any direction. There is no Shift-for-square or Shift-for-circle constraint.
- Straight line snap ("SnapToGrid", default on, no UI): on release, if `|dy| < |dx| / 10` the line is made exactly horizontal; if `|dx| < |dy| / 10` it is made exactly vertical. Applies to lines and arrows only.
- Arrow geometry (`DrawArrow`, line 6080, called from `DrawShape` line 6171): head length = 2.5 x pen width, head half-width = 1.5 x pen width; the head is drawn as an outlined triangle (six-segment path, round join), then the line. The arrow head is at the mouse-down point (the code passes `Rect.left/top` as the tip; the drag end is the tail). UNCERTAIN: I read this from argument order only, not by running the app.
- Freehand strokes are drawn segment by segment as the mouse moves, each with `DrawLine` round caps.
- Ctrl (rectangle) alone is not a modifier for anything else; there is no Ctrl+drag "move".

### 3.7 Screen blanking (sketch pad), erase, undo

| Input | Effect |
|---|---|
| Ctrl+W | Fill the whole canvas white. Pushes an undo snapshot. Also enters pen mode if not in it. |
| Ctrl+K | Fill the whole canvas black. Same. |
| E | Erase all drawing: restore the pristine snapshot taken at zoom entry (or, in LiveDraw, blank to transparent black). Clears the undo list. Not allowed in type mode. |
| Ctrl+Z | Undo one step. Snapshots are whole-canvas bitmaps, kept in a list of at most 32 (`MAX_UNDO_HISTORY`); oldest dropped first. Works only when something has been drawn and no stroke is in progress. There is no redo. |
| Pen inverted (eraser end) | On a tablet pen, entering inverted state pops one undo step. |

The undo snapshot is pushed at each left-button-down that starts a stroke or shape, at blanking, and when type mode is entered.

### 3.8 Tablet pen

`WM_POINTER*` handlers translate pen input into synthetic mouse messages tagged with a signature (`MI_WP_SIGNATURE`). Pen tip down enters pen mode and draws. Pen lift sends a synthetic right-button-down (exit pen mode). Pen coordinates are scaled by the zoom (`ScalePenPosition`, line 6300) instead of clipping the cursor. Mouse messages that arrive while a pen is down and lack the signature are ignored.

## 4. Type mode (`T`, `Shift+T`)

Source: https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L9413-L9520 (`WM_CHAR`, `WM_KEYUP`), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L9247-L9330 (font size on wheel), https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomIt.rc (Type tab text), https://learn.microsoft.com/en-us/windows/powertoys/zoomit.

Entered on key up of `T` (`WM_KEYUP`, line 9465), from zoom or draw mode. If not already in pen mode it enters pen mode first. `Shift` held at key up gives right-aligned input, otherwise left-aligned.

| Item | Behaviour |
|---|---|
| Text colour | Current pen colour, RGB only (`SetTextColor(g_PenColor & 0xFFFFFF)`), so highlighter alpha does not change text |
| Font | Windows default GUI font at entry, regular weight; changed with the Type tab's Font button (`Font` registry blob). PowerToys page: default "Microsoft Sans Serif". |
| Size | `lfHeight = max( (screenHeight / zoom) / FontScale , 12 )`, `FontScale` default 10, so about one tenth of the visible source height. Text at a size under 20 px is drawn without anti-aliasing. |
| Size change | Wheel up or Up arrow = larger (`FontScale` decreases by the delta), wheel down or Down = smaller. `FontScale` range -20 to 50 in the code; 0 is skipped to 1. Negative values look unintended. UNCERTAIN. No Ctrl needed. |
| Position | Before the first character, the caret follows the mouse. First left click fixes the position. |
| Typing | Printable characters are drawn into the canvas as you type. Enter starts a new line at the original x. Backspace or Delete removes the last character (or, right-aligned, re-renders). |
| Right-aligned mode | Text grows leftward from the anchor; previous lines are kept and re-rendered on each key so the right edge stays fixed. |
| Exit | Esc, right click, or a left click after the position was fixed (a click before any typing only fixes the position). |
| Undo | Ctrl+Z is swallowed once a character has been typed (the printable-key branch returns before the `Z` case). Before the first character it undoes the last canvas snapshot. Use Backspace to delete text. |

Note the "Ctrl + wheel for font size" in DOC-SYS does not match source (0.1).

## 5. Copy and save (static zoom only)

Source: https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L10875-L11340 (`IDC_SAVE`, `IDC_COPY`), https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/SelectRectangle.cpp, https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/SelectRectangle.h.

Available while zoomed or in draw mode. The hotkeys are registered only then (1).

| Key | Action | Detail |
|---|---|---|
| Ctrl+C | Copy the whole zoomed view | Copies the monitor's screen contents including zoom and drawing, as it is displayed (`IDC_COPY`, line 11158 onward), as `CF_BITMAP`. It does not add the actual-size version. |
| Ctrl+Shift+C | Copy a cropped region | Shows `SelectRectangle` (crosshair cursor, drag a rectangle, release to accept, Esc cancels). Copies that part of the displayed view. |
| Ctrl+S | Save the whole view | Save As dialog titled "ZoomIt: Save Zoomed Screen...", default file type "Zoomed PNG", suggested unique name, remembers the last folder. |
| Ctrl+Shift+S | Save a cropped region | Same dialog after `SelectRectangle`. |

Save formats (six filter entries, `IDC_SAVE`, line 10875 onward): Zoomed PNG, Actual size PNG, Zoomed WebP, Actual size WebP, Zoomed JPG, Actual size JPG.
- "Zoomed" = the view at screen resolution, upscaled from the source bitmap.
- "Actual size" = the 1:1 source pixels of the visible (or cropped) region, before magnification. It is computed from `viewport + copyRect / zoom`.
- If "Copy to clipboard when saving" is on (`SnipCopyToClipboard`, default off), the actual-size image is also placed on the clipboard after saving.
- The last chosen path is stored (`ScreenshotSaveLocation`).

`SelectRectangle` (`SelectRectangle.cpp/.h`), used for every region pick (crop copy or save, snip, OCR, record region, panorama, DemoMirror region):
- A layered, topmost, full-monitor black window at alpha 176 with the selected rectangle cut out with `SetWindowRgn`, so the outside is dimmed and the inside is clear. Crosshair cursor. The cursor is clipped to the monitor that held the pointer at start.
- Left press sets the anchor, drag sizes, left release accepts. Esc cancels. Losing focus before release cancels. Minimum size 34 px. Optional 16:9 lock (landscape: width drives height; portrait: height drives width).
- After release the window becomes click-through at alpha 191 and shows a 2 px border: 1 px colour plus 1 px black inside (colour only while recording). Border colours: yellow RGB(255,222,0) default (matches the Windows capture border), orange RGB(255,127,39) once a recording has delivered its first frame, green RGB(0,255,0) for DemoMirror, blue for panorama.
- For recording the whole monitor it draws only the border (`fullMonitor` mode) and returns immediately.

## 6. Break timer (Ctrl+3)

Source: https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L11349-L11470 (`IDC_BREAK`), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L11599-L11780 (paint), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L9376-L9400 (wheel in timer), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L1734-L1750 (opacity), https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomIt.rc (Break tab), https://learn.microsoft.com/en-us/windows/powertoys/zoomit.

Entry: Ctrl+3 (only when not in zoom and not in LiveZoom above 1x), or tray menu "Break Timer" (`BREAK_HOTKEY` line 8411, `IDC_BREAK` line 11349). Pressing the zoom hotkey while a timer runs ends it.

| Item | Behaviour |
|---|---|
| Window | Full-monitor borderless window on the monitor holding the cursor. It is not topmost, so Alt+Tab away works and the countdown continues. Double-clicking the tray icon brings a running timer forward (`WM_LBUTTONDBLCLK`). The tray menu item "Break Timer" re-runs the start code, which sets `breakTimeout` back to the full length, so it appears to restart the countdown. UNCERTAIN (see 0.1). |
| Length | `BreakTimeout` minutes, default 10, allowed 1 to 99 (Options). Stored as seconds `minutes x 60 + 1`; a tick every 1000 ms (timer id 0). |
| Display | `m:ss` in the text font (Type tab font) at height `screenHeight / 5`. Below 0:00 it shows `0:00` and, if "Show Time Elapsed After Expiration" is on (default on), a second line `(-m:ss)` in a font of `screenHeight / 8`. The countdown never stops by itself; the user exits. |
| Position | 3 x 3 grid, `BreakTimerPosition` 0 to 8 left to right, top to bottom, default 4 (centre). 50 px margin from the screen edge. |
| Colours | Timer text colour: same keys as the pen (R G B O Y P W K), stored as `BreakPenColor`, default red. Background: `Ctrl+W` white (default) or `Ctrl+K` black, stored as `BreakBackgroundColor`. |
| Opacity | Window alpha 10% to 100% in steps of 10 (`BreakOpacity`, default 100). |
| Background image | Off by default. When on: either the desktop, faded (snapshot alpha-blended over black with constant alpha 0x4F, about 31%, `CreateFadedDesktopBackground`, line 1432) or an image file, drawn centred or stretched to the screen ("Scale to screen"). |
| Sound | Optional file; `PlaySound` with `SND_FILENAME` and `SND_ASYNC` when the timer reaches exactly 0. |
| Lock workstation | Optional. Runs the timer as an embedded screensaver on the secure desktop, so unlocking needs a sign-in. Registry `BreakLockWorkstation`. |
| Side effects | Screen saver, display power-off and low-power timers are disabled for the duration (`EnableDisableScreenSaver`). |
| Registry-only | `BreakOnSecondary` (show on a second display; has helper `EnableDisableSecondaryDisplay`, no UI). UNCERTAIN what it does beyond that. |

Keys in the timer:

| Input | Effect |
|---|---|
| Wheel up or Up arrow | Add whole minutes. From a non-whole-minute value, the first notch rounds up to the next minute. |
| Wheel down or Down arrow | Subtract whole minutes. The same rounding is applied before subtracting, so from 9:30 one notch gives 8:00, not 9:00. UNCERTAIN whether that is intended (`WM_MOUSEWHEEL`, line 9376). Clamped at 0. |
| Right arrow / Left arrow | +10 s / -10 s, snapped to a multiple of 10. Clamped at 0. |
| R G B O Y P W K | Timer text colour. |
| Ctrl+W / Ctrl+K | Background white / black. |
| Esc or right click | Exit the timer. |

## 7. LiveZoom (Ctrl+4) and LiveDraw (Ctrl+Shift+4)

Source: https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L11837-L12330 (`LiveZoomWndProc`), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L8466-L8540 (`LIVE_HOTKEY`), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L8083-L8158 (`LIVE_DRAW_HOTKEY`), https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomIt.rc (LiveZoom tab).

### 7.1 LiveZoom

LiveZoom magnifies the live desktop. The desktop stays interactive.

Implementation (`LiveZoomWndProc`, line 11837 onward; `LIVE_HOTKEY`, line 8466): a full-monitor `MagnifierClass` window, layered and transparent to input (`WS_EX_LAYERED|WS_EX_TRANSPARENT`), driven by the Windows Magnification API (`MagSetWindowTransform`; alternate path `MagSetFullscreenTransform` + `MagSetInputTransform`). The real cursor is hidden and the magnifier draws a magnified cursor (`MS_SHOWMAGNIFIEDCURSOR`). Updates run from a 10 ms timer.

| Item | Behaviour |
|---|---|
| Start | Ctrl+4. Zoom animates from 1.0 to the initial magnification (2.0 by default). |
| Zoom in | Ctrl+Up: level x 2 (up to 256). Not the wheel, because mouse and keyboard input never reach the window. |
| Zoom out | Ctrl+Down: level / 2 above 2, or x 0.75 at or below 2, floor 1. Animated. |
| Exit | Ctrl+4 again. Also reaching level 1 hides the window. |
| View follows mouse, while zoom is animating | Same proportional mapping as static zoom (2.4). |
| View follows mouse, at steady zoom | Edge scrolling. Divide the visible source rectangle into 8 bands per axis. The view stays put while the cursor is in the inner 6 bands, and shifts by the overshoot once it enters an outer band, so the cursor can reach the screen edge. |
| Smoothing | `MagSetLensUseBitmapSmoothing` follows the "Smooth zoomed image" setting; forced on during animation. |

Interactions:
- Ctrl+1 while LiveZoom is on freezes the current magnified view and switches to static zoom (`g_ZoomOnLiveZoom`), scaling pen width by the live zoom level. Exiting static zoom returns to LiveZoom. UNCERTAIN on the level restored (the code forces 2.0 in one path).
- Ctrl+2 while LiveZoom is on enters draw mode on a frozen copy of the magnified view; Esc returns to LiveZoom (LiveZoom tab text).
- Recording while LiveZoom is on uses a workaround that keeps the magnifier alive at level 1 (`WINDOWS_CURSOR_RECORDING_WORKAROUND`, `g_LiveZoomLevelOne`).

### 7.2 LiveDraw

Ctrl+Shift+4 (or the LiveZoom key with Shift flipped). The drawing window is made layered with black as the transparent colour key (`LWA_COLORKEY`), so a black canvas is see-through and the live desktop shows below. The magnifier's filter list is updated so the overlay is not magnified twice.

- Works with or without LiveZoom active.
- Esc removes all annotations and leaves LiveDraw. `E` erases (blanks to transparent black). `Ctrl+K` and `Ctrl+W` sketch pads are blocked.
- Highlighter, blur and wheel zoom are disabled. Anti-aliasing is skipped whenever the window is layered.
- Snip, OCR and panorama hotkeys are ignored while LiveDraw over LiveZoom is active (comment "due to mirroring bug" in the `SNIP_HOTKEY`, `SNIP_PANORAMA_HOTKEY` and `SNIP_OCR_HOTKEY` cases).
- The pen follows the mouse via a 10 ms poll (timer 3) because mouse-up can be missed; a synthetic button-up is sent when the physical button is up.

## 8. Record (Ctrl+5, Ctrl+Shift+5, Ctrl+Alt+5)

Source: https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L8542-L8738 (hotkeys), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L7083-L7345 (`StartRecordingAsync`), https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomItSettings.h, https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomIt.rc (Record and Webcam tabs), https://learn.microsoft.com/en-us/windows/powertoys/zoomit.

| Item | Behaviour |
|---|---|
| Start and stop | The same hotkey toggles. Options text: "Record video of the unzoomed live screen or a static zoomed session". |
| Source, Ctrl+5 | The monitor under the cursor at start. A full-monitor border is drawn (yellow, turning orange when the first frame arrives). |
| Source, Ctrl+Shift+5 | `SelectRectangle` region (see 5), optional 16:9 lock ("Lock region selection to 16:9"). Not allowed while already recording. |
| Source, Ctrl+Alt+5 | The top-level window under the cursor (`WindowFromPoint` then parent walk). If that is the desktop, records the monitor instead. |
| Engine | Windows.Graphics.Capture (WGC) with Direct3D11, so it records the composited output including ZoomIt's own zoom overlay. Needs a GPU-backed display; otherwise the run is cancelled with an explanatory box. |
| Temp file | `%TEMP%\ZoomIt\zoomit.mp4` or `zoomit.gif`. |
| Format | MP4 (default) or GIF. |
| Frame rate | 30 for MP4, 15 for GIF, fixed. A combo for 15, 24, 30, 60 exists in the dialog template but is hidden. |
| Scaling | 10% to 100% in steps of 10. Defaults: MP4 100%, GIF 50%. Stored per format. |
| Audio (MP4 only) | System audio, default on. Microphone, default off. Microphone device choice. Mono mix, default off. Noise cancellation, default on (the source tree bundles `rnnoise` and `NoiseSuppressor.cpp`). |
| Webcam overlay (MP4) | Off by default. Camera device, position (top-left, top-right, bottom-left, bottom-right; default bottom-right), size (small 15%, medium 25% default, large 33%, extra-large 50%; PowerToys docs add "Full screen"), shape (rectangle, rounded rectangle, rounded square, circle), background (none, blur, image; uses the MediaPipe SelfieSegmentation ONNX model), brightness 0 to 100 (default 50). While recording, Ctrl+C toggles the overlay. A movable preview window shows on screen. |
| After stop | One dialog combines Save As and the trim editor (`ShowSaveDialogWithTrim`). Default name `Recording.mp4`; a default-named save gets a timestamp suffix, e.g. `Recording 2025-11-03 143015.mp4`; a custom name gets `(1)`, `(2)` suffixes on collision. Last folder is remembered. Starting folder falls back to the user's Videos folder. |
| Trim editor | Preview, timeline, play/pause, skip, step, volume slider, "Delete Region", "Append..." (add another clip). PowerToys docs: transitions between clips are "No Transition (Hard Cut)", "Fade to Black (0.5s)" or "Fade to White (0.5s)". The Record tab also has a "Trim" button that opens an existing video ("ZoomIt: Open Video for Trimming..."). |
| Empty result | If no frame was ever captured the hotkey is treated as never pressed and the temp file is deleted. |

## 9. Snip, OCR and panorama (Ctrl+6, Ctrl+Shift+6, Ctrl+Alt+6, Ctrl+8, Ctrl+Shift+8)

Source: https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L8159-L8395 (`SNIP_*` hotkeys), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L11234-L11330 (`IDC_COPY_OCR`), https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/PanoramaCapture.cpp (header comment), https://learn.microsoft.com/en-us/windows/powertoys/zoomit.

### 9.1 Snip

Path (`SNIP_HOTKEY`, line 8159 onward):
1. If not zoomed, enter static zoom at 1x (hidden internal path so the screen is frozen). If already zoomed or drawing, leave pen mode first (twice if needed) to hide the pen cursor.
2. Run the crop copy or crop save code from section 5 (`IDC_COPY_CROP` for Ctrl+6, `IDC_SAVE_CROP` for Ctrl+Shift+6).
3. If snip started from idle, leave zoom again.

Ctrl+6 puts the region on the clipboard as a bitmap. Ctrl+Shift+6 opens the Save As dialog (PNG, WebP, JPG; zoomed and actual-size variants apply when snipping from a zoomed view). If "Copy to clipboard when saving" is on, a save also copies. When snipping while zoomed, the region is the crop of the displayed zoomed view and includes annotations drawn so far. ZoomIt stays in zoom afterwards, with pen mode left.

### 9.2 OCR snip (Ctrl+Alt+6)

Same freeze and region pick. The crop is run through Windows.Media.Ocr (`OcrFromHBITMAP`) and the recognised text is put on the clipboard as Unicode text. Nothing is copied if the result is empty. Language depends on the Windows OCR language packs (UNCERTAIN: I did not read the language selection code).

### 9.3 Panorama (Ctrl+8 clipboard, Ctrl+Shift+8 file)

Source: `PanoramaCapture.cpp` (header comment describes the algorithm).

1. Press the hotkey. Pick a region (blue border). Any zoom or pen mode is left first.
2. Scroll the content under the region slowly and at a constant rate, without reversing.
3. Press the hotkey again to stop. The result goes to the clipboard, or to a Save As dialog for Ctrl+Shift+8 (PNG, WebP or JPEG). Other hotkeys are ignored while a panorama is active.

Algorithm summary from the source comment:
- Capture: grab the absolute screen rectangle about every 16 ms. Drop frames that are near duplicates of the previous one (mean per-pixel RGB difference below 6, sampling every 6th pixel with a 2.5% margin). Cap on the number of frames (`kMaxCaptureFrames`).
- Stitch: for each consecutive pair, find the vertical shift by minimum mean absolute luma difference. Coarse search on 4x downsampled luma (2x for frames under 240 px tall), shortlist the best 12, refine at full resolution (+-1 px horizontally). SSE2 on x64. Reject a pair if the frames are identical (stationary score <= 2) or if a large shift matches suspiciously well against a low stationary score (guards against repeating layouts). Compose the accepted frames onto a canvas. Fixed headers, footers and scrollbars inside the region hurt the result, and the Options text tells users to exclude them.
- Vertical scrolling is the documented case. UNCERTAIN whether horizontal scrolling is supported.

## 10. DemoType (Ctrl+7, Ctrl+Shift+7)

Source: https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/DemoType.cpp, https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/DemoType.h, https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomIt.rc (DemoType tab), https://techcommunity.microsoft.com/blog/sysinternals-blog/make-demo-typing-easy-with-demotype-in-zoomit-v8-0/4050566, https://learn.microsoft.com/en-us/sysinternals/downloads/zoomit.

Files: `DemoType.cpp/.h`. Sources for syntax: the DemoType tab text in `ZoomIt.rc`, `HandleControlKeyword`, and the Sysinternals blog post "Make demo typing easy with DemoType in ZoomIt v8.0".

What it does: types a prepared script into whichever window has focus, synthesising keystrokes, so a presenter does not have to type live.

### 10.1 Script file

| Item | Rule |
|---|---|
| Input | The clipboard if its text starts with `[start]` (the prefix is a safety check and is stripped); otherwise the text file chosen in Options. The clipboard is checked first on every activation. |
| Reload | The file is re-read when its modification time is newer than the last read. |
| Encoding | UTF-8, UTF-8 with BOM, UTF-16LE, UTF-16BE. |
| Size | 1 MiB maximum (`MAX_INPUT_SIZE` 1,048,576 bytes). Larger files give "Unsupported DemoType file size". |
| Snippets | Separated by the keyword `[end]`. Each hotkey press types the next snippet. At the end of the file it wraps to the start (the file is reloaded). |
| Newlines around `[end]`, `[paste]`, `[/paste]` | A newline immediately before `[end]`/`[/paste]`, and immediately after `[end]`/`[paste]`, is trimmed (`TrimNewlineAroundControl`). |
| Errors | "Error loading DemoType file", "No DemoType file specified", "Unrecognized DemoType file content". |

Control keywords (case-sensitive, longest `[pause:000]`, at most 11 characters):

| Keyword | Effect |
|---|---|
| `[end]` | Ends the snippet. In automatic mode it stops typing at once. |
| `[pause:n]` | Wait `n` seconds (whole seconds). Ignored in user-driven mode. |
| `[paste]` ... `[/paste]` | Put the enclosed text on the clipboard and send Ctrl+V. The user's clipboard is overwritten. It is restored at the end only when the script itself came from the clipboard (`[start]` mode). |
| `[enter]` `[up]` `[down]` `[left]` `[right]` | Send that key. |
| Any other `[...]` | Treated as literal text. |

Text handling: characters go in as Unicode key events (`KEYEVENTF_UNICODE`, `VK_PACKET`), so any character works. Characters that trip an editor's auto-formatting are pasted through the clipboard instead: `\n`, `\t`, a space after a newline, `{`, `[`, `(`, and `*` after `/`. A trailing `[` in a pasted chunk is deleted again with Backspace because it might begin a keyword. Before typing into a smart editor, DemoType probes the current line's indentation by pressing Shift+Left and Ctrl+C repeatedly (up to `MAX_INDENT_DEPTH` 100) and reading the clipboard, then adjusts what it injects. Notepad (detected by window class) skips the probe. Details of the indentation logic: UNCERTAIN, only skimmed.

### 10.2 Running

| Mode | Behaviour |
|---|---|
| Automatic (default) | Ctrl+7 starts typing the current snippet at the set speed. Physical keyboard input is blocked while it types, using a low-level keyboard hook that lets only ZoomIt's own injected keys through. Control returns at `[end]`. |
| User-driven ("Drive input with typing") | After Ctrl+7, each real key press you make is swallowed and replaced with 1 to 3 characters of the script (the count depends on the speed slider: 1 for slow, up to 3 for fast). At the end of a snippet, press Space to unblock the keyboard. |
| Step back | Ctrl+Shift+7 moves the read position back to the previous `[end]`. |
| Kill | Esc (a physical Esc press) or a change of foreground window ends typing. The read position then jumps to just after the next `[end]`. |
| Modifier keys | Modifiers still held from the hotkey are released, and Caps Lock is switched off if it was on, at start (`BlockModifierKeys`). |

Speed: slider 10 to 100 in the Options tab, default 55 (higher is faster). Per character delay = `110 - slider` ms with random variance of plus or minus 100% of that value (range 1 ms to 2 x delay). Default about 55 ms average.

## 11. DemoMirror (Ctrl+9, Ctrl+Shift+9, Ctrl+Alt+9)

Source: https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L8740-L8880, https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/MirrorWindow.h, https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomIt.rc (DemoMirror tab), https://learn.microsoft.com/en-us/windows/powertoys/zoomit.

Added in v12.2 (`MirrorWindow.cpp/.h`, `MIRROR_*_HOTKEY`, line 8740 onward). Mirrors the screen, a region, or a window, including the mouse pointer, onto another monitor, so the audience sees the demo on the presentation display without the presenter leaving the slide show.

| Item | Behaviour |
|---|---|
| Ctrl+9 | Mirror the whole monitor under the cursor. A full-monitor green border marks it. |
| Ctrl+Shift+9 | Pick a region with `SelectRectangle` (green border stays up, excluded from capture). |
| Ctrl+Alt+9 | Mirror the top-level window under the cursor. |
| Stop | Press the hotkey again. Also stops if the mirrored window closes. |
| Target | The first other monitor (`FindMirrorTargetMonitor`). Needs a second monitor, otherwise "Screen mirroring requires a second monitor". |
| Capture | WGC, same as recording. |
| Track window region | Default on. A window mirror captures the monitor region under the window instead of the window's own surface, so ZoomIt zoom and draw appear in place, at the cost of overlapping windows showing through. Off = mirror the window's own surface. |
| While zooming or drawing | Zoom, draw and LiveZoom render in overlay windows a window capture cannot see, so a window mirror temporarily switches to capturing the monitor. |

## 12. Options dialog

Source: https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomIt.rc (dialog templates, from line 116), https://github.com/microsoft/PowerToys/blob/7fb348024855418fcb7e1c7413ba37238fdd2311/src/modules/ZoomIt/ZoomIt/Zoomit.cpp#L127-L140 (`g_OptionsTabs`), https://learn.microsoft.com/en-us/windows/powertoys/zoomit.

Title "ZoomIt - Sysinternals: www.sysinternals.com". Standalone build only; the PowerToys build opens PowerToys Settings instead. Shown automatically on first run. Common footer: "Show tray icon" (default on), "Run ZoomIt when Windows starts" (autostart). OK and Cancel. Hotkey boxes use the Windows hotkey control; a hotkey that fails to register keeps the dialog open.

Ten tabs (`g_OptionsTabs`, line 127), in this order:

| Tab | Controls |
|---|---|
| Zoom | "Zoom Toggle" hotkey. "Specify the initial level of magnification when zooming in": 6-stop slider labelled 1.25, 1.5, 1.75, 2.0, 3.0, 4.0. "Animate zoom in and zoom out" checkbox. "Smooth zoomed image" checkbox. Help text: zoom with wheel or Up/Down; exit with Esc or right click; Ctrl+C copies; Ctrl+S saves; Ctrl+Shift crops. |
| LiveZoom | "LiveZoom Toggle" hotkey. Text: use Ctrl+Up/Down for level; LiveDraw is the toggle with Shift; Esc in LiveDraw removes annotations. |
| Draw | "Draw w/out Zoom" hotkey. Text only for the rest: click to draw, Ctrl+Z, E, Space, right click; pen width with left Ctrl + wheel or arrows; colour keys; Shift for highlighter; X and Shift+X for blur; Shift, Ctrl, Tab, Ctrl+Shift for line, rectangle, ellipse, arrow; Ctrl+W and Ctrl+K sketch pads; Ctrl+C and Ctrl+S. |
| Type | "Font" button with a sample box (standard font picker: font, style, size). Text: press `t` or Shift+`t`, Esc or left click to exit, wheel or Up/Down for size, text colour is the pen colour. |
| DemoType | Input file (Browse). "DemoType toggle" hotkey. "Drive input with typing" checkbox. Typing speed slider (Slow to Fast). Text: keywords `[end]`, `[pause:n]`, `[paste]`/`[/paste]`, `[enter]` `[up]` `[down]` `[left]` `[right]`, `[start]` for clipboard text; Space to unblock in user-driven mode; Shift with the hotkey steps back. |
| Break | "Start Timer" hotkey. "Timer" minutes with spin box (1 to 99). "Show Time Elapsed After Expiration". "Lock Workstation During Break". "Advanced" button. Text: arrow keys change time, Alt+Tab away and reactivate from the tray icon, Esc exits, colour keys apply to the timer. |
| Advanced Break Options (sub-dialog) | "Play Sound on Expiration" checkbox plus "Alarm Sound File" browse. "Timer Opacity" drop-down 10% to 100%. "Timer Position" 3 x 3 radio grid. "Show background bitmap" checkbox with radios "Use faded desktop as background" and "Use image file as background", a file browse, and "Scale to screen". |
| Record | "Record Toggle" hotkey. "Scaling" drop-down (10% to 100%). "16:9" checkbox (lock crop selection). "Format" drop-down (GIF, MP4). "Frame Rate" drop-down exists but is hidden. "Capture system audio". "Capture audio input" with "Mono", "Noise cancellation" and a "Microphone" drop-down. "Show webcam overlay (Ctrl+C toggles)" with a "Webcam Settings..." button. A "Trim" button opens an existing video in the trim editor. Text: Shift with the hotkey crops; Alt with the hotkey records a window. |
| Webcam Settings (sub-dialog) | Camera, Position, Size, Shape, Background (None, Blur, Image) with image browse, Brightness slider 0 to 100, note "Uses MediaPipe SelfieSegmentation (Apache 2.0)". |
| Snip | "Snip Toggle", "Snip Save Toggle", "Text Toggle" (OCR) hotkeys. PowerToys adds "Copy to clipboard when saving". |
| Panorama | "Panorama Toggle" and "Panorama Save Toggle" hotkeys, with usage text. |
| DemoMirror | "Mirror Toggle" hotkey. "Track window region" checkbox. Text: Shift with the hotkey mirrors a region; Alt mirrors a window. |

Settings with no UI in this version (registry only): `TelescopeZoomOut`, `SnapToGrid`, `BreakOnSecondary`, `Theme` (0 light, 1 dark, 2 system), pen width (`PenWidth`), `FontScale`, trim dialog size and volume, `RecordingSaveLocation`, `ScreenshotSaveLocation` (also settable in PowerToys).

## 13. Defaults (from `ZoomItSettings.h`, `ZoomIt.h`)

Source: https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomItSettings.h, https://github.com/microsoft/PowerToys/blob/main/src/modules/ZoomIt/ZoomIt/ZoomIt.h.

Registry root: `HKCU\Software\Sysinternals\ZoomIt`. A registry value stores a hotkey as `(modifier flags << 8) | virtual key`.

| Setting | Default |
|---|---|
| Hotkeys | Ctrl+1 zoom, Ctrl+2 draw, Ctrl+3 break, Ctrl+4 LiveZoom, Ctrl+5 record, Ctrl+6 snip, Ctrl+Shift+6 snip save, Ctrl+Alt+6 OCR, Ctrl+7 DemoType, Ctrl+8 panorama, Ctrl+Shift+8 panorama save, Ctrl+9 mirror |
| Initial zoom | 2.0 (slider index 3) |
| Animate zoom, smooth image, telescope zoom out, snap-to-grid | all on |
| Pen colour, width | red, 5 |
| Break: minutes, opacity, position | 10, 100%, 4 (centre) |
| Break: pen colour, background | red, white |
| Break: show elapsed, background bitmap, sound, lock | on, off, off, off |
| Text `FontScale` | 10 |
| DemoType speed, user-driven | 55, off |
| Record format, scaling | MP4; 100% (MP4), 50% (GIF) |
| Frame rate | 30 (MP4), 15 (GIF) |
| Audio | system on, mic off, mono off, noise cancellation on |
| Webcam | off; bottom-right (3), medium (1), rectangle (0), background none, brightness 50 |
| Record 16:9 lock | off |
| Snip copy on save | off |
| Mirror track window | on |
| Undo depth | 32 snapshots |

## 14. What is Windows-specific in the original

Source: my reading of the files listed above; no separate upstream document.

Useful when planning a port. Each item is a mechanism a Linux version has to replace, not a behaviour to copy.

| Mechanism | Used for |
|---|---|
| `RegisterHotKey` | All global hotkeys, including the mode-only Ctrl+C/S and Ctrl+Up/Down. |
| Full-screen topmost window plus `BitBlt` from the screen DC | Static zoom and draw. The screen is captured once and the desktop is otherwise hidden. |
| Magnification API (`MagSetWindowTransform`), click-through layered window | LiveZoom. |
| Layered window with colour key | LiveDraw transparency. |
| `ClipCursor` and `SetCursorPos` | Confining the pointer to the zoomed source region and repositioning it on exit and on Space. |
| Windows.Graphics.Capture | Recording, mirror, panorama frames and the screenshot used while recording. |
| Media Foundation / WinRT encoders, `AudioGraph` | MP4, GIF, audio and trim editor. |
| Windows.Media.Ocr | OCR snip. |
| `WH_KEYBOARD_LL` hook and `SendInput` | DemoType keystroke injection and blocking of physical keys. |
| `SetLayeredWindowAttributes`, `SHQueryUserNotificationState`, `SystemParametersInfo` | Break timer opacity, screen-saver suppression. |
| Session lock/screensaver embedding | "Lock workstation during break". |
| GDI/GDI+ (`StretchBlt` HALFTONE, `Gdiplus::Blur`) | Scaling quality, pen anti-aliasing, blur. |
