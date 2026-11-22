#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "receiver.hpp"

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

    ReceiverConfig cfg;
    if (argc > 1)
        cfg.url = argv[1];
    if (argc > 2)
        cfg.latency_ms = static_cast<uint32_t>(std::atoi(argv[2]));

    std::ofstream csv;
    if (argc > 3) {
        csv.open(argv[3]);
        if (!csv) {
            std::fprintf(stderr, "Nao foi possivel abrir %s\n", argv[3]);
            return 1;
        }
        csv << "elapsed_s,latency_ms,pushed,lost,late,duplicates,avg_jitter_ms,loss_rate_pct\n";
    }
    const auto t0 = std::chrono::steady_clock::now();

    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
    std::signal(SIGINT, on_sigint);
    g_timeout_add(200, check_quit, loop);

    Receiver rx;
    rx.on_status = [](const std::string& s) {
        std::printf("[RX] %s\n", s.c_str());
        if (s.rfind("Erro", 0) == 0 || s == "EOS")
            g_quit = true;
    };
    rx.on_event = [](const std::string& s) { std::fprintf(stderr, "%s\n", s.c_str()); };
    rx.on_stats = [&](const JitterStats& st) {
        std::printf("[STATS] pushed=%llu lost=%llu late=%llu dup=%llu avg_jitter=%.2fms\n",
                    (unsigned long long)st.num_pushed, (unsigned long long)st.num_lost,
                    (unsigned long long)st.num_late, (unsigned long long)st.num_duplicates,
                    st.avg_jitter_ms);

        if (csv) {
            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            const uint64_t total = st.num_pushed + st.num_lost;
            const double loss_pct = total > 0 ? 100.0 * st.num_lost / total : 0.0;
            char line[160];
            std::snprintf(line, sizeof line, "%.0f,%u,%llu,%llu,%llu,%llu,%.2f,%.2f\n", elapsed, cfg.latency_ms,
                          (unsigned long long)st.num_pushed, (unsigned long long)st.num_lost,
                          (unsigned long long)st.num_late, (unsigned long long)st.num_duplicates,
                          st.avg_jitter_ms, loss_pct);
            csv << line;
            csv.flush();
        }
    };

    if (!rx.start(cfg)) {
        g_main_loop_unref(loop);
        return 1;
    }

    std::printf("Lendo %s, latencia %u ms (Ctrl+C para sair)\n", cfg.url.c_str(), cfg.latency_ms);
    g_main_loop_run(loop);

    rx.stop();
    g_main_loop_unref(loop);
    return 0;
}
