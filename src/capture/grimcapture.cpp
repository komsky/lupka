#include "backends.h"

#include <QProcess>
#include <QtConcurrent>

namespace {

struct Target {
    QPointer<QScreen> screen;
    QString output;
    int logicalWidth;
};

ScreenImages grabAll(const QList<Target> &targets)
{
    ScreenImages images;
    for (const Target &target : targets) {
        QProcess grim;
        grim.start(QStringLiteral("grim"), {QStringLiteral("-o"), target.output, QStringLiteral("-t"),
                                            QStringLiteral("ppm"), QStringLiteral("-")});
        if (!grim.waitForFinished(5000) || grim.exitCode() != 0)
            return {};
        QImage image;
        if (!image.loadFromData(grim.readAllStandardOutput(), "PPM"))
            return {};
        // grim renders at the output's buffer scale; derive the ratio from the result.
        image.setDevicePixelRatio(target.logicalWidth > 0 ? double(image.width()) / target.logicalWidth : 1.0);
        images.append({target.screen, image});
    }
    return images;
}

}  // namespace

void GrimCapture::capture(const QList<QScreen *> &screens)
{
    QList<Target> targets;
    for (QScreen *screen : screens)
        targets.append({screen, screen->name(), screen->geometry().width()});

    disconnect(&m_watcher, nullptr, this, nullptr);
    connect(&m_watcher, &QFutureWatcher<ScreenImages>::finished, this, [this] {
        const ScreenImages images = m_watcher.result();
        if (images.isEmpty())
            Q_EMIT failed(QStringLiteral("grim did not produce an image"));
        else
            Q_EMIT finished(images);
    });
    m_watcher.setFuture(QtConcurrent::run(grabAll, targets));
}
