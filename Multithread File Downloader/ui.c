#include "ui.h"
#include <gtk/gtk.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

static DownloadProgress progress;
static GtkWidget *progress_bar, *status_label, *download_btn, *url_entry, *output_entry;
static pthread_t download_thread;
static int downloading = 0;
static char output_path[256];

static void *download_thread_func(void *arg) {
    UserInput *input = (UserInput *)arg;
    strncpy(output_path, input->output, sizeof(output_path)-1);
    output_path[sizeof(output_path)-1] = '\0';
    start_download(input->url, output_path, &progress);
    free(input);
    return NULL;
}

gboolean update_progress_ui(gpointer data) {
    pthread_mutex_lock(&progress.mutex);
    double frac = (progress.total > 0) ? ((double)progress.downloaded / progress.total) : 0.0;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), frac);
    
    if (progress.error) {
        gtk_label_set_text(GTK_LABEL(status_label), progress.error_msg);
        gtk_widget_set_sensitive(download_btn, TRUE);
        downloading = 0;
    } else if (progress.done) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Download complete! Saved as: %s (All segments verified)", output_path);
        gtk_label_set_text(GTK_LABEL(status_label), msg);
        gtk_widget_set_sensitive(download_btn, TRUE);
        downloading = 0;
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Downloading... %.1f%% (Verified: %d segments)", 
                frac * 100, progress.segments_verified);
        gtk_label_set_text(GTK_LABEL(status_label), msg);
    }
    pthread_mutex_unlock(&progress.mutex);
    return TRUE;
}

void apply_theme() {
    const char *css =
        "window {"
        "    background-color: #E6E6FA;"
        "    font-family: 'Courier New', monospace;"
        "    font-size: 17pt;"
        "}"
        "button {"
        "    background-image: linear-gradient(to bottom, #663399, #4B0082);"
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
        "    background-image: linear-gradient(to bottom, #4B0082, #2E0854);"
        "}"
        "button:disabled {"
        "    background-image: linear-gradient(to bottom, #9B8BA0, #7B6B7F);"
        "}"
        "entry {"
        "    font-size: 15pt;"
        "    padding: 10px 15px;"
        "    border: 2px solid #663399;"
        "    border-radius: 6px;"
        "    background: white;"
        "    color: #4B0082;"
        "}"
        "label {"
        "    font-size: 15pt;"
        "    color: #4B0082;"
        "}"
        "progressbar {"
        "    min-height: 18px;"
        "    border-radius: 9px;"
        "    background: #D8BFD8;"
        "}"
        "progressbar > trough > progress {"
        "    background-image: linear-gradient(to right, #663399, #9370DB);"
        "    border-radius: 9px;"
        "}";

    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

void on_download_clicked(GtkButton *button, gpointer user_data) {
    if (downloading) return;
    const char *url = gtk_entry_get_text(GTK_ENTRY(url_entry));
    const char *output = gtk_entry_get_text(GTK_ENTRY(output_entry));
    if (strlen(url) == 0 || strlen(output) == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Please enter URL and output file name.");
        return;
    }
    UserInput *input = malloc(sizeof(UserInput));
    strncpy(input->url, url, sizeof(input->url)-1);
    strncpy(input->output, output, sizeof(input->output)-1);
    input->url[sizeof(input->url)-1] = '\0';
    input->output[sizeof(input->output)-1] = '\0';
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    gtk_label_set_text(GTK_LABEL(status_label), "Starting download...");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    downloading = 1;
    pthread_create(&download_thread, NULL, download_thread_func, input);
}

void run_gui(void) {
    gtk_init(NULL, NULL);
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Multithreaded File Downloader");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 200);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *url_label = gtk_label_new("Enter URL:");
    gtk_label_set_xalign(GTK_LABEL(url_label), 0);
    gtk_box_pack_start(GTK_BOX(vbox), url_label, FALSE, FALSE, 0);

    url_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(url_entry), "https://example.com/file.txt");
    gtk_box_pack_start(GTK_BOX(vbox), url_entry, FALSE, FALSE, 0);

    GtkWidget *output_label = gtk_label_new("Save as:");
    gtk_label_set_xalign(GTK_LABEL(output_label), 0);
    gtk_box_pack_start(GTK_BOX(vbox), output_label, FALSE, FALSE, 0);

    output_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(output_entry), "output.txt");
    gtk_box_pack_start(GTK_BOX(vbox), output_entry, FALSE, FALSE, 0);

    download_btn = gtk_button_new_with_label("Download");
    g_signal_connect(download_btn, "clicked", G_CALLBACK(on_download_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), download_btn, FALSE, FALSE, 10);

    progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 0);

    status_label = gtk_label_new("Ready to download.");
    gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 0);

    apply_theme();

    g_timeout_add(200, update_progress_ui, NULL);

    gtk_widget_show_all(window);
    gtk_main();
} 
