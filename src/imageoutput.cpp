#include "imageoutput.h"

#include "config.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QMimeData>

namespace imageoutput {

void copyToClipboard(const QImage &image)
{
    auto *mime = new QMimeData;
    mime->setImageData(image);
    QGuiApplication::clipboard()->setMimeData(mime, QClipboard::Clipboard);
}

QString saveToDirectory(const QImage &image, const QString &directory, const QString &prefix)
{
    QDir dir(directory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return {};
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    QString path = dir.filePath(QStringLiteral("%1 %2.png").arg(prefix, stamp));
    for (int n = 2; QFile::exists(path); ++n)
        path = dir.filePath(QStringLiteral("%1 %2 (%3).png").arg(prefix, stamp).arg(n));
    return image.save(path, "PNG") ? path : QString();
}

}  // namespace imageoutput
