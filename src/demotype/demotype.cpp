// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "demotype.h"

#include "platform.h"
#include "settings.h"
#include "typist.h"

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QMimeData>
#include <QProcess>
#include <QRandomGenerator>

namespace {

constexpr quint32 kKeyV = 'v';

}  // namespace

DemoType::DemoType(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &DemoType::runStep);
}

bool DemoType::reload()
{
    QString fileText;
    const QString path = m_settings->demoTypeFile();
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.size() > 1024 * 1024) {
            Q_EMIT failed(tr("The DemoType file is larger than 1 MB."));
            return false;
        }
        if (file.open(QIODevice::ReadOnly))
            fileText = demoscript::decode(file.readAll());
    }
    const QString source = demoscript::chooseSource(QGuiApplication::clipboard()->text(), fileText);
    if (source.trimmed().isEmpty()) {
        Q_EMIT failed(tr("No DemoType script: choose a file in Settings, or copy text that starts with [start]."));
        return false;
    }
    // Keep our place unless the script changed.
    if (source != m_loadedSource) {
        m_loadedSource = source;
        m_snippets = demoscript::parse(source);
        m_next = 0;
    }
    return !m_snippets.isEmpty();
}

void DemoType::typeNext()
{
    if (m_typing)
        return;
    if (!reload())
        return;
    if (m_next >= m_snippets.size())
        m_next = 0;  // wrap around, as ZoomIt does
    m_current = m_snippets.at(m_next++);
    m_step = 0;
    m_typing = true;
    Q_EMIT typingChanged(true);

    if (!m_typist) {
        m_typist = Typist::create(m_settings, this);
        connect(m_typist, &Typist::ready, this, &DemoType::begin);
        connect(m_typist, &Typist::failed, this, [this](const QString &reason) {
            done();
            Q_EMIT failed(reason);
        });
    }
    m_typist->prepare();
}

void DemoType::stepBack()
{
    if (m_typing || !reload())
        return;
    // m_next points past the last typed snippet; go back one more.
    m_next = qMax(0, m_next - 1);
}

void DemoType::stop()
{
    if (!m_typing)
        return;
    m_timer.stop();
    done();
}

void DemoType::begin()
{
    m_modifierWaits = 0;
    // The hotkey's Ctrl is probably still down; typing now would send Ctrl+letter.
    m_timer.start(platform::isX11() ? 20 : 400);
}

int DemoType::delayMs() const
{
    // ZoomIt: 110 - speed ms per character, randomly between 1x and 2x of that.
    const int base = qMax(1, 110 - m_settings->demoTypeSpeed());
    return base + QRandomGenerator::global()->bounded(base + 1);
}

void DemoType::runStep()
{
    if (!m_typing)
        return;
    if (m_step == 0 && m_typist->modifiersHeld() && ++m_modifierWaits < 150) {
        m_timer.start(20);
        return;
    }
    if (m_step >= m_current.size()) {
        done();
        return;
    }
    const TypeStep step = m_current.at(m_step++);
    int wait = delayMs();
    switch (step.kind) {
    case TypeStep::Char:
        if (!m_typist->typeText(step.text))
            paste(step.text);
        break;
    case TypeStep::Key:
        m_typist->pressKey(step.keysym);
        break;
    case TypeStep::Pause:
        wait = step.ms;
        break;
    case TypeStep::Paste:
        paste(step.text);
        wait = 150;
        break;
    }
    m_timer.start(wait);
}

void DemoType::paste(const QString &text)
{
    // Under Wayland only the focused client may set the clipboard; wl-copy
    // knows how to get around that.
    if (platform::isWayland() && platform::hasProgram(QStringLiteral("wl-copy"))) {
        QProcess copy;
        copy.start(QStringLiteral("wl-copy"), {QStringLiteral("--"), text});
        copy.waitForFinished(2000);
    } else {
        auto *mime = new QMimeData;
        mime->setText(text);
        QGuiApplication::clipboard()->setMimeData(mime);
    }
    m_typist->pressKey(kKeyV, true);
}

void DemoType::done()
{
    m_typing = false;
    if (m_typist)
        m_typist->finish();
    Q_EMIT typingChanged(false);
}
