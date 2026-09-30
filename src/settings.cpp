// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include "config.h"

#include <QDir>
#include <QStandardPaths>

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_store(QSettings::IniFormat, QSettings::UserScope, QStringLiteral(APP_BIN), QStringLiteral(APP_BIN))
{
}

Settings::Settings(const QString &file, QObject *parent)
    : QObject(parent)
    , m_store(file, QSettings::IniFormat)
{
}

QVariant Settings::get(const QString &key, const QVariant &fallback) const
{
    return m_store.value(key, fallback);
}

void Settings::put(const QString &key, const QVariant &value)
{
    if (m_store.value(key) == value && m_store.contains(key))
        return;
    m_store.setValue(key, value);
    Q_EMIT changed();
}

double Settings::initialZoom() const { return qBound(1.25, get(QStringLiteral("zoom/initial"), 2.0).toDouble(), 16.0); }
void Settings::setInitialZoom(double zoom) { put(QStringLiteral("zoom/initial"), zoom); }
bool Settings::animateZoom() const { return get(QStringLiteral("zoom/animate"), true).toBool(); }
void Settings::setAnimateZoom(bool on) { put(QStringLiteral("zoom/animate"), on); }
bool Settings::smoothZoom() const { return get(QStringLiteral("zoom/smooth"), true).toBool(); }
void Settings::setSmoothZoom(bool on) { put(QStringLiteral("zoom/smooth"), on); }

int Settings::penWidth() const { return qBound(2, get(QStringLiteral("draw/penWidth"), 5).toInt(), 40); }
void Settings::setPenWidth(int width) { put(QStringLiteral("draw/penWidth"), width); }
QColor Settings::penColor() const
{
    const QColor c(get(QStringLiteral("draw/penColor"), QStringLiteral("#ff0000")).toString());
    return c.isValid() ? c : QColor(Qt::red);
}
void Settings::setPenColor(const QColor &color) { put(QStringLiteral("draw/penColor"), color.name()); }
QString Settings::fontFamily() const { return get(QStringLiteral("draw/fontFamily"), QStringLiteral("Sans Serif")).toString(); }
void Settings::setFontFamily(const QString &family) { put(QStringLiteral("draw/fontFamily"), family); }
int Settings::fontScale() const { return qBound(1, get(QStringLiteral("draw/fontScale"), 10).toInt(), 50); }
void Settings::setFontScale(int scale) { put(QStringLiteral("draw/fontScale"), qBound(1, scale, 50)); }

QList<QKeySequence> Settings::shortcuts(Action action) const
{
    const ActionInfo &info = actionInfo(action);
    const QString key = QStringLiteral("shortcuts/") + info.id;
    if (!m_store.contains(key))
        return info.defaults;
    const QVariant value = m_store.value(key);
    if (value.toString() == QLatin1String("none"))
        return {};
    QList<QKeySequence> result;
    const QStringList stored = value.toStringList();
    for (const QString &text : stored) {
        const QKeySequence seq = QKeySequence::fromString(text, QKeySequence::PortableText);
        if (!seq.isEmpty())
            result << seq;
    }
    return result;
}

void Settings::setShortcuts(Action action, const QList<QKeySequence> &keys)
{
    QStringList stored;
    for (const QKeySequence &seq : keys) {
        if (!seq.isEmpty())
            stored << seq.toString(QKeySequence::PortableText);
    }
    const QString key = QStringLiteral("shortcuts/") + actionInfo(action).id;
    if (m_store.contains(key) && shortcuts(action) == keys)
        return;
    // QSettings turns an empty list into an invalid variant; keep "disabled" explicit.
    m_store.setValue(key, stored.isEmpty() ? QVariant(QStringLiteral("none")) : QVariant(stored));
    Q_EMIT shortcutsChanged();
    Q_EMIT changed();
}

void Settings::resetShortcuts()
{
    m_store.remove(QStringLiteral("shortcuts"));
    Q_EMIT shortcutsChanged();
    Q_EMIT changed();
}

QString Settings::saveDirectory() const
{
    QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (pictures.isEmpty())
        pictures = QDir::homePath();
    return get(QStringLiteral("snip/saveDirectory"), pictures + QStringLiteral("/Screenshots")).toString();
}
void Settings::setSaveDirectory(const QString &dir) { put(QStringLiteral("snip/saveDirectory"), dir); }
QString Settings::lastSaveDirectory() const { return get(QStringLiteral("snip/lastSaveDirectory"), saveDirectory()).toString(); }
void Settings::setLastSaveDirectory(const QString &dir) { put(QStringLiteral("snip/lastSaveDirectory"), dir); }
bool Settings::snipAlsoSaves() const { return get(QStringLiteral("snip/alsoSave"), false).toBool(); }
void Settings::setSnipAlsoSaves(bool on) { put(QStringLiteral("snip/alsoSave"), on); }

int Settings::breakMinutes() const { return qBound(1, get(QStringLiteral("break/minutes"), 10).toInt(), 99); }
void Settings::setBreakMinutes(int minutes) { put(QStringLiteral("break/minutes"), minutes); }
bool Settings::breakPlaySound() const { return get(QStringLiteral("break/sound"), false).toBool(); }
void Settings::setBreakPlaySound(bool on) { put(QStringLiteral("break/sound"), on); }
QString Settings::breakSoundFile() const { return get(QStringLiteral("break/soundFile"), QString()).toString(); }
void Settings::setBreakSoundFile(const QString &file) { put(QStringLiteral("break/soundFile"), file); }
bool Settings::breakShowElapsed() const { return get(QStringLiteral("break/showElapsed"), true).toBool(); }
void Settings::setBreakShowElapsed(bool on) { put(QStringLiteral("break/showElapsed"), on); }
int Settings::breakOpacity() const { return qBound(10, get(QStringLiteral("break/opacity"), 100).toInt(), 100); }
void Settings::setBreakOpacity(int percent) { put(QStringLiteral("break/opacity"), percent); }
int Settings::breakPosition() const { return qBound(0, get(QStringLiteral("break/position"), 4).toInt(), 8); }
void Settings::setBreakPosition(int position) { put(QStringLiteral("break/position"), position); }
QColor Settings::breakTextColor() const
{
    const QColor c(get(QStringLiteral("break/textColor"), QStringLiteral("#ff0000")).toString());
    return c.isValid() ? c : QColor(Qt::red);
}
void Settings::setBreakTextColor(const QColor &color) { put(QStringLiteral("break/textColor"), color.name()); }
QColor Settings::breakBackgroundColor() const
{
    const QColor c(get(QStringLiteral("break/backgroundColor"), QStringLiteral("#ffffff")).toString());
    return c.isValid() ? c : QColor(Qt::white);
}
void Settings::setBreakBackgroundColor(const QColor &color) { put(QStringLiteral("break/backgroundColor"), color.name()); }
Settings::BreakBackground Settings::breakBackground() const
{
    return BreakBackground(qBound(0, get(QStringLiteral("break/background"), 0).toInt(), 2));
}
void Settings::setBreakBackground(BreakBackground mode) { put(QStringLiteral("break/background"), int(mode)); }
QString Settings::breakImageFile() const { return get(QStringLiteral("break/imageFile"), QString()).toString(); }
void Settings::setBreakImageFile(const QString &file) { put(QStringLiteral("break/imageFile"), file); }
bool Settings::breakScaleImage() const { return get(QStringLiteral("break/scaleImage"), true).toBool(); }
void Settings::setBreakScaleImage(bool on) { put(QStringLiteral("break/scaleImage"), on); }

QString Settings::recordDirectory() const
{
    QString videos = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (videos.isEmpty())
        videos = QDir::homePath();
    return get(QStringLiteral("record/directory"), videos).toString();
}
void Settings::setRecordDirectory(const QString &dir) { put(QStringLiteral("record/directory"), dir); }
bool Settings::recordAudio() const { return get(QStringLiteral("record/audio"), true).toBool(); }
void Settings::setRecordAudio(bool on) { put(QStringLiteral("record/audio"), on); }
int Settings::recordFrameRate() const { return qBound(5, get(QStringLiteral("record/frameRate"), 30).toInt(), 60); }
void Settings::setRecordFrameRate(int fps) { put(QStringLiteral("record/frameRate"), fps); }
QString Settings::recordToken() const { return get(QStringLiteral("record/screenCastToken"), QString()).toString(); }
void Settings::setRecordToken(const QString &token) { put(QStringLiteral("record/screenCastToken"), token); }

QString Settings::demoTypeFile() const { return get(QStringLiteral("demoType/file"), QString()).toString(); }
void Settings::setDemoTypeFile(const QString &file) { put(QStringLiteral("demoType/file"), file); }
int Settings::demoTypeSpeed() const { return qBound(10, get(QStringLiteral("demoType/speed"), 55).toInt(), 100); }
void Settings::setDemoTypeSpeed(int speed) { put(QStringLiteral("demoType/speed"), speed); }
QString Settings::demoTypeToken() const { return get(QStringLiteral("demoType/token"), QString()).toString(); }
void Settings::setDemoTypeToken(const QString &token) { put(QStringLiteral("demoType/token"), token); }

QString Settings::screenCastToken() const { return get(QStringLiteral("capture/screenCastToken"), QString()).toString(); }
void Settings::setScreenCastToken(const QString &token) { put(QStringLiteral("capture/screenCastToken"), token); }

double Settings::liveZoomFactor() const { return qBound(1.25, get(QStringLiteral("liveZoom/factor"), 2.0).toDouble(), 16.0); }
void Settings::setLiveZoomFactor(double factor) { put(QStringLiteral("liveZoom/factor"), factor); }

bool Settings::startAtLogin() const { return get(QStringLiteral("app/startAtLogin"), true).toBool(); }
void Settings::setStartAtLogin(bool on) { put(QStringLiteral("app/startAtLogin"), on); }
bool Settings::showTrayIcon() const { return get(QStringLiteral("app/trayIcon"), true).toBool(); }
void Settings::setShowTrayIcon(bool on) { put(QStringLiteral("app/trayIcon"), on); }
bool Settings::firstRun() const { return !get(QStringLiteral("app/firstRunDone"), false).toBool(); }
void Settings::setFirstRunDone() { put(QStringLiteral("app/firstRunDone"), true); }
