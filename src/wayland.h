#ifndef SUCHANA_WAYLAND_H
#define SUCHANA_WAYLAND_H

#include <wayland-client.h>
#include <stdbool.h>
#include "wlr-layer-shell-v1-protocol.h"

#define WIDTH 400
#define HEIGHT 100
#define ICON_SIZE 64
#define PADDING 10
#define NOTIFICATION_SPACING 10
#define MAX_NOTIFICATIONS 5
#define NOTIFICATION_TIMEOUT 5000 // 5 seconds in milliseconds

struct notification_action {
    char *id;
    char *label;
    struct notification_action *next;
};

struct notification_hint {
    char *key;
    char *value;
    struct notification_hint *next;
};

struct notification {
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct wl_buffer *buffer;
    char *app_name;
    char *app_icon;
    char *summary;
    char *body;
    uint32_t id;
    bool active;
    int timer_fd;
    struct notification_action *actions;
    struct notification_hint *hints;
    int urgency; // 0: low, 1: normal, 2: critical
};

// Function declarations
void new_notification(char *app_name, char *app_icon, char *summary, char *body, int urgency);
void remove_notification(uint32_t id);
void update_notification_positions();
void set_notification_timer(uint32_t id, int timer_fd);
int get_notification_timer_fd(uint32_t id);
void add_notification_action(uint32_t id, const char *action_id, const char *action_label);
void add_notification_hint(uint32_t id, const char *key, const char *value);

struct wl_display *init_wayland();

// Global variables
extern struct notification *notifications;
extern int num_notifications;

#endif
