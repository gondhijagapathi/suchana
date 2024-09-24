#ifndef SUCHANA_WAYLAND_H
#define SUCHANA_WAYLAND_H

#include <wayland-client.h>

// Function declarations
void new_notification(char *app_name, char *summary);

struct wl_display *init_wayland();

#endif
