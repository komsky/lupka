#pragma once

#include <QImage>
#include <QString>

namespace imageoutput {

// Put an image on the clipboard. Under Wayland this must happen while one of
// our windows has keyboard focus.
void copyToClipboard(const QImage &image);

// Save as PNG into `directory` (created if needed) with a timestamped name.
// Returns the file path, or an empty string on failure.
QString saveToDirectory(const QImage &image, const QString &directory, const QString &prefix);

}  // namespace imageoutput
