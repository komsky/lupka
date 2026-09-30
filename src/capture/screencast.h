// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QObject>
#include <QRect>
#include <QTimer>
#include <QVariantMap>

// One PipeWire stream offered by the portal: a monitor or a window.
struct CastStream {
    quint32 node = 0;
    QRect geometry;  // compositor (logical) coordinates; empty if not reported
    quint32 sourceType = 0;
};

// A session of org.freedesktop.portal.ScreenCast: CreateSession, SelectSources,
// Start, then OpenPipeWireRemote for each consumer. The desktop asks the user
// which screens to share the first time; a restore token skips that later.
class ScreenCastSession : public QObject
{
    Q_OBJECT
public:
    enum SourceType : quint32 { Monitor = 1, Window = 2 };
    enum CursorMode : quint32 { Hidden = 1, Embedded = 2, Metadata = 4 };

    struct Options {
        quint32 types = Monitor;
        bool multiple = true;
        quint32 cursorMode = Hidden;
        bool persist = true;  // persist_mode 2: until revoked
        QString restoreToken;
        int timeoutMs = 120000;  // the first time a person has to answer a dialog
    };

    explicit ScreenCastSession(QObject *parent = nullptr);
    ~ScreenCastSession() override;

    void start(const Options &options);
    void close();

    const QList<CastStream> &streams() const { return m_streams; }
    QString restoreToken() const { return m_restoreToken; }
    // A new connection to the portal's PipeWire instance; the caller owns the fd.
    int openPipeWireRemote(QString *error);

    static bool isAvailable();
    // The closest cursor mode the portal offers to `wanted` (0: leave unset).
    static quint32 pickCursorMode(quint32 wanted);

Q_SIGNALS:
    void started();
    void failed(const QString &reason);

private Q_SLOTS:
    void onCreateSessionResponse(uint response, const QVariantMap &results);
    void onSelectSourcesResponse(uint response, const QVariantMap &results);
    void onStartResponse(uint response, const QVariantMap &results);

private:
    void request(const QString &method, const QList<QVariant> &args, QVariantMap options, const char *slot);
    void stopWatching();
    void fail(const QString &reason);

    Options m_options;
    QString m_session;
    QString m_watchedPath;
    const char *m_watchedSlot = nullptr;
    QList<CastStream> m_streams;
    QString m_restoreToken;
    QTimer m_timeout;
    bool m_active = false;
};
