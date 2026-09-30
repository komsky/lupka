// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QKeyCombination>
#include <QKeySequence>
#include <QString>

#include <cstdint>

// Conversions between Qt key codes and the names other systems use:
// X11 keysyms (for XGrabKey) and GTK accelerator strings (for GNOME settings).
namespace keynames {

// Keysym for a Qt key without modifiers, or 0 if there is no mapping.
// Letters map to their lower-case keysym, which is what the keyboard produces.
uint32_t keysym(Qt::Key key);

// Keysym name as used in GTK accelerators and xkb, e.g. "Print", "a", "1".
QString keysymName(Qt::Key key);

// "<Primary><Shift>6", "<Super><Shift>s". Empty if the key has no mapping.
QString toGtkAccelerator(QKeyCombination combo);
QString toGtkAccelerator(const QKeySequence &sequence);  // uses the first chord only

// Inverse of toGtkAccelerator; Qt::Key_unknown on failure.
QKeyCombination fromGtkAccelerator(const QString &accelerator);

}  // namespace keynames
