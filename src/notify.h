// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace notify {

// Desktop notification through org.freedesktop.Notifications. `imagePath`
// becomes the notification's preview image when given.
void show(const QString &summary, const QString &body, const QString &imagePath = {});

}  // namespace notify
