// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace platform {

enum class Desktop { Gnome, Kde, Other };

// Which desktop we run under, from XDG_CURRENT_DESKTOP.
Desktop desktop();
QString desktopName();

// The Qt platform plugin in use. Only valid after QGuiApplication exists.
bool isWayland();
bool isX11();

// Environment tweaks that must happen before QApplication is created.
void prepareEnvironment();

// Is an executable available on PATH?
bool hasProgram(const QString &name);

}  // namespace platform
