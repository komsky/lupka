// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gstutil.h"

#include <mutex>

namespace gstutil {

bool init()
{
    static std::once_flag once;
    static bool ok = false;
    std::call_once(once, [] {
        GError *error = nullptr;
        ok = gst_init_check(nullptr, nullptr, &error);
        g_clear_error(&error);
    });
    return ok;
}

bool hasElement(const char *factory)
{
    if (!init())
        return false;
    GstElementFactory *f = gst_element_factory_find(factory);
    if (!f)
        return false;
    gst_object_unref(f);
    return true;
}

}  // namespace gstutil
