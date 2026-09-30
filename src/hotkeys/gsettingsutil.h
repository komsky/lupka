// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Small helpers over GSettings. Include <gio/gio.h> (with Qt's "signals"
// macro pushed away) before this header.

#include <QString>
#include <QStringList>

namespace gsettingsutil {

// g_settings_new() aborts the process for unknown schemas; check first.
inline bool hasSchema(const char *id)
{
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    if (!source)
        return false;
    GSettingsSchema *schema = g_settings_schema_source_lookup(source, id, TRUE);
    if (!schema)
        return false;
    g_settings_schema_unref(schema);
    return true;
}

inline bool hasKey(const char *schemaId, const char *key)
{
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    if (!source)
        return false;
    GSettingsSchema *schema = g_settings_schema_source_lookup(source, schemaId, TRUE);
    if (!schema)
        return false;
    const bool found = g_settings_schema_has_key(schema, key);
    g_settings_schema_unref(schema);
    return found;
}

inline QStringList getStrv(GSettings *settings, const char *key)
{
    QStringList result;
    gchar **values = g_settings_get_strv(settings, key);
    for (gchar **v = values; v && *v; ++v)
        result << QString::fromUtf8(*v);
    g_strfreev(values);
    return result;
}

inline void setStrv(GSettings *settings, const char *key, const QStringList &values)
{
    QList<QByteArray> storage;
    QList<const gchar *> pointers;
    for (const QString &v : values)
        storage << v.toUtf8();
    for (const QByteArray &b : storage)
        pointers << b.constData();
    pointers << nullptr;
    g_settings_set_strv(settings, key, pointers.data());
}

inline QString getString(GSettings *settings, const char *key)
{
    gchar *value = g_settings_get_string(settings, key);
    const QString result = QString::fromUtf8(value);
    g_free(value);
    return result;
}

inline void setString(GSettings *settings, const char *key, const QString &value)
{
    if (getString(settings, key) != value)
        g_settings_set_string(settings, key, value.toUtf8().constData());
}

}  // namespace gsettingsutil
