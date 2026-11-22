#include "receiver.hpp"

#include <cstdio>

void Receiver::status(const std::string& text) const {
    if (on_status)
        on_status(text);
}

void Receiver::event(const std::string& text) const {
    if (on_event)
        on_event(text);
}

void Receiver::on_pad_added(GstElement*, GstPad* pad, gpointer data) {
    auto* self = static_cast<Receiver*>(data);

    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, nullptr);
    const gchar* media = gst_structure_get_string(gst_caps_get_structure(caps, 0), "media");

    const gchar* branch = nullptr;
    if (media && g_str_equal(media, "video"))
        branch = "queue ! rtph264depay ! h264parse ! openh264dec ! videoconvert ! autovideosink";
    else if (media && g_str_equal(media, "audio"))
        branch = "queue ! rtpopusdepay ! opusdec ! audioconvert ! audioresample ! autoaudiosink";
    gst_caps_unref(caps);
    if (!branch)
        return;

    GError* err = nullptr;
    GstElement* bin = gst_parse_bin_from_description(branch, TRUE, &err);
    if (!bin) {
        self->status(std::string("Erro ao criar ramo: ") + err->message);
        g_error_free(err);
        return;
    }
    gst_bin_add(GST_BIN(self->pipeline_), bin);
    gst_element_sync_state_with_parent(bin);

    GstPad* sink = gst_element_get_static_pad(bin, "sink");
    if (gst_pad_link(pad, sink) != GST_PAD_LINK_OK)
        self->status(std::string("Erro ao ligar ramo de ") + media);
    gst_object_unref(sink);
}

GstPadProbeReturn Receiver::on_lost_event(GstPad*, GstPadProbeInfo* info, gpointer data) {
    auto* self = static_cast<Receiver*>(data);
    const GstStructure* s = gst_event_get_structure(GST_PAD_PROBE_INFO_EVENT(info));

    if (s && gst_structure_has_name(s, "GstRTPPacketLost")) {
        guint seqnum = 0;
        guint64 timestamp = 0;
        gst_structure_get_uint(s, "seqnum", &seqnum);
        gst_structure_get_uint64(s, "timestamp", &timestamp);
        char buf[128];
        std::snprintf(buf, sizeof buf, "[LOSS] Pacote PERDIDO: seqnum=%u ts=%" G_GUINT64_FORMAT " ms",
                      seqnum, timestamp / GST_MSECOND);
        self->event(buf);
    }
    return GST_PAD_PROBE_OK;
}

void Receiver::on_deep_element_added(GstBin*, GstBin*, GstElement* element, gpointer data) {
    auto* self = static_cast<Receiver*>(data);

    GstElementFactory* factory = gst_element_get_factory(element);
    if (!factory || !g_str_equal(GST_OBJECT_NAME(factory), "rtpjitterbuffer"))
        return;

    g_object_set(element, "do-lost", TRUE, "post-drop-messages", TRUE, NULL);

    GstPad* src = gst_element_get_static_pad(element, "src");
    gst_pad_add_probe(src, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, on_lost_event, self, nullptr);
    gst_object_unref(src);

    std::lock_guard<std::mutex> lock(self->mutex_);
    self->jitterbuffers_.push_back(GST_ELEMENT(gst_object_ref(element)));
}

gboolean Receiver::on_stats_timer(gpointer data) {
    auto* self = static_cast<Receiver*>(data);

    JitterStats total;
    int count = 0;
    double jitter_ns = 0.0;
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        for (GstElement* jb : self->jitterbuffers_) {
            GstStructure* st = nullptr;
            g_object_get(jb, "stats", &st, NULL);
            if (!st)
                continue;
            guint64 pushed = 0, lost = 0, late = 0, dup = 0, avg = 0;
            gst_structure_get_uint64(st, "num-pushed", &pushed);
            gst_structure_get_uint64(st, "num-lost", &lost);
            gst_structure_get_uint64(st, "num-late", &late);
            gst_structure_get_uint64(st, "num-duplicates", &dup);
            gst_structure_get_uint64(st, "avg-jitter", &avg);
            gst_structure_free(st);
            total.num_pushed += pushed;
            total.num_lost += lost;
            total.num_late += late;
            total.num_duplicates += dup;
            jitter_ns += avg;
            ++count;
        }
    }
    if (count == 0)
        return G_SOURCE_CONTINUE;

    total.avg_jitter_ms = jitter_ns / count / 1e6;
    if (self->on_stats)
        self->on_stats(total);
    return G_SOURCE_CONTINUE;
}

gboolean Receiver::on_bus_message(GstBus*, GstMessage* msg, gpointer data) {
    auto* self = static_cast<Receiver*>(data);

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
    case GST_MESSAGE_ELEMENT: {
        const GstStructure* s = gst_message_get_structure(msg);
        if (s && gst_structure_has_name(s, "drop-msg")) {
            guint seqnum = 0;
            guint64 timestamp = 0;
            gst_structure_get_uint(s, "seqnum", &seqnum);
            gst_structure_get_uint64(s, "timestamp", &timestamp);
            const gchar* reason = gst_structure_get_string(s, "reason");
            char buf[160];
            std::snprintf(buf, sizeof buf, "[DROP] Pacote DESCARTADO: seqnum=%u ts=%" G_GUINT64_FORMAT " ms motivo=\"%s\"",
                          seqnum, timestamp / GST_MSECOND, reason ? reason : "?");
            self->event(buf);
        }
        break;
    }
    default:
        break;
    }
    return G_SOURCE_CONTINUE;
}

bool Receiver::start(const ReceiverConfig& cfg) {
    if (pipeline_)
        return false;

    pipeline_ = gst_pipeline_new("receiver");
    GstElement* src = gst_element_factory_make("rtspsrc", nullptr);
    if (!src) {
        status("Erro: plugin rtspsrc ausente");
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        return false;
    }
    g_object_set(src, "location", cfg.url.c_str(), "latency", cfg.latency_ms, NULL);
    gst_util_set_object_arg(G_OBJECT(src), "protocols", "udp");
    gst_bin_add(GST_BIN(pipeline_), src);

    g_signal_connect(src, "pad-added", G_CALLBACK(on_pad_added), this);
    g_signal_connect(pipeline_, "deep-element-added", G_CALLBACK(on_deep_element_added), this);

    GstBus* bus = gst_element_get_bus(pipeline_);
    bus_watch_ = gst_bus_add_watch(bus, on_bus_message, this);
    gst_object_unref(bus);
    stats_timer_ = g_timeout_add_seconds(2, on_stats_timer, this);

    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        status("Erro ao iniciar pipeline");
        stop();
        return false;
    }
    return true;
}

void Receiver::stop() {
    if (!pipeline_)
        return;
    g_source_remove(stats_timer_);
    g_source_remove(bus_watch_);
    stats_timer_ = bus_watch_ = 0;
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (GstElement* jb : jitterbuffers_)
            gst_object_unref(jb);
        jitterbuffers_.clear();
    }
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
}
