#pragma once

#include <cstdint>
#include <string>

constexpr const char* DEFAULT_SRT_URL = "srt://127.0.0.1:8890?streamid=publish:aula&latency=50";
constexpr const char* DEFAULT_RTSP_URL = "rtsp://127.0.0.1:8554/aula";

constexpr int VIDEO_WIDTH = 640;
constexpr int VIDEO_HEIGHT = 480;
constexpr int VIDEO_FPS = 30;
constexpr int VIDEO_BITRATE_KBPS = 1500;
constexpr int VIDEO_KEY_INT_MAX = 30;
constexpr const char* VIDEO_PROFILE = "constrained-baseline";

constexpr int AUDIO_RATE = 48000;
constexpr int AUDIO_CHANNELS = 2;

constexpr int DEFAULT_RECEIVER_LATENCY_MS = 200;

enum class MediaSource { Test, Camera };

struct TransmitterConfig {
    std::string url = DEFAULT_SRT_URL;
    MediaSource source = MediaSource::Test;
};

struct ReceiverConfig {
    std::string url = DEFAULT_RTSP_URL;
    uint32_t latency_ms = DEFAULT_RECEIVER_LATENCY_MS;
};

struct JitterStats {
    uint64_t num_pushed = 0;
    uint64_t num_lost = 0;
    uint64_t num_late = 0;
    uint64_t num_duplicates = 0;
    double avg_jitter_ms = 0.0;
};
