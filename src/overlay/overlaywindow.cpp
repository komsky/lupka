#include "overlaywindow.h"

#include "config.h"
#include "platform.h"
#include "settings.h"
#include "x11util.h"
#include "zoommath.h"

#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QWindow>

namespace {

constexpr int kNormalBlur = 20;
constexpr int kStrongBlur = 40;
constexpr int kMinPenWidth = 2;
constexpr int kMaxPenWidth = 40;

// ZoomIt's colour keys.
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

const QString kZoomedPng = QStringLiteral("Zoomed PNG (*.png)");
const QString kActualPng = QStringLiteral("Actual size PNG (*.png)");
const QString kZoomedJpg = QStringLiteral("Zoomed JPEG (*.jpg)");
const QString kActualJpg = QStringLiteral("Actual size JPEG (*.jpg)");

}  // namespace

OverlayWindow::OverlayWindow(QScreen *screen, const QImage &shot, Settings *settings, Kind kind)
    : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_kind(kind)
    , m_screen(screen)
    , m_settings(settings)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setWindowTitle(QStringLiteral(APP_NAME));

    if (kind == Kind::LiveDraw) {
        // Draw over the live desktop: a transparent canvas in a translucent window.
        setAttribute(Qt::WA_TranslucentBackground);
        QImage clear(shot.size(), QImage::Format_ARGB32_Premultiplied);
        clear.fill(Qt::transparent);
        clear.setDevicePixelRatio(shot.devicePixelRatio());
        m_canvas.setBackground(clear);
    } else {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_NoSystemBackground);
        m_canvas.setBackground(shot);
    }

    m_penColor = settings->penColor();
    m_penWidth = settings->penWidth();
    m_fontScale = settings->fontScale();
    m_view = QRectF(QPointF(0, 0), m_canvas.logicalSize());

    m_zoomAnimation.setStartValue(0.0);
    m_zoomAnimation.setEndValue(1.0);
    connect(&m_zoomAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_zoom = zoommath::interpolate(m_zoomFrom, m_zoomTo, value.toDouble());
        updateView();
        update();
    });
    connect(&m_zoomAnimation, &QVariantAnimation::finished, this, [this] {
        m_zoom = m_zoomTo;
        updateView();
        updateCursor();
        update();
        if (m_leaving)
            Q_EMIT finished();
    });

    m_caretTimer.setInterval(530);
    connect(&m_caretTimer, &QTimer::timeout, this, [this] {
        m_caretVisible = !m_caretVisible;
        update();
    });
    m_hintTimer.setSingleShot(true);
    m_hintTimer.setInterval(1800);
    connect(&m_hintTimer, &QTimer::timeout, this, [this] {
        m_hint.clear();
        update();
    });
    updateCursor();
}

OverlayWindow::~OverlayWindow()
{
    if (platform::isX11() && isVisible())
        x11util::ungrabKeyboard();
}

void OverlayWindow::showOnScreen()
{
    if (m_screen) {
        setScreen(m_screen);
        setGeometry(m_screen->geometry());
    }
    showFullScreen();
    raise();
    activateWindow();
}

void OverlayWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_view = zoommath::viewport(QSizeF(size()), m_cursor, m_zoom);
    if (platform::isX11()) {
        x11util::activate(windowHandle());
        x11util::grabKeyboard(windowHandle());
    }
    setFocus(Qt::ActiveWindowFocusReason);
}

void OverlayWindow::hideEvent(QHideEvent *event)
{
    if (platform::isX11())
        x11util::ungrabKeyboard();
    QWidget::hideEvent(event);
}

void OverlayWindow::notePointer(const QPointF &pos)
{
    m_cursor = pos;
    if (!m_pointerKnown) {
        m_pointerKnown = true;
        Q_EMIT pointerSeen(this, pos);
    }
}

void OverlayWindow::begin(const QPointF &cursor)
{
    if (m_started)
        return;
    m_started = true;
    m_cursor = cursor;
    m_zoom = m_zoomFrom = m_zoomTo = 1.0;
    updateView();
    switch (m_kind) {
    case Kind::Zoom:
        setZoomTarget(m_settings->initialZoom(), true);
        break;
    case Kind::Draw:
    case Kind::LiveDraw:
        enterPenMode();
        break;
    case Kind::Snip:
        startCrop(CropPurpose::Copy, true);
        break;
    case Kind::SnipSave:
        startCrop(CropPurpose::Save, true);
        break;
    }
    update();
}

void OverlayWindow::leave()
{
    if (m_leaving)
        return;
    m_leaving = true;
    m_drawing = false;
    m_typing = false;
    m_cropping = false;
    m_caretTimer.stop();
    // "Telescope" back out so the real desktop appears without a jump.
    if (m_zoom > 1.001 && m_settings->animateZoom()) {
        m_viewFrozen = false;
        setZoomTarget(1.0, false);
        return;
    }
    QMetaObject::invokeMethod(this, &OverlayWindow::finished, Qt::QueuedConnection);
}

// ---------------------------------------------------------------- view

QPointF OverlayWindow::toSource(const QPointF &widget) const
{
    return zoommath::toSource(m_view, QSizeF(size()), widget);
}

void OverlayWindow::updateView()
{
    const QSizeF screen(size());
    if (m_viewFrozen)
        m_view = zoommath::anchoredViewport(screen, m_anchorSource, m_anchorWidget, m_zoom);
    else
        m_view = zoommath::viewport(screen, m_cursor, m_zoom);
}

void OverlayWindow::freezeView()
{
    // Keep what is under the pointer in place from now on.
    m_anchorWidget = m_cursor;
    m_anchorSource = toSource(m_cursor);
    m_viewFrozen = true;
}

void OverlayWindow::setZoomTarget(double target, bool firstStepImmediate)
{
    target = qBound(zoommath::kMinZoom, target, zoommath::kMaxZoom);
    if (m_viewFrozen)
        freezeView();  // re-anchor at the pointer, like zooming a map
    m_zoomAnimation.stop();
    m_zoomFrom = m_zoom;
    m_zoomTo = target;
    const int ms = m_settings->animateZoom() ? zoommath::animationMs(m_zoom, target, firstStepImmediate) : 0;
    if (ms <= 0) {
        m_zoom = target;
        updateView();
        update();
        if (m_leaving)
            QMetaObject::invokeMethod(this, &OverlayWindow::finished, Qt::QueuedConnection);
        return;
    }
    m_zoomAnimation.setDuration(ms);
    m_zoomAnimation.start();
}

void OverlayWindow::stepZoom(int notches)
{
    if (m_kind == Kind::LiveDraw || notches == 0)
        return;
    double target = m_zoomTo;
    for (int i = 0; i < qAbs(notches); ++i)
        target = notches > 0 ? zoommath::zoomIn(target) : zoommath::zoomOut(target);
    setZoomTarget(target, true);
    updateCursor();
}

// ---------------------------------------------------------------- pen mode

void OverlayWindow::enterPenMode()
{
    if (m_penMode)
        return;
    m_penMode = true;
    freezeView();
    updateCursor();
}

void OverlayWindow::leavePenMode()
{
    if (m_drawing)
        commitShape();
    m_penMode = false;
    if (m_kind == Kind::LiveDraw)
        return;
    // Back to panning. X11 lets us move the pointer to where the current view
    // would put it, so nothing jumps; elsewhere the view follows the pointer.
    if (platform::isX11() && m_zoom > 1.001) {
        const QSizeF screen(size());
        const double k = 1.0 - 1.0 / m_zoom;
        const QPointF target(qBound(0.0, m_view.x() / k, screen.width() - 1),
                             qBound(0.0, m_view.y() / k, screen.height() - 1));
        m_cursor = target;
        QCursor::setPos(mapToGlobal(target.toPoint()));
    }
    m_viewFrozen = false;
    updateView();
    updateCursor();
    update();
}

Shape::Kind OverlayWindow::kindForModifiers(Qt::KeyboardModifiers mods) const
{
    const bool ctrl = mods & Qt::ControlModifier;
    const bool shift = mods & Qt::ShiftModifier;
    if (ctrl && shift)
        return Shape::Arrow;
    if (ctrl)
        return Shape::Rectangle;
    if (shift)
        return Shape::Line;
    if (m_tabDown)
        return Shape::Ellipse;
    return Shape::Freehand;
}

void OverlayWindow::beginShape(const QPointF &source, Qt::KeyboardModifiers mods)
{
    m_current = Shape();
    m_current.kind = kindForModifiers(mods);
    m_current.color = m_penColor;
    m_current.width = m_penWidth;
    m_current.highlighter = m_highlighter;
    m_current.blurRadius = m_blurRadius;
    m_current.points = {source};
    if (m_current.kind != Shape::Freehand)
        m_current.points << source;
    m_drawing = true;
    update();
}

void OverlayWindow::commitShape()
{
    m_drawing = false;
    snapLine(m_current);
    m_canvas.add(m_current);
    m_current = Shape();
    update();
}

void OverlayWindow::setPenColor(const QColor &color, bool highlighter)
{
    m_penColor = color;
    m_highlighter = highlighter && m_kind != Kind::LiveDraw;
    m_blurRadius = 0;
    m_settings->setPenColor(color);
    if (m_typing) {
        m_text.color = color;
        update();
    }
    updateCursor();
}

void OverlayWindow::setBlur(int radius)
{
    if (m_kind == Kind::LiveDraw)
        return;
    m_blurRadius = radius;
    m_highlighter = false;
    updateCursor();
    showHint(radius > kNormalBlur ? tr("Strong blur") : tr("Blur"));
}

void OverlayWindow::changePenWidth(int delta)
{
    m_penWidth = qBound(kMinPenWidth, m_penWidth + delta, kMaxPenWidth);
    m_settings->setPenWidth(m_penWidth);
    updateCursor();
}

void OverlayWindow::startTyping(bool alignRight)
{
    enterPenMode();
    m_typing = true;
    m_textPlaced = false;
    m_text = Shape();
    m_text.kind = Shape::Text;
    m_text.color = m_blurRadius > 0 ? QColor(112, 112, 112) : m_penColor;
    m_text.alignRight = alignRight;
    m_text.points = {toSource(m_cursor)};
    updateTextFont();
    m_caretVisible = true;
    m_caretTimer.start();
    updateCursor();
    update();
}

void OverlayWindow::updateTextFont()
{
    // ZoomIt sizes text to a fraction of the visible height, so it reads the
    // same at any magnification.
    const double visibleHeight = height() / qMax(1.0, m_zoom);
    const int pixels = qMax(12, int(visibleHeight / qMax(1, m_fontScale)));
    m_text.font = QFont(m_settings->fontFamily());
    m_text.font.setPixelSize(pixels);
}

void OverlayWindow::endTyping()
{
    if (!m_typing)
        return;
    m_typing = false;
    m_caretTimer.stop();
    m_canvas.add(m_text);
    m_text = Shape();
    updateCursor();
    update();
}

void OverlayWindow::updateCursor()
{
    if (m_cropping) {
        setCursor(Qt::CrossCursor);
        return;
    }
    if (m_typing) {
        setCursor(Qt::IBeamCursor);
        return;
    }
    if (!m_penMode) {
        setCursor(Qt::ArrowCursor);
        return;
    }
    const double dpr = devicePixelRatioF();
    if (m_highlighter || m_blurRadius > 0) {
        // Highlighter and blur get a small cross, as in ZoomIt.
        const int side = 13;
        QPixmap pixmap(QSize(side, side) * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        const QColor ink = m_blurRadius > 0 ? QColor(112, 112, 112) : m_penColor;
        p.setPen(QPen(QColor(0, 0, 0, 180), 3));
        p.drawLine(QPointF(2, side / 2.0), QPointF(side - 2, side / 2.0));
        p.drawLine(QPointF(side / 2.0, 2), QPointF(side / 2.0, side - 2));
        p.setPen(QPen(ink, 1));
        p.drawLine(QPointF(2, side / 2.0), QPointF(side - 2, side / 2.0));
        p.drawLine(QPointF(side / 2.0, 2), QPointF(side / 2.0, side - 2));
        p.end();
        setCursor(QCursor(pixmap, side / 2, side / 2));
        return;
    }
    // A dot the size and colour of the pen as it will appear on screen.
    const double width = m_penWidth * m_zoom;
    const int side = qMax(9, int(std::ceil(width)) + 4);
    QPixmap pixmap(QSize(side, side) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(side / 2.0, side / 2.0);
    const double r = qMax(width / 2.0, 1.5);
    p.setPen(QPen(m_penColor.lightness() > 200 ? QColor(0, 0, 0, 170) : QColor(255, 255, 255, 170), 1.0));
    p.setBrush(m_penColor);
    p.drawEllipse(c, r, r);
    p.end();
    setCursor(QCursor(pixmap, side / 2, side / 2));
}

// ---------------------------------------------------------------- selection

void OverlayWindow::startCrop(CropPurpose purpose)
{
    startCrop(purpose, false);
}

void OverlayWindow::startCrop(CropPurpose purpose, bool exitAfter)
{
    if (m_drawing)
        commitShape();
    endTyping();
    if (m_penMode)
        leavePenMode();
    freezeView();
    m_cropping = true;
    m_selecting = false;
    m_cropPurpose = purpose;
    m_exitAfterCrop = exitAfter;
    updateCursor();
    showHint(tr("Drag to select  ·  Enter: everything  ·  Esc: cancel"));
}

void OverlayWindow::cancelCrop()
{
    m_cropping = false;
    m_selecting = false;
    m_viewFrozen = false;
    updateView();
    updateCursor();
    update();
    if (m_exitAfterCrop)
        leave();
}

void OverlayWindow::finishCrop(const QRectF &area)
{
    m_cropping = false;
    m_selecting = false;
    update();
    if (m_cropPurpose == CropPurpose::Copy)
        copyArea(area, m_exitAfterCrop);
    else
        saveArea(area);
    if (m_exitAfterCrop) {
        leave();
        return;
    }
    m_viewFrozen = false;
    updateView();
    updateCursor();
}

void OverlayWindow::paintCrop(QPainter &painter) const
{
    const QRectF area = QRectF(m_selectStart, m_selectEnd).normalized().intersected(QRectF(rect()));
    QRegion dim(rect());
    if (m_selecting && area.width() >= 1 && area.height() >= 1)
        dim -= area.toAlignedRect();
    painter.save();
    painter.setClipRegion(dim);
    painter.fillRect(rect(), QColor(0, 0, 0, 176));
    painter.restore();
    if (!m_selecting || area.width() < 1 || area.height() < 1)
        return;

    painter.setPen(QPen(QColor(255, 222, 0), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(area.adjusted(-1, -1, 0, 0));
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(area);

    // Size of the image the selection will produce (see copyArea).
    const double scale = m_zoom > 1.001 ? devicePixelRatioF()
                                         : const_cast<Canvas &>(m_canvas).composed().devicePixelRatio() / m_zoom;
    const QString label =
        QStringLiteral("%1 × %2").arg(qRound(area.width() * scale)).arg(qRound(area.height() * scale));
    QFont font = painter.font();
    font.setPixelSize(13);
    painter.setFont(font);
    const QFontMetrics fm(font);
    QRectF box(0, 0, fm.horizontalAdvance(label) + 12, fm.height() + 6);
    box.moveTopLeft(area.bottomRight() + QPointF(-box.width(), 6));
    if (box.bottom() > height())
        box.moveBottom(area.top() - 6);
    if (box.left() < 0)
        box.moveLeft(area.left());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 190));
    painter.drawRoundedRect(box, 4, 4);
    painter.setPen(Qt::white);
    painter.drawText(box, Qt::AlignCenter, label);
}

// ---------------------------------------------------------------- output

QImage OverlayWindow::renderView(const QRectF &area) const
{
    // What is on screen (zoomed, with drawings), at the screen's resolution.
    const double dpr = devicePixelRatioF();
    QImage full(QSize(qRound(width() * dpr), qRound(height() * dpr)), QImage::Format_ARGB32_Premultiplied);
    full.setDevicePixelRatio(dpr);
    full.fill(Qt::transparent);
    {
        QPainter p(&full);
        const QImage &img = const_cast<Canvas &>(m_canvas).composed();
        const double sdpr = img.devicePixelRatio();
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(QRectF(rect()), img,
                    QRectF(m_view.x() * sdpr, m_view.y() * sdpr, m_view.width() * sdpr, m_view.height() * sdpr));
    }
    const QRect px = Canvas::toPixels(full, area);
    QImage out = full.copy(px);
    out.setDevicePixelRatio(1.0);
    return out;
}

QImage OverlayWindow::renderActual(const QRectF &area)
{
    // The screenshot's own pixels (plus drawings) for the part of the view in `area`.
    const QPointF a = toSource(area.topLeft());
    const QPointF b = toSource(area.bottomRight());
    const QImage &img = m_canvas.composed();
    QImage out = img.copy(Canvas::toPixels(img, QRectF(a, b)));
    out.setDevicePixelRatio(1.0);
    return out;
}

void OverlayWindow::copyArea(const QRectF &area, bool snip)
{
    // At 1x the screenshot's pixels are exact; zoomed, copy what is shown.
    const QImage image = m_zoom > 1.001 ? renderView(area) : renderActual(area);
    Q_EMIT copyRequested(image, snip);
    if (!snip)
        showHint(tr("Copied to the clipboard"));
}

void OverlayWindow::saveArea(const QRectF &area)
{
    QDir().mkpath(m_settings->lastSaveDirectory());
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    const QString suggested = QDir(m_settings->lastSaveDirectory()).filePath(QStringLiteral(APP_NAME " %1.png").arg(stamp));

    // Our keyboard grab would starve the dialog; give the keyboard back meanwhile.
    if (platform::isX11())
        x11util::ungrabKeyboard();
    QFileDialog dialog(this, tr("%1: Save Zoomed Screen").arg(QStringLiteral(APP_NAME)), suggested);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setOption(QFileDialog::DontUseNativeDialog);
    dialog.setNameFilters({kZoomedPng, kActualPng, kZoomedJpg, kActualJpg});
    dialog.selectNameFilter(kZoomedPng);
    dialog.setDefaultSuffix(QStringLiteral("png"));
    connect(&dialog, &QFileDialog::filterSelected, &dialog, [&dialog](const QString &filter) {
        dialog.setDefaultSuffix(filter.contains(QLatin1String("JPEG")) ? QStringLiteral("jpg") : QStringLiteral("png"));
    });
    const bool accepted = dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty();
    if (platform::isX11() && isVisible()) {
        x11util::activate(windowHandle());
        x11util::grabKeyboard(windowHandle());
    }
    activateWindow();
    if (!accepted)
        return;

    const QString path = dialog.selectedFiles().constFirst();
    const bool actual = dialog.selectedNameFilter().startsWith(QLatin1String("Actual"));
    const QImage image = actual ? renderActual(area) : renderView(area);
    const bool jpeg = path.endsWith(QLatin1String(".jpg"), Qt::CaseInsensitive)
                      || path.endsWith(QLatin1String(".jpeg"), Qt::CaseInsensitive);
    if (!image.save(path, jpeg ? "JPG" : "PNG", jpeg ? 92 : -1)) {
        showHint(tr("Could not save %1").arg(path));
        return;
    }
    m_settings->setLastSaveDirectory(QFileInfo(path).absolutePath());
    showHint(tr("Saved %1").arg(QFileInfo(path).fileName()));
    Q_EMIT saved(path);
}

// ---------------------------------------------------------------- events

void OverlayWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QImage &img = m_canvas.composed();
    const double sdpr = img.devicePixelRatio();
    if (m_kind == Kind::LiveDraw)
        painter.setCompositionMode(QPainter::CompositionMode_Source);
    const bool whole = qFuzzyCompare(m_zoom, 1.0) && m_view.topLeft().isNull();
    if (whole) {
        painter.drawImage(QRectF(rect()), img);
    } else {
        painter.setRenderHint(QPainter::SmoothPixmapTransform,
                              m_settings->smoothZoom() || m_zoomAnimation.state() == QAbstractAnimation::Running);
        painter.drawImage(QRectF(rect()), img,
                          QRectF(m_view.x() * sdpr, m_view.y() * sdpr, m_view.width() * sdpr, m_view.height() * sdpr));
    }
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    if (m_drawing || m_typing) {
        // Shapes in progress are painted in screenshot coordinates through the view.
        painter.save();
        painter.scale(width() / m_view.width(), height() / m_view.height());
        painter.translate(-m_view.topLeft());
        if (m_drawing)
            Canvas::paintShape(painter, m_current, img);
        if (m_typing) {
            Canvas::paintShape(painter, m_text, img);
            if (m_caretVisible) {
                const QFontMetricsF fm(m_text.font);
                const QStringList lines = m_text.text.split(QLatin1Char('\n'));
                const QPointF anchor = m_text.points.first();
                const double x = m_text.alignRight ? anchor.x() + 1 : anchor.x() + fm.horizontalAdvance(lines.last()) + 1;
                const double y = anchor.y() + fm.lineSpacing() * (lines.size() - 1);
                QColor caret = m_text.color;
                caret.setAlpha(255);
                painter.fillRect(QRectF(x, y, qMax(1.0, m_text.font.pixelSize() / 16.0), fm.height()), caret);
            }
        }
        painter.restore();
    }

    if (m_cropping)
        paintCrop(painter);

    if (!m_hint.isEmpty()) {
        QFont font = painter.font();
        font.setPixelSize(15);
        painter.setFont(font);
        const QFontMetrics fm(font);
        QRectF box(0, 0, fm.horizontalAdvance(m_hint) + 32, fm.height() + 18);
        box.moveCenter(QPointF(width() / 2.0, height() - 64));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(20, 20, 20, 210));
        painter.drawRoundedRect(box, box.height() / 2, box.height() / 2);
        painter.setPen(Qt::white);
        painter.drawText(box, Qt::AlignCenter, m_hint);
    }
}

void OverlayWindow::enterEvent(QEnterEvent *event)
{
    notePointer(event->position());
    QWidget::enterEvent(event);
}

void OverlayWindow::mousePressEvent(QMouseEvent *event)
{
    notePointer(event->position());
    if (!m_started || m_leaving)
        return;
    const QPointF pos = event->position();
    const Qt::MouseButton button = event->button();

    if (m_cropping) {
        if (button == Qt::LeftButton) {
            m_selecting = true;
            m_selectStart = m_selectEnd = pos;
            m_hint.clear();
            update();
        } else if (button == Qt::RightButton) {
            cancelCrop();
        }
        return;
    }

    if (button == Qt::RightButton) {
        if (m_typing)
            endTyping();
        else if (m_penMode && m_kind != Kind::LiveDraw)
            leavePenMode();
        else
            leave();
        return;
    }
    if (button != Qt::LeftButton)
        return;

    if (m_typing) {
        // The first click places the text; the next one finishes it.
        if (!m_textPlaced) {
            m_textPlaced = true;
            m_text.points = {toSource(pos)};
            update();
        } else {
            endTyping();
        }
        return;
    }
    if (!m_penMode) {
        // ZoomIt ignores the click until the zoom has settled.
        if (m_zoomAnimation.state() != QAbstractAnimation::Running)
            enterPenMode();
        return;
    }
    beginShape(toSource(pos), event->modifiers());
}

void OverlayWindow::mouseMoveEvent(QMouseEvent *event)
{
    notePointer(event->position());
    if (!m_started)
        return;
    const QPointF pos = event->position();
    if (m_cropping) {
        if (m_selecting) {
            m_selectEnd = pos;
            update();
        }
        return;
    }
    if (m_typing && !m_textPlaced) {
        m_text.points = {toSource(pos)};
        update();
        return;
    }
    if (m_drawing) {
        const QPointF source = toSource(pos);
        if (m_current.kind == Shape::Freehand) {
            const QPointF delta = source - m_current.points.last();
            if (QPointF::dotProduct(delta, delta) * m_zoom * m_zoom >= 1.0)
                m_current.points << source;
        } else {
            m_current.points.last() = source;
        }
        update();
        return;
    }
    if (!m_viewFrozen && !m_leaving) {
        updateView();
        update();
    }
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_cropping && m_selecting) {
        m_selectEnd = event->position();
        const QRectF area = QRectF(m_selectStart, m_selectEnd).normalized().intersected(QRectF(rect()));
        if (area.width() >= 3 && area.height() >= 3) {
            finishCrop(area);
        } else {
            m_selecting = false;
            update();
        }
    } else if (m_drawing) {
        commitShape();
    }
}

void OverlayWindow::wheelEvent(QWheelEvent *event)
{
    if (!m_started || m_leaving || m_cropping)
        return;
    // Whole notches; smooth-scrolling devices accumulate up to one.
    m_wheelRemainder += event->angleDelta().y();
    const int notches = m_wheelRemainder / 120;
    m_wheelRemainder %= 120;
    if (notches == 0)
        return;

    if (m_typing) {
        m_fontScale = qBound(1, m_fontScale - notches, 50);
        m_settings->setFontScale(m_fontScale);
        updateTextFont();
        update();
    } else if ((event->modifiers() & Qt::ControlModifier) && m_penMode) {
        changePenWidth(notches);
    } else {
        stepZoom(notches);
    }
}

void OverlayWindow::keyPressEvent(QKeyEvent *event)
{
    if (m_keyTarget && m_keyTarget != this)
        m_keyTarget->handleKeyPress(event);
    else
        handleKeyPress(event);
}

void OverlayWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (m_keyTarget && m_keyTarget != this)
        m_keyTarget->handleKeyRelease(event);
    else
        handleKeyRelease(event);
}

std::optional<Action> OverlayWindow::hotkeyFor(QKeyEvent *event) const
{
    const QKeyCombination pressed(event->modifiers() & ~Qt::KeypadModifier, Qt::Key(event->key()));
    for (Action action : configurableActions()) {
        for (const QKeySequence &sequence : m_settings->shortcuts(action)) {
            if (!sequence.isEmpty() && sequence[0] == pressed)
                return action;
        }
    }
    return std::nullopt;
}

void OverlayWindow::handleKeyRelease(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;
    if (event->key() == Qt::Key_Tab) {
        m_tabDown = false;
        return;
    }
    // ZoomIt starts typing when T is released.
    if (event->key() == Qt::Key_T && m_started && !m_leaving && !m_typing && !m_cropping
        && !(event->modifiers() & Qt::ControlModifier)) {
        startTyping(event->modifiers() & Qt::ShiftModifier);
    }
}

void OverlayWindow::handleKeyPress(QKeyEvent *event)
{
    if (m_leaving)
        return;
    // Our own hotkeys reach us here while the keyboard is grabbed (X11).
    if (const auto action = hotkeyFor(event)) {
        if (!event->isAutoRepeat())
            Q_EMIT hotkeyPressed(*action);
        return;
    }

    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;
    const bool ctrl = mods & Qt::ControlModifier;
    const bool shift = mods & Qt::ShiftModifier;

    if (!m_started) {
        if (key == Qt::Key_Escape)
            leave();
        return;
    }

    if (m_cropping) {
        if (key == Qt::Key_Escape)
            cancelCrop();
        else if (key == Qt::Key_Return || key == Qt::Key_Enter)
            finishCrop(QRectF(rect()));
        return;
    }

    if (m_typing) {
        if (key == Qt::Key_Escape) {
            endTyping();
        } else if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            m_textPlaced = true;
            m_text.text += QLatin1Char('\n');
        } else if (key == Qt::Key_Backspace || key == Qt::Key_Delete) {
            m_text.text.chop(1);
        } else if (key == Qt::Key_Up || key == Qt::Key_Down) {
            m_fontScale = qBound(1, m_fontScale + (key == Qt::Key_Up ? -1 : 1), 50);
            m_settings->setFontScale(m_fontScale);
            updateTextFont();
        } else if (!ctrl && !event->text().isEmpty() && event->text().at(0).isPrint()) {
            m_textPlaced = true;
            m_text.text += event->text();
        }
        m_caretVisible = true;
        update();
        return;
    }

    if (ctrl) {
        switch (key) {
        case Qt::Key_Z:
            if (!m_drawing && m_canvas.undo())
                update();
            break;
        case Qt::Key_C:
            if (shift)
                startCrop(CropPurpose::Copy, false);
            else
                copyArea(QRectF(rect()), false);
            break;
        case Qt::Key_S:
            if (shift)
                startCrop(CropPurpose::Save, false);
            else
                saveArea(QRectF(rect()));
            break;
        case Qt::Key_W:
        case Qt::Key_K:
            if (m_kind == Kind::LiveDraw)
                break;
            m_canvas.setBoard(key == Qt::Key_W ? Board::Whiteboard : Board::Blackboard);
            enterPenMode();
            update();
            break;
        case Qt::Key_Up:
        case Qt::Key_Down:
            if (m_penMode)
                changePenWidth(key == Qt::Key_Up ? 1 : -1);
            else
                stepZoom(key == Qt::Key_Up ? 1 : -1);
            break;
        default:
            break;
        }
        return;
    }

    const QColor color = colorForKey(key);
    if (color.isValid()) {
        setPenColor(color, shift);
        return;
    }
    switch (key) {
    case Qt::Key_Escape:
        leave();
        break;
    case Qt::Key_X:
        setBlur(shift ? kStrongBlur : kNormalBlur);
        break;
    case Qt::Key_E:
        if (!m_drawing) {
            m_canvas.clear();
            update();
        }
        break;
    case Qt::Key_Up:
        stepZoom(1);
        break;
    case Qt::Key_Down:
        stepZoom(-1);
        break;
    case Qt::Key_Space:
        // Centre the pointer in the view. Only X11 lets applications move it.
        if (m_penMode && !m_drawing && platform::isX11()) {
            m_cursor = QPointF(width() / 2.0, height() / 2.0);
            QCursor::setPos(mapToGlobal(m_cursor.toPoint()));
        }
        break;
    case Qt::Key_Tab:
        if (!event->isAutoRepeat())
            m_tabDown = true;
        break;
    default:
        break;
    }
}

void OverlayWindow::showHint(const QString &text)
{
    m_hint = text;
    m_hintTimer.start();
    update();
}
