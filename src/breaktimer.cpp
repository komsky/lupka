#include "breaktimer.h"

#include "config.h"
#include "platform.h"
#include "settings.h"
#include "x11util.h"

#include <QApplication>
#include <QFile>
#include <QKeyEvent>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <QWindow>

namespace {

QString formatTime(qint64 ms)
{
    const qint64 total = qAbs(ms) / 1000;
    return QStringLiteral("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QLatin1Char('0'));
}

void playAlarm(const QString &file)
{
    if (!file.isEmpty() && QFile::exists(file)) {
        for (const char *player : {"pw-play", "paplay", "aplay"}) {
            if (platform::hasProgram(QLatin1String(player))) {
                QProcess::startDetached(QLatin1String(player), {file});
                return;
            }
        }
    }
    // canberra follows the desktop's sound theme.
    if (platform::hasProgram(QStringLiteral("canberra-gtk-play"))) {
        QProcess::startDetached(QStringLiteral("canberra-gtk-play"),
                                {QStringLiteral("-i"), QStringLiteral("alarm-clock-elapsed")});
        return;
    }
    const QString fallback = QStringLiteral("/usr/share/sounds/freedesktop/stereo/complete.oga");
    if (platform::hasProgram(QStringLiteral("paplay")) && QFile::exists(fallback)) {
        QProcess::startDetached(QStringLiteral("paplay"), {fallback});
        return;
    }
    QApplication::beep();
}

QColor colorForKey(int key)
{
    switch (key) {
    case Qt::Key_R:
        return QColor(255, 0, 0);
    case Qt::Key_G:
        return QColor(0, 255, 0);
    case Qt::Key_B:
        return QColor(0, 0, 255);
    case Qt::Key_O:
        return QColor(255, 128, 0);
    case Qt::Key_Y:
        return QColor(255, 255, 0);
    case Qt::Key_P:
        return QColor(255, 128, 255);
    case Qt::Key_W:
        return QColor(255, 255, 255);
    case Qt::Key_K:
        return QColor(0, 0, 0);
    default:
        return {};
    }
}

}  // namespace

BreakTimer::BreakTimer(Settings *settings)
    : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint)
    , m_settings(settings)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    setWindowTitle(tr("%1 Break Timer").arg(QStringLiteral(APP_NAME)));
    m_tick.setInterval(250);
    connect(&m_tick, &QTimer::timeout, this, &BreakTimer::tick);
}

void BreakTimer::start(QScreen *screen, const QImage &desktop)
{
    m_background = desktop;
    setRemaining(qint64(m_settings->breakMinutes()) * 60000);
    m_alarmPlayed = false;
    m_running = true;
    if (screen) {
        setScreen(screen);
        setGeometry(screen->geometry());
    }
    showFullScreen();
    bringToFront();
    m_tick.start();
}

void BreakTimer::stop()
{
    m_running = false;
    m_tick.stop();
    m_background = QImage();
    hide();
}

void BreakTimer::bringToFront()
{
    show();
    raise();
    activateWindow();
    if (platform::isX11())
        x11util::activate(windowHandle());
}

void BreakTimer::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (platform::isX11())
        x11util::activate(windowHandle());
}

void BreakTimer::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
}

qint64 BreakTimer::remainingMs() const
{
    return QDateTime::currentDateTimeUtc().msecsTo(m_end);
}

void BreakTimer::setRemaining(qint64 ms)
{
    // Round to whole seconds so the display does not skip a second at the start.
    m_end = QDateTime::currentDateTimeUtc().addMSecs(ms + 999);
    if (ms > 0)
        m_alarmPlayed = false;
    update();
}

void BreakTimer::tick()
{
    if (remainingMs() <= 0 && !m_alarmPlayed) {
        m_alarmPlayed = true;
        if (m_settings->breakPlaySound())
            playAlarm(m_settings->breakSoundFile());
    }
    update();
}

qint64 BreakTimer::adjustedMinutes(qint64 remainingMs, int minutes)
{
    const qint64 minute = 60000;
    qint64 base = qMax<qint64>(0, remainingMs);
    const qint64 whole = (base / minute) * minute;
    if (base != whole) {
        // A part-minute first snaps to the minute boundary in the direction of travel.
        base = minutes > 0 ? whole + minute : whole;
        minutes += minutes > 0 ? -1 : 1;
    }
    return qMax<qint64>(0, base + qint64(minutes) * minute);
}

qint64 BreakTimer::adjustedSeconds(qint64 remainingMs, int tens)
{
    const qint64 step = 10000;
    qint64 base = qMax<qint64>(0, remainingMs);
    const qint64 snapped = (base / step) * step;
    if (base != snapped) {
        base = tens > 0 ? snapped + step : snapped;
        tens += tens > 0 ? -1 : 1;
    }
    return qMax<qint64>(0, base + qint64(tens) * step);
}

void BreakTimer::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(m_settings->breakOpacity() / 100.0);

    const Settings::BreakBackground mode = m_settings->breakBackground();
    if (mode == Settings::BreakBackground::FadedDesktop && !m_background.isNull()) {
        painter.fillRect(rect(), Qt::black);
        const double opacity = painter.opacity();
        painter.setOpacity(opacity * 0x4F / 255.0);
        painter.drawImage(QRectF(rect()), m_background);
        painter.setOpacity(opacity);
    } else if (mode == Settings::BreakBackground::Image) {
        painter.fillRect(rect(), m_settings->breakBackgroundColor());
        const QImage image(m_settings->breakImageFile());
        if (!image.isNull()) {
            if (m_settings->breakScaleImage()) {
                painter.drawImage(QRectF(rect()), image);
            } else {
                QRectF target(QPointF(0, 0), QSizeF(image.size()) / image.devicePixelRatio());
                target.moveCenter(QRectF(rect()).center());
                painter.drawImage(target, image);
            }
        }
    } else {
        painter.fillRect(rect(), m_settings->breakBackgroundColor());
    }

    const qint64 remaining = remainingMs();
    const bool expired = remaining <= 0;
    const QString main = expired ? formatTime(0) : formatTime(remaining);
    const QString elapsed = expired && m_settings->breakShowElapsed()
                                ? QStringLiteral("(-%1)").arg(formatTime(remaining))
                                : QString();

    QFont big(m_settings->fontFamily());
    big.setPixelSize(qMax(24, height() / 5));
    QFont small(m_settings->fontFamily());
    small.setPixelSize(qMax(16, height() / 8));
    const QFontMetricsF bigMetrics(big);
    const QFontMetricsF smallMetrics(small);
    const double blockWidth = qMax(bigMetrics.horizontalAdvance(main),
                                   elapsed.isEmpty() ? 0.0 : smallMetrics.horizontalAdvance(elapsed));
    const double blockHeight = bigMetrics.height() + (elapsed.isEmpty() ? 0.0 : smallMetrics.height());

    // 3x3 grid position with a 50 px margin, as in ZoomIt.
    const int position = m_settings->breakPosition();
    const QRectF area = QRectF(rect()).adjusted(50, 50, -50, -50);
    const double x = area.left() + (area.width() - blockWidth) * (position % 3) / 2.0;
    const double y = area.top() + (area.height() - blockHeight) * (position / 3) / 2.0;

    painter.setPen(m_settings->breakTextColor());
    painter.setFont(big);
    painter.drawText(QRectF(x, y, blockWidth, bigMetrics.height()), Qt::AlignCenter, main);
    if (!elapsed.isEmpty()) {
        painter.setFont(small);
        painter.drawText(QRectF(x, y + bigMetrics.height(), blockWidth, smallMetrics.height()), Qt::AlignCenter,
                         elapsed);
    }
}

void BreakTimer::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    if (ctrl && (key == Qt::Key_W || key == Qt::Key_K)) {
        m_settings->setBreakBackgroundColor(key == Qt::Key_W ? Qt::white : Qt::black);
        update();
        return;
    }
    const QColor color = colorForKey(key);
    if (color.isValid() && !ctrl) {
        m_settings->setBreakTextColor(color);
        update();
        return;
    }
    switch (key) {
    case Qt::Key_Escape:
        stop();
        break;
    case Qt::Key_Up:
    case Qt::Key_Down:
        setRemaining(adjustedMinutes(remainingMs(), key == Qt::Key_Up ? 1 : -1));
        break;
    case Qt::Key_Right:
    case Qt::Key_Left:
        setRemaining(adjustedSeconds(remainingMs(), key == Qt::Key_Right ? 1 : -1));
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

void BreakTimer::wheelEvent(QWheelEvent *event)
{
    m_wheelRemainder += event->angleDelta().y();
    const int notches = m_wheelRemainder / 120;
    m_wheelRemainder %= 120;
    if (notches)
        setRemaining(adjustedMinutes(remainingMs(), notches));
}

void BreakTimer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
        stop();
}
