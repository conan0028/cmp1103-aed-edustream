#pragma once

#include <functional>
#include <string>

#include <gst/gst.h>

#include "config.hpp"

class Transmitter {
public:
    ~Transmitter() { stop(); }

    bool start(const TransmitterConfig& cfg);
    void stop();
    bool is_running() const { return pipeline_ != nullptr; }

    std::function<void(const std::string&)> on_status;

private:
    static gboolean on_bus_message(GstBus*, GstMessage* msg, gpointer self);
    void status(const std::string& text) const;

    GstElement* pipeline_ = nullptr;
    guint bus_watch_ = 0;
};
