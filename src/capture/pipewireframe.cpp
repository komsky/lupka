// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pipewireframe.h"

#include "gstutil.h"

#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include <unistd.h>

namespace pipewire {

bool isAvailable()
{
    return gstutil::init() && gstutil::hasElement("pipewiresrc") && gstutil::hasElement("videoconvert")
           && gstutil::hasElement("appsink");
}

QImage grabFrame(int fd, uint32_t node, int timeoutMs, QString *error)
{
    auto setError = [error](const QString &text) {
        if (error)
            *error = text;
    };
    if (!isAvailable()) {
        ::close(fd);
        setError(QStringLiteral("GStreamer's PipeWire plugin is not installed (gstreamer1.0-pipewire)"));
        return {};
    }

    // pipewiresrc duplicates the fd for its own connection.
    const QByteArray description =
        QStringLiteral("pipewiresrc fd=%1 path=%2 always-copy=true do-timestamp=true ! videoconvert ! "
                       "video/x-raw,format=BGRx ! appsink name=sink max-buffers=1 drop=true sync=false")
            .arg(fd)
            .arg(node)
            .toUtf8();
    GError *parseError = nullptr;
    GstElement *pipeline = gst_parse_launch(description.constData(), &parseError);
    if (!pipeline) {
        setError(QString::fromUtf8(parseError ? parseError->message : "cannot build pipeline"));
        g_clear_error(&parseError);
        ::close(fd);
        return {};
    }
    g_clear_error(&parseError);

    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    QImage image;
    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        setError(QStringLiteral("could not start the PipeWire stream"));
    } else if (GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), GstClockTime(timeoutMs) * GST_MSECOND)) {
        GstVideoInfo info;
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        if (gst_video_info_from_caps(&info, gst_sample_get_caps(sample)) && buffer
            && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            const int width = GST_VIDEO_INFO_WIDTH(&info);
            const int height = GST_VIDEO_INFO_HEIGHT(&info);
            const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
            image = QImage(map.data, width, height, stride, QImage::Format_RGB32).copy();
            gst_buffer_unmap(buffer, &map);
        } else {
            setError(QStringLiteral("unreadable frame"));
        }
        gst_sample_unref(sample);
    } else {
        setError(QStringLiteral("no frame arrived from the screen cast"));
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(sink);
    gst_object_unref(pipeline);
    ::close(fd);
    return image;
}

}  // namespace pipewire
