#include "transmitter.hpp"

#include <sstream>

static std::string build_description(const TransmitterConfig& cfg) {
    std::ostringstream d;
    const bool cam = cfg.source == MediaSource::Camera;

    if (!cam) {
        d << "videotestsrc is-live=true";
    } else {
#ifdef _WIN32
        d << "autovideosrc";
#else
        d << "v4l2src";
#endif
    }
    d << " ! videoconvert ! videoscale ! videorate"
      << " ! video/x-raw,format=I420,width=" << VIDEO_WIDTH << ",height=" << VIDEO_HEIGHT
      << ",framerate=" << VIDEO_FPS << "/1"
      << " ! queue max-size-buffers=2 leaky=downstream ! x264enc tune=zerolatency speed-preset=ultrafast bitrate=" << VIDEO_BITRATE_KBPS
      << " key-int-max=" << VIDEO_KEY_INT_MAX
      << " ! video/x-h264,profile=" << VIDEO_PROFILE << " ! h264parse config-interval=-1 ! mux. ";

    if (!cam)
        d << "audiotestsrc is-live=true wave=sine freq=440";
    else
        d << "autoaudiosrc";
    d << " ! audioconvert ! audioresample"
      << " ! audio/x-raw,rate=" << AUDIO_RATE << ",channels=" << AUDIO_CHANNELS
      << " ! queue max-size-buffers=2 leaky=downstream ! opusenc ! opusparse ! mux. ";

    d << "mpegtsmux name=mux alignment=7 latency=500000000 ! srtsink name=sink wait-for-connection=false";
    return d.str();
}

void Transmitter::status(const std::string& text) const {
    if (on_status)
        on_status(text);
}

gboolean Transmitter::on_bus_message(GstBus*, GstMessage* msg, gpointer data) {
    auto* self = static_cast<Transmitter*>(data);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError* err = nullptr;
        gchar* dbg = nullptr;
        gst_message_parse_error(msg, &err, &dbg);
        self->status(std::string("Erro: ") + err->message);
        g_error_free(err);
        g_free(dbg);
        break;
    }
    case GST_MESSAGE_EOS:
        self->status("EOS");
        break;
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(self->pipeline_)) {
            GstState old_s, new_s;
            gst_message_parse_state_changed(msg, &old_s, &new_s, nullptr);
            self->status(std::string("Estado: ") + gst_element_state_get_name(new_s));
        }
        break;
    default:
        break;
    }
    return G_SOURCE_CONTINUE;
}

bool Transmitter::start(const TransmitterConfig& cfg) {
    if (pipeline_)
        return false;

    if (!gst_uri_is_valid(cfg.url.c_str())) {
        status("Erro: URL SRT invalida");
        return false;
    }

    GError* err = nullptr;
    pipeline_ = gst_parse_launch(build_description(cfg).c_str(), &err);
    if (!pipeline_) {
        status(std::string("Erro ao criar pipeline: ") + err->message);
        g_error_free(err);
        return false;
    }

    GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
    g_object_set(sink, "uri", cfg.url.c_str(), NULL);
    gst_object_unref(sink);

    GstBus* bus = gst_element_get_bus(pipeline_);
    bus_watch_ = gst_bus_add_watch(bus, on_bus_message, this);
    gst_object_unref(bus);

    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        status("Erro ao iniciar pipeline");
        stop();
        return false;
    }
    return true;
}

void Transmitter::stop() {
    if (!pipeline_)
        return;
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    g_source_remove(bus_watch_);
    bus_watch_ = 0;
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
}
