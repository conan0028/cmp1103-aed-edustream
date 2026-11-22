#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>

#include "transmitter.hpp"

static std::atomic<bool> g_quit{false};

static void on_sigint(int) { g_quit = true; }

static gboolean check_quit(gpointer loop) {
    if (g_quit) {
        g_main_loop_quit(static_cast<GMainLoop*>(loop));
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

int main(int argc, char** argv) {
    gst_init(&argc, &argv);

    TransmitterConfig cfg;
    if (argc > 1)
        cfg.url = argv[1];
    if (argc > 2)
        cfg.source = std::strcmp(argv[2], "cam") == 0 ? MediaSource::Camera : MediaSource::Test;

    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
    std::signal(SIGINT, on_sigint);
    g_timeout_add(200, check_quit, loop);

    Transmitter tx;
    tx.on_status = [](const std::string& s) { std::printf("[TX] %s\n", s.c_str()); };
    if (!tx.start(cfg)) {
        g_main_loop_unref(loop);
        return 1;
    }

    std::printf("Publicando em %s (Ctrl+C para sair)\n", cfg.url.c_str());
    g_main_loop_run(loop);

    tx.stop();
    g_main_loop_unref(loop);
    return 0;
}
