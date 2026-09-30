#include "recorder.h"

#include "capture/gstutil.h"
#include "capture/screencast.h"
#include "config.h"
#include "platform.h"
#include "settings.h"

#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLoggingCategory>

#include <xcb/xcb.h>

#include <cmath>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcRecord, "app.record")

namespace {

// What part of the incoming frames to keep. Frames may arrive at a different
// scale than the logical coordinates the region was picked in, so the crop is
// worked out once the frame size is known.
struct CropPlan {
    QRectF area;           // in the stream's logical coordinates; empty = everything
    double logicalWidth;   // the stream's logical width, to derive the scale
};

GstPadProbeReturn onCaps(GstPad *pad, GstPadProbeInfo *info, gpointer data)
{
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
    if (!event || GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
        return GST_PAD_PROBE_OK;
    GstCaps *caps = nullptr;
    gst_event_parse_caps(event, &caps);
    const GstStructure *s = gst_caps_get_structure(caps, 0);
    int width = 0;
    int height = 0;
    if (!gst_structure_get_int(s, "width", &width) || !gst_structure_get_int(s, "height", &height))
        return GST_PAD_PROBE_OK;

    const auto *plan = static_cast<const CropPlan *>(data);
    int left = 0, top = 0, right = 0, bottom = 0;
    if (!plan->area.isEmpty()) {
        const double scale = plan->logicalWidth > 0 ? width / plan->logicalWidth : 1.0;
        left = qBound(0, int(std::lround(plan->area.x() * scale)), width - 2);
        top = qBound(0, int(std::lround(plan->area.y() * scale)), height - 2);
        const int w = qBound(2, int(std::lround(plan->area.width() * scale)), width - left);
        const int h = qBound(2, int(std::lround(plan->area.height() * scale)), height - top);
        right = width - left - w;
        bottom = height - top - h;
    }
    // H.264 in 4:2:0 needs even dimensions.
    if ((width - left - right) % 2)
        ++right;
    if ((height - top - bottom) % 2)
        ++bottom;
    GstElement *crop = gst_pad_get_parent_element(pad);
    g_object_set(crop, "left", left, "top", top, "right", right, "bottom", bottom, nullptr);
    gst_object_unref(crop);
    return GST_PAD_PROBE_OK;
}

const char *firstAvailable(std::initializer_list<const char *> elements)
{
    for (const char *e : elements) {
        if (gstutil::hasElement(e))
            return e;
    }
    return nullptr;
}

// Can we reach a sound server to record from? Without one pulsesrc would
// fail the whole recording, so check first and record video only.
bool audioSourceWorks()
{
    GstElement *probe = gst_element_factory_make("pulsesrc", nullptr);
    if (!probe)
        return false;
    g_object_set(probe, "device", "@DEFAULT_MONITOR@", nullptr);
    const bool ok = gst_element_set_state(probe, GST_STATE_READY) != GST_STATE_CHANGE_FAILURE;
    gst_element_set_state(probe, GST_STATE_NULL);
    gst_object_unref(probe);
    return ok;
}

// Top-level X11 window under the pointer, or 0.
quint32 windowUnderPointer()
{
    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    xcb_connection_t *c = x11 ? x11->connection() : nullptr;
    if (!c)
        return 0;
    const xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;
    xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(c, xcb_query_pointer(c, root), nullptr);
    const quint32 child = reply ? reply->child : 0;
    free(reply);
    return child;
}

}  // namespace

Recorder::Recorder(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_bus.setInterval(100);
    connect(&m_bus, &QTimer::timeout, this, &Recorder::pollBus);
}

Recorder::~Recorder()
{
    teardown();
}

bool Recorder::isAvailable()
{
    if (!gstutil::init())
        return false;
    const bool source = platform::isX11() ? gstutil::hasElement("ximagesrc") : gstutil::hasElement("pipewiresrc");
    const bool encoder = (gstutil::hasElement("x264enc") && gstutil::hasElement("mp4mux"))
                         || (gstutil::hasElement("vp8enc") && gstutil::hasElement("webmmux"));
    return source && encoder && gstutil::hasElement("videocrop");
}

QString Recorder::outputPath(const QString &extension) const
{
    QDir dir(m_settings->recordDirectory());
    dir.mkpath(QStringLiteral("."));
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH-mm-ss"));
    QString path = dir.filePath(QStringLiteral(APP_NAME " %1.%2").arg(stamp, extension));
    for (int n = 2; QFile::exists(path); ++n)
        path = dir.filePath(QStringLiteral(APP_NAME " %1 (%2).%3").arg(stamp).arg(n).arg(extension));
    return path;
}

void Recorder::start(Mode mode, QScreen *screen, const QRect &region)
{
    if (isRecording())
        return;
    m_retriedWithoutToken = false;
    if (!isAvailable()) {
        Q_EMIT failed(tr("Recording needs GStreamer with an H.264 or VP8 encoder "
                         "(gstreamer1.0-plugins-ugly or -good) and %1.")
                          .arg(platform::isX11() ? QStringLiteral("gstreamer1.0-x") : QStringLiteral("gstreamer1.0-pipewire")));
        return;
    }
    m_mode = mode;
    m_screen = screen ? screen : QGuiApplication::primaryScreen();
    m_region = region;
    m_stopping = false;

    if (!platform::isX11()) {
        startWayland();
        return;
    }

    // X11: read the root window (or one window) directly.
    const double dpr = m_screen->devicePixelRatio();
    QString source;
    if (mode == Mode::Window) {
        if (const quint32 xid = windowUnderPointer())
            source = QStringLiteral("ximagesrc use-damage=false show-pointer=true xid=%1").arg(xid);
    }
    if (source.isEmpty()) {
        // Qt keeps each screen's top-left corner in device pixels and scales
        // from there, so map through the screen's origin.
        const QRect area = mode == Mode::Region && !region.isEmpty() ? region : m_screen->geometry();
        const QPoint origin = m_screen->geometry().topLeft();
        const QPoint topLeft = origin + QPoint(qRound((area.x() - origin.x()) * dpr), qRound((area.y() - origin.y()) * dpr));
        const QRect device(topLeft, QSize(qRound(area.width() * dpr), qRound(area.height() * dpr)));
        source = QStringLiteral("ximagesrc use-damage=false show-pointer=true startx=%1 starty=%2 endx=%3 endy=%4")
                     .arg(device.left())
                     .arg(device.top())
                     .arg(device.right())
                     .arg(device.bottom());
    }
    QString error;
    if (!launch(source, QRect(), 0, &error)) {
        teardown();
        Q_EMIT failed(error);
    }
}

void Recorder::startWayland()
{
    m_session = new ScreenCastSession(this);
    connect(m_session, &ScreenCastSession::failed, this, [this](const QString &reason) {
        if (m_mode != Mode::Window)
            m_settings->setRecordToken(QString());
        teardown();
        Q_EMIT failed(reason);
    });
    connect(m_session, &ScreenCastSession::started, this, &Recorder::onCastStarted);

    ScreenCastSession::Options options;
    options.types = m_mode == Mode::Window ? ScreenCastSession::Window : ScreenCastSession::Monitor;
    options.multiple = false;
    options.cursorMode = ScreenCastSession::Embedded;
    // A remembered window would be stale; a remembered monitor is what people want.
    options.persist = m_mode != Mode::Window;
    if (m_mode != Mode::Window)
        options.restoreToken = m_settings->recordToken();
    m_session->start(options);
}

void Recorder::onCastStarted()
{
    if (m_mode != Mode::Window)
        m_settings->setRecordToken(m_session->restoreToken());
    const CastStream stream = m_session->streams().constFirst();
    QString error;
    const int fd = m_session->openPipeWireRemote(&error);
    if (fd < 0) {
        teardown();
        Q_EMIT failed(error);
        return;
    }

    // A remembered monitor that is not where the region was picked: forget
    // it and ask again, or we would record the wrong screen.
    if (m_mode == Mode::Region && !stream.geometry.isEmpty() && !stream.geometry.intersects(m_region)
        && !m_retriedWithoutToken) {
        ::close(fd);
        m_retriedWithoutToken = true;
        m_settings->setRecordToken(QString());
        m_session->close();
        m_session->deleteLater();
        m_session = nullptr;
        startWayland();
        return;
    }

    QRectF crop;
    double logicalWidth = stream.geometry.width();
    if (m_mode == Mode::Region && !m_region.isEmpty()) {
        // The region was picked on one of our screens; express it relative to
        // the monitor being streamed.
        const QRect monitor = stream.geometry.isEmpty() && m_screen ? m_screen->geometry() : stream.geometry;
        crop = QRectF(m_region.translated(-monitor.topLeft())).intersected(QRectF(QPointF(0, 0), QSizeF(monitor.size())));
        logicalWidth = monitor.width();
    }
    const QString source =
        QStringLiteral("pipewiresrc fd=%1 path=%2 do-timestamp=true keepalive-time=1000 always-copy=true")
            .arg(fd)
            .arg(stream.node);
    QString launchError;
    const bool ok = launch(source, crop.toAlignedRect(), logicalWidth, &launchError);
    ::close(fd);  // pipewiresrc has its own duplicate
    if (!ok) {
        teardown();
        Q_EMIT failed(launchError);
    }
}

bool Recorder::launch(const QString &videoSource, const QRect &crop, double logicalWidth, QString *error)
{
    const bool mp4 = gstutil::hasElement("x264enc") && gstutil::hasElement("mp4mux");
    QString video = mp4 ? QStringLiteral("x264enc speed-preset=veryfast tune=zerolatency key-int-max=60 bitrate=12000 ! "
                                         "video/x-h264,profile=high,stream-format=avc,alignment=au")
                        : QStringLiteral("vp8enc deadline=1 cpu-used=8 target-bitrate=8000000 keyframe-max-dist=60");
    const char *audioEncoder = mp4 ? firstAvailable({"avenc_aac", "fdkaacenc", "voaacenc"}) : firstAvailable({"opusenc"});
    const bool audio = m_settings->recordAudio() && audioEncoder && audioSourceWorks();
    // No faststart: it spools the whole video through a temporary file and
    // copies it at the end, which can outlast our stop timeout.
    const QString muxer = mp4 ? QStringLiteral("mp4mux name=mux") : QStringLiteral("webmmux name=mux");

    QString description = QStringLiteral("%1 ! queue ! videoconvert ! videorate ! video/x-raw,framerate=%2/1 ! "
                                         "videocrop name=crop ! videoconvert ! video/x-raw,format=I420 ! %3 ! queue ! mux. ")
                              .arg(videoSource)
                              .arg(m_settings->recordFrameRate())
                              .arg(video);
    if (audio) {
        description += QStringLiteral("pulsesrc device=@DEFAULT_MONITOR@ do-timestamp=true ! queue ! audioconvert ! "
                                      "audioresample ! %1 ! queue ! mux. ")
                           .arg(QLatin1String(audioEncoder));
    }
    description += muxer + QStringLiteral(" ! filesink name=out");

    GError *parseError = nullptr;
    GstElement *pipeline = gst_parse_launch(description.toUtf8().constData(), &parseError);
    if (!pipeline || parseError) {
        *error = QString::fromUtf8(parseError ? parseError->message : "cannot build the recording pipeline");
        g_clear_error(&parseError);
        if (pipeline)
            gst_object_unref(pipeline);
        return false;
    }
    m_pipeline = pipeline;
    m_path = outputPath(mp4 ? QStringLiteral("mp4") : QStringLiteral("webm"));
    GstElement *out = gst_bin_get_by_name(GST_BIN(pipeline), "out");
    g_object_set(out, "location", m_path.toUtf8().constData(), nullptr);
    gst_object_unref(out);

    GstElement *cropper = gst_bin_get_by_name(GST_BIN(pipeline), "crop");
    GstPad *pad = gst_element_get_static_pad(cropper, "sink");
    auto *plan = new CropPlan{QRectF(crop), logicalWidth};
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, onCaps, plan,
                      [](gpointer data) { delete static_cast<CropPlan *>(data); });
    gst_object_unref(pad);
    gst_object_unref(cropper);

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        *error = tr("The recording pipeline did not start (is %1 writable?).").arg(QFileInfo(m_path).absolutePath());
        return false;
    }
    qCInfo(lcRecord).noquote() << "recording to" << m_path << (audio ? "with audio" : "without audio");
    m_bus.start();
    Q_EMIT started();
    return true;
}

void Recorder::stop()
{
    if (!m_pipeline) {
        // Still waiting for the portal (a dialog may be open): give up.
        if (m_session) {
            teardown();
            Q_EMIT failed(tr("Recording cancelled."));
        }
        return;
    }
    if (m_stopping)
        return;
    m_stopping = true;
    qCInfo(lcRecord) << "stopping";
    // End of stream lets the muxer write its index; without it the file is unplayable.
    gst_element_send_event(m_pipeline, gst_event_new_eos());
    const int generation = ++m_generation;
    QTimer::singleShot(15000, this, [this, generation] {
        if (m_pipeline && m_stopping && generation == m_generation) {
            qCWarning(lcRecord) << "no end of stream after 15 s, closing anyway";
            const QString path = m_path;
            teardown();
            Q_EMIT finished(path);
        }
    });
}

void Recorder::pollBus()
{
    if (!m_pipeline)
        return;
    GstBus *bus = gst_element_get_bus(m_pipeline);
    while (GstMessage *message = gst_bus_pop(bus)) {
        switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_EOS: {
            const QString path = m_path;
            qCInfo(lcRecord).noquote() << "finished" << path;
            gst_message_unref(message);
            gst_object_unref(bus);
            teardown();
            Q_EMIT finished(path);
            return;
        }
        case GST_MESSAGE_ERROR: {
            GError *error = nullptr;
            gchar *debug = nullptr;
            gst_message_parse_error(message, &error, &debug);
            const QString text = QString::fromUtf8(error ? error->message : "recording error");
            qCWarning(lcRecord) << text << debug;
            g_clear_error(&error);
            g_free(debug);
            gst_message_unref(message);
            gst_object_unref(bus);
            teardown();
            Q_EMIT failed(text);
            return;
        }
        default:
            break;
        }
        gst_message_unref(message);
    }
    gst_object_unref(bus);
}

void Recorder::teardown()
{
    m_bus.stop();
    m_stopping = false;
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
    if (m_session) {
        m_session->close();
        m_session->deleteLater();
        m_session = nullptr;
    }
}
