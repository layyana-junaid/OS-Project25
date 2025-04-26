#include <gtk/gtk.h>
#include "../headers/downloader.h"

GtkCssProvider *css_provider;
GtkWidget *url_entry;
GtkWidget *progress_bar;
GtkWidget *status_label;
Monitor monitor;

void apply_theme() {
    const char *css =
        "window {"
        "    background-color: #f3e5ff;"
        "    font-family: 'Courier New', monospace;"
        "    font-size: 17pt;"
        "}"
        "button {"
        "    background-image: linear-gradient(to bottom, #9c64ff, #7b2cbf);"
        "    color: white;"
        "    font-size: 15pt;"
        "    font-weight: bold;"
        "    border-radius: 8px;"
        "    padding: 10px 20px;"
        "    border: none;"
        "    box-shadow: 0 3px 6px rgba(0,0,0,0.16);"
        "    transition: background-image 0.2s ease;"
        "    margin: 5px;"
        "}"
        "button:hover {"
        "    background-image: linear-gradient(to bottom, #8a50e6, #6a1b9a);"
        "}"
        "#download_btn {"
        "    background-image: linear-gradient(to bottom, #ab73ff, #8a3ffb);"
        "}"
        "#cancel_btn {"
        "    background-image: linear-gradient(to bottom, #ab73ff, #8a3ffb);"
        "}"
        "entry {"
        "    font-size: 15pt;"
        "    padding: 10px 15px;"
        "    border: 2px solid #ce93d8;"
        "    border-radius: 6px;"
        "    background: white;"
        "    color: #4a148c;"
        "}"
        "label {"
        "    font-size: 15pt;"
        "    color: #4a148c;"
        "}"
        "progressbar {"
        "    min-height: 18px;"
        "    border-radius: 9px;"
        "    background: #e1bee7;"
        "}"
        "progressbar > trough > progress {"
        "    background-image: linear-gradient(to right, #9c27b0, #ba68c8);"
        "    border-radius: 9px;"
        "}";

    css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

void update_progress_ui() {
    pthread_mutex_lock(&monitor.mutex);
    if (monitor.downloadCount > 0) {
        Download *dl = &monitor.downloads[0];
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), dl->progress / 100.0);
        gchar *status = g_strdup_printf("Downloading: %d%%", dl->progress);
        gtk_label_set_text(GTK_LABEL(status_label), status);
        g_free(status);
    }
    pthread_mutex_unlock(&monitor.mutex);
}

gboolean progress_timeout(gpointer data) {
    update_progress_ui();
    return G_SOURCE_CONTINUE;
}

void on_download_clicked(GtkWidget *widget, gpointer data) {
    const gchar *url = gtk_entry_get_text(GTK_ENTRY(url_entry));
    if (strlen(url) == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Please enter a URL");
        return;
    }

    gtk_label_set_text(GTK_LABEL(status_label), "Starting download...");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);

    // Start download in a new thread
    GThread *download_thread = g_thread_new("download_thread", 
        (GThreadFunc)startDownload, (gpointer)url);
}

void on_cancel_clicked(GtkWidget *widget, gpointer data) {
    pthread_mutex_lock(&monitor.mutex);
    if (monitor.downloadCount > 0) {
        monitor.downloads[0].isPaused = 1;
    }
    pthread_mutex_unlock(&monitor.mutex);
    gtk_label_set_text(GTK_LABEL(status_label), "Download cancelled");
}

void activate(GtkApplication *app, gpointer user_data) {
    // Create main window
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Advanced Downloader");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    gtk_container_set_border_width(GTK_CONTAINER(window), 20);

    // Create main container
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), box);

    // URL entry
    GtkWidget *url_label = gtk_label_new("Download URL:");
    gtk_box_pack_start(GTK_BOX(box), url_label, FALSE, FALSE, 0);

    url_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(url_entry), "https://example.com/file.zip");
    gtk_box_pack_start(GTK_BOX(box), url_entry, FALSE, FALSE, 0);

    // Progress bar
    progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(box), progress_bar, FALSE, FALSE, 10);

    // Status label
    status_label = gtk_label_new("Ready to download");
    gtk_box_pack_start(GTK_BOX(box), status_label, FALSE, FALSE, 0);

    // Button box
    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_CENTER);
    gtk_box_pack_start(GTK_BOX(box), button_box, FALSE, FALSE, 10);

    // Download button
    GtkWidget *download_btn = gtk_button_new_with_label("Download");
    gtk_widget_set_name(download_btn, "download_btn");
    g_signal_connect(download_btn, "clicked", G_CALLBACK(on_download_clicked), NULL);
    gtk_container_add(GTK_CONTAINER(button_box), download_btn);

    // Cancel button
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_set_name(cancel_btn, "cancel_btn");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_clicked), NULL);
    gtk_container_add(GTK_CONTAINER(button_box), cancel_btn);

    // Initialize monitor
    initMonitor(&monitor);

    // Set up progress update timer
    g_timeout_add(500, progress_timeout, NULL);

    // Apply theme
    apply_theme();

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.downloader", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    destroyMonitor(&monitor);
    return status;
}
