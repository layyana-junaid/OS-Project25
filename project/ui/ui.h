#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>
#include "downloader.h"  // This contains Monitor and startDownload declarations

// Function declarations
void apply_theme(gboolean dark_mode);
void update_progress_ui();
void on_download_clicked(GtkWidget *widget, gpointer data);
void on_cancel_clicked(GtkWidget *widget, gpointer data);
void activate(GtkApplication *app, gpointer user_data);

#endif
