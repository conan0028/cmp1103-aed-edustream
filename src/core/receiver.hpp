#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <gst/gst.h>

#include "config.hpp"

class Receiver {
public:
    ~Receiver() { stop(); }

    bool start(const ReceiverConfig& cfg);
    void stop();
    bool is_running() const { return pipeline_ != nullptr; }

    std::function<void(const std::string&)> on_status;
    std::function<void(const std::string&)> on_event;
    std::function<void(const JitterStats&)> on_stats;

private:
    static void on_pad_added(GstElement*, GstPad* pad, gpointer self);
    static void on_deep_element_added(GstBin*, GstBin*, GstElement* element, gpointer self);
    static GstPadProbeReturn on_lost_event(GstPad*, GstPadProbeInfo* info, gpointer self);
    static gboolean on_bus_message(GstBus*, GstMessage* msg, gpointer self);
    static gboolean on_stats_timer(gpointer self);

    void status(const std::string& text) const;
    void event(const std::string& text) const;

    GstElement* pipeline_ = nullptr;
    guint bus_watch_ = 0;
    guint stats_timer_ = 0;
    std::mutex mutex_;
    std::vector<GstElement*> jitterbuffers_;
};
