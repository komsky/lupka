// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// GStreamer headers use "signals" as an identifier, which Qt defines as a macro.
#pragma push_macro("signals")
#undef signals
#include <gst/gst.h>
#pragma pop_macro("signals")

namespace gstutil {

// Initialise GStreamer once; false if it cannot be.
bool init();
bool hasElement(const char *factory);

}  // namespace gstutil
