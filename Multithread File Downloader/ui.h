#ifndef UI_H
#define UI_H

#include "downloader.h"

// Launch the GTK GUI
typedef struct {
    char url[512];
    char output[256];
} UserInput;

void run_gui(void);

#endif // UI_H 