#include <memory>
#include <string>

#include <gtk/gtk.h>

#include "transmitter.hpp"

struct App {
    Transmitter tx;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    GtkWidget *url, *source, *start, *stop, *status;
};

struct StatusMsg {
    App* app;
    std::shared_ptr<bool> alive;
    std::string text;
};

static void set_status(App* a, const std::string& text) {
    gtk_label_set_text(GTK_LABEL(a->status), ("Status: " + text).c_str());
}

static void set_transmitting(App* a, bool on) {
    gtk_widget_set_sensitive(a->start, !on);
    gtk_widget_set_sensitive(a->url, !on);
    gtk_widget_set_sensitive(a->source, !on);
    gtk_widget_set_sensitive(a->stop, on);
}

static gboolean apply_status(gpointer data) {
    std::unique_ptr<StatusMsg> m(static_cast<StatusMsg*>(data));
    if (!*m->alive || !m->app->tx.is_running())
        return G_SOURCE_REMOVE;

    set_status(m->app, m->text);
    if (m->text.rfind("Erro", 0) == 0 || m->text == "EOS") {
        m->app->tx.stop();
        set_transmitting(m->app, false);
    }
    return G_SOURCE_REMOVE;
}

static void on_start(GtkButton*, gpointer data) {
    auto* a = static_cast<App*>(data);

    TransmitterConfig cfg;
    cfg.url = gtk_editable_get_text(GTK_EDITABLE(a->url));
    if (cfg.url.rfind("srt://", 0) != 0) {
        set_status(a, "Erro: a URL deve começar com srt://");
        return;
    }
    cfg.source = gtk_drop_down_get_selected(GTK_DROP_DOWN(a->source)) == 0 ? MediaSource::Camera
                                                                            : MediaSource::Test;
    if (a->tx.start(cfg)) {
        set_transmitting(a, true);
        set_status(a, "Iniciando...");
    } else {
        set_status(a, "Erro ao iniciar");
    }
}

static void on_stop(GtkButton*, gpointer data) {
    auto* a = static_cast<App*>(data);
    a->tx.stop();
    set_transmitting(a, false);
    set_status(a, "Parado");
}

static void on_activate(GtkApplication* gapp, gpointer data) {
    auto* a = static_cast<App*>(data);

    GtkWidget* win = gtk_application_window_new(gapp);
    gtk_window_set_title(GTK_WINDOW(win), "EduStream — Professor");
    gtk_window_set_default_size(GTK_WINDOW(win), 480, 220);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_window_set_child(GTK_WINDOW(win), box);

    a->url = gtk_entry_new();
    gtk_widget_set_hexpand(a->url, TRUE);
    gtk_editable_set_text(GTK_EDITABLE(a->url), DEFAULT_SRT_URL);

    const char* const sources[] = {"Webcam + Microfone", "Teste (Barras + 440 Hz)", nullptr};
    a->source = gtk_drop_down_new_from_strings(sources);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(a->source), 1);

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    a->start = gtk_button_new_with_label("Iniciar Aula");
    a->stop = gtk_button_new_with_label("Encerrar Aula");
    gtk_box_append(GTK_BOX(buttons), a->start);
    gtk_box_append(GTK_BOX(buttons), a->stop);
    g_signal_connect(a->start, "clicked", G_CALLBACK(on_start), a);
    g_signal_connect(a->stop, "clicked", G_CALLBACK(on_stop), a);

    a->status = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(a->status), TRUE);
    gtk_label_set_xalign(GTK_LABEL(a->status), 0);

    GtkWidget* url_label = gtk_label_new("URL de publicação (SRT):");
    GtkWidget* source_label = gtk_label_new("Fonte de mídia:");
    gtk_label_set_xalign(GTK_LABEL(url_label), 0);
    gtk_label_set_xalign(GTK_LABEL(source_label), 0);

    gtk_box_append(GTK_BOX(box), url_label);
    gtk_box_append(GTK_BOX(box), a->url);
    gtk_box_append(GTK_BOX(box), source_label);
    gtk_box_append(GTK_BOX(box), a->source);
    gtk_box_append(GTK_BOX(box), buttons);
    gtk_box_append(GTK_BOX(box), a->status);

    set_transmitting(a, false);
    set_status(a, "Parado");

    a->tx.on_status = [a, alive = a->alive](const std::string& s) {
        g_idle_add(apply_status, new StatusMsg{a, alive, s});
    };

    gtk_window_present(GTK_WINDOW(win));
}

static void on_shutdown(GApplication*, gpointer data) {
    auto* a = static_cast<App*>(data);
    *a->alive = false;
    a->tx.stop();
}

int main(int argc, char** argv) {
    gst_init(&argc, &argv);

    App app;
    GtkApplication* gapp = gtk_application_new("br.edustream.professor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gapp, "activate", G_CALLBACK(on_activate), &app);
    g_signal_connect(gapp, "shutdown", G_CALLBACK(on_shutdown), &app);
    int rc = g_application_run(G_APPLICATION(gapp), argc, argv);
    g_object_unref(gapp);
    return rc;
}
