#include <atomic>
#include <cstdio>
#include <memory>
#include <string>

#include <gtk/gtk.h>

#include "receiver.hpp"

struct App {
    Receiver rx;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    std::atomic<unsigned> session{0};
    GtkWidget *url, *latency, *start, *stop, *status, *stats, *last_event;
};

enum class Kind { Status, Event, Stats };

struct Msg {
    App* app;
    std::shared_ptr<bool> alive;
    unsigned session;
    Kind kind;
    std::string text;
    JitterStats stats;
};

static void set_status(App* a, const std::string& text) {
    gtk_label_set_text(GTK_LABEL(a->status), ("Status: " + text).c_str());
}

static void reset_metrics(App* a) {
    gtk_label_set_text(GTK_LABEL(a->stats),
                       "Recebidos: - | Perdidos: - | Atrasados: - | Duplicados: - | Jitter médio: - ms");
    gtk_label_set_text(GTK_LABEL(a->last_event), "Último evento: nenhum");
}

static void set_connected(App* a, bool on) {
    gtk_widget_set_sensitive(a->start, !on);
    gtk_widget_set_sensitive(a->url, !on);
    gtk_widget_set_sensitive(a->latency, !on);
    gtk_widget_set_sensitive(a->stop, on);
}

static void disconnect(App* a, const std::string& status) {
    a->session++;
    a->rx.stop();
    set_connected(a, false);
    set_status(a, status);
}

static gboolean apply_msg(gpointer data) {
    std::unique_ptr<Msg> m(static_cast<Msg*>(data));
    App* a = m->app;
    if (!*m->alive || m->session != a->session)
        return G_SOURCE_REMOVE;

    switch (m->kind) {
    case Kind::Status:
        if (m->text.rfind("Erro", 0) == 0 || m->text == "EOS")
            disconnect(a, m->text);
        else
            set_status(a, m->text);
        break;
    case Kind::Event:
        gtk_label_set_text(GTK_LABEL(a->last_event), ("Último evento: " + m->text).c_str());
        break;
    case Kind::Stats: {
        char buf[200];
        std::snprintf(buf, sizeof buf,
                      "Recebidos: %llu | Perdidos: %llu | Atrasados: %llu | Duplicados: %llu | Jitter médio: %.2f ms",
                      (unsigned long long)m->stats.num_pushed, (unsigned long long)m->stats.num_lost,
                      (unsigned long long)m->stats.num_late, (unsigned long long)m->stats.num_duplicates,
                      m->stats.avg_jitter_ms);
        gtk_label_set_text(GTK_LABEL(a->stats), buf);
        break;
    }
    }
    return G_SOURCE_REMOVE;
}

static void post(App* a, Kind kind, const std::string& text, const JitterStats& stats = {}) {
    g_idle_add(apply_msg, new Msg{a, a->alive, a->session.load(), kind, text, stats});
}

static void on_start(GtkButton*, gpointer data) {
    auto* a = static_cast<App*>(data);

    ReceiverConfig cfg;
    cfg.url = gtk_editable_get_text(GTK_EDITABLE(a->url));
    cfg.latency_ms = static_cast<uint32_t>(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(a->latency)));
    if (cfg.url.rfind("rtsp://", 0) != 0) {
        set_status(a, "Erro: a URL deve começar com rtsp://");
        return;
    }

    a->session++;
    reset_metrics(a);
    if (a->rx.start(cfg)) {
        set_connected(a, true);
        set_status(a, "Conectando...");
    } else {
        set_status(a, "Erro ao iniciar");
    }
}

static void on_stop(GtkButton*, gpointer data) {
    auto* a = static_cast<App*>(data);
    disconnect(a, "Desconectado");
    reset_metrics(a);
}

static GtkWidget* make_label(const char* text) {
    GtkWidget* l = gtk_label_new(text);
    gtk_label_set_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    return l;
}

static void on_activate(GtkApplication* gapp, gpointer data) {
    auto* a = static_cast<App*>(data);

    GtkWidget* win = gtk_application_window_new(gapp);
    gtk_window_set_title(GTK_WINDOW(win), "EduStream — Aluno");
    gtk_window_set_default_size(GTK_WINDOW(win), 500, 300);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_window_set_child(GTK_WINDOW(win), box);

    a->url = gtk_entry_new();
    gtk_widget_set_hexpand(a->url, TRUE);
    gtk_editable_set_text(GTK_EDITABLE(a->url), DEFAULT_RTSP_URL);

    a->latency = gtk_spin_button_new_with_range(0, 2000, 50);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->latency), DEFAULT_RECEIVER_LATENCY_MS);

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    a->start = gtk_button_new_with_label("Acessar Aula");
    a->stop = gtk_button_new_with_label("Sair da Aula");
    gtk_box_append(GTK_BOX(buttons), a->start);
    gtk_box_append(GTK_BOX(buttons), a->stop);
    g_signal_connect(a->start, "clicked", G_CALLBACK(on_start), a);
    g_signal_connect(a->stop, "clicked", G_CALLBACK(on_stop), a);

    a->status = make_label("");
    a->stats = make_label("");
    a->last_event = make_label("");

    gtk_box_append(GTK_BOX(box), make_label("URL da aula (RTSP):"));
    gtk_box_append(GTK_BOX(box), a->url);
    gtk_box_append(GTK_BOX(box), make_label("Latência do jitterbuffer (ms):"));
    gtk_box_append(GTK_BOX(box), a->latency);
    gtk_box_append(GTK_BOX(box), buttons);
    gtk_box_append(GTK_BOX(box), a->status);
    gtk_box_append(GTK_BOX(box), a->stats);
    gtk_box_append(GTK_BOX(box), a->last_event);

    set_connected(a, false);
    set_status(a, "Desconectado");
    reset_metrics(a);

    a->rx.on_status = [a](const std::string& s) { post(a, Kind::Status, s); };
    a->rx.on_event = [a](const std::string& s) { post(a, Kind::Event, s); };
    a->rx.on_stats = [a](const JitterStats& st) { post(a, Kind::Stats, "", st); };

    gtk_window_present(GTK_WINDOW(win));
}

static void on_shutdown(GApplication*, gpointer data) {
    auto* a = static_cast<App*>(data);
    *a->alive = false;
    a->rx.stop();
}

int main(int argc, char** argv) {
    gst_init(&argc, &argv);

    App app;
    GtkApplication* gapp = gtk_application_new("br.edustream.aluno", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gapp, "activate", G_CALLBACK(on_activate), &app);
    g_signal_connect(gapp, "shutdown", G_CALLBACK(on_shutdown), &app);
    int rc = g_application_run(G_APPLICATION(gapp), argc, argv);
    g_object_unref(gapp);
    return rc;
}
