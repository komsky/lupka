// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>

class QWindow;

// X11 helpers for the overlay. All of them are no-ops outside X11.
namespace x11util {

// Route all keyboard input to `window`. Window managers refuse focus to
// windows of background processes, so a grab is the reliable way to get
// Escape and the drawing keys. Retries for a short while, because the hotkey
// that started us may still be held by the window manager's own grab.
void grabKeyboard(QWindow *window);
void ungrabKeyboard();

// Ask the window manager to activate `window` right now.
void activate(QWindow *window);

// Keycodes that produce `keysym` in the current keyboard mapping.
QList<int> keycodesFor(quint32 keysym);

}  // namespace x11util
