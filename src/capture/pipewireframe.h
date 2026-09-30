#pragma once

#include <QImage>
#include <QString>

#include <cstdint>

namespace pipewire {

// Whether GStreamer and its PipeWire source element are usable.
bool isAvailable();

// Read one frame of PipeWire stream `node` through the remote `fd` (from the
// ScreenCast portal). Takes ownership of `fd`. Blocks, so run it off the GUI
// thread. Returns a null image and sets `error` on failure.
QImage grabFrame(int fd, uint32_t node, int timeoutMs, QString *error);

}  // namespace pipewire
