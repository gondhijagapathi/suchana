#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <time.h>
#include <sys/timerfd.h>
#include <poll.h>
#include "wlr-layer-shell-v1-protocol.h"
#include "wayland.h"
#include "notification.h"

struct wl_display *display;
struct wl_compositor *compositor = NULL;
struct wl_shm *shm = NULL;
struct zwlr_layer_shell_v1 *layer_shell = NULL;

// Global variables
struct notification *notifications = NULL;
int num_notifications = 0;

cairo_surface_t *load_icon(const char *icon_path) {
    if (!icon_path) {
        fprintf(stderr, "Icon path is NULL\n");
        return NULL;
    }

    cairo_surface_t *surface = cairo_image_surface_create_from_png(icon_path);
    cairo_status_t status = cairo_surface_status(surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to load icon: %s, error: %s\n", icon_path, cairo_status_to_string(status));
        cairo_surface_destroy(surface);
        return NULL;
    }

    return surface;
}

void create_shm_buffer(int width, int height, struct notification *notification, struct wl_buffer **buffer) {
    int stride = width * 4;
    int size = stride * height;

    // Create a unique shm file name
    char shm_file[32];
    snprintf(shm_file, sizeof(shm_file), "/suchana-shm-%d", (long) time(NULL));

    int fd = shm_open(shm_file, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        perror("shm_open failed");
        exit(1);
    }

    if (ftruncate(fd, size) < 0) {
        perror("ftruncate failed");
        close(fd);
        shm_unlink(shm_file);
        exit(1);
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        shm_unlink(shm_file);
        exit(1);
    }

    cairo_surface_t *cairo_surface = cairo_image_surface_create_for_data(
        (unsigned char *)data, CAIRO_FORMAT_ARGB32, width, height, stride);
    if (cairo_surface_status(cairo_surface) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to create Cairo surface\n");
        munmap(data, size);
        close(fd);
        shm_unlink(shm_file);
        exit(1);
    }

    cairo_t *cr = cairo_create(cairo_surface);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to create Cairo context\n");
        cairo_surface_destroy(cairo_surface);
        munmap(data, size);
        close(fd);
        shm_unlink(shm_file);
        exit(1);
    }

    // Set background color based on urgency
    switch (notification->urgency) {
        case 0: // Low
            cairo_set_source_rgb(cr, 0.2, 0.2, 0.2); // Dark gray
            break;
        case 1: // Normal
            cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); // Darker gray
            break;
        case 2: // Critical
            cairo_set_source_rgb(cr, 0.3, 0.1, 0.1); // Dark red
            break;
    }
    cairo_paint(cr);

    // Draw icon if available
    if (notification->app_icon) {
        cairo_surface_t *icon_surface = load_icon(notification->app_icon);
        if (icon_surface) {
            // Calculate icon position and size
            double scale = (double)ICON_SIZE / cairo_image_surface_get_width(icon_surface);
            cairo_save(cr);
            cairo_translate(cr, PADDING, PADDING);
            cairo_scale(cr, scale, scale);
            cairo_set_source_surface(cr, icon_surface, 0, 0);
            cairo_paint(cr);
            cairo_restore(cr);
            cairo_surface_destroy(icon_surface);
        } else {
            // Draw placeholder if icon loading failed
            cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
            cairo_rectangle(cr, PADDING, PADDING, ICON_SIZE, ICON_SIZE);
            cairo_fill(cr);
        }
    }

    // Set text color
    cairo_set_source_rgb(cr, 1, 1, 1); // White text

    // Draw app name
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14); // Reverted font size
    cairo_move_to(cr, PADDING + (notification->app_icon ? ICON_SIZE + PADDING : 0), PADDING + 20); // Reverted position
    cairo_show_text(cr, notification->app_name);

    // Draw summary
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12); // Reverted font size
    cairo_move_to(cr, PADDING + (notification->app_icon ? ICON_SIZE + PADDING : 0), PADDING + 40); // Reverted position
    cairo_show_text(cr, notification->summary);

    // Draw body if available
    if (notification->body) {
        cairo_move_to(cr, PADDING + (notification->app_icon ? ICON_SIZE + PADDING : 0), PADDING + 60); // Reverted position
        cairo_show_text(cr, notification->body);
    }

    // Draw action buttons if available
    if (notification->actions) {
        int button_height = 30;
        int button_padding = 10; // Increased padding
        int button_spacing = 15; // Increased spacing
        int button_y = height - button_height - PADDING;
        int button_x = PADDING + (notification->app_icon ? ICON_SIZE + PADDING : 0);
        
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12);
        
        for (int i = 0; notification->actions[i].id != NULL; i++) {
            const char *action_id = notification->actions[i].id;
            const char *action_label = notification->actions[i].label;
            
            if (!action_label) break;
            
            // Calculate button width based on text
            cairo_text_extents_t extents;
            cairo_text_extents(cr, action_label, &extents);
            int button_width = extents.width + 30; // Increased padding
            
            // Draw button background
            cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
            cairo_rectangle(cr, button_x, button_y, button_width, button_height);
            cairo_fill(cr);
            
            // Draw button border
            cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
            cairo_rectangle(cr, button_x, button_y, button_width, button_height);
            cairo_stroke(cr);
            
            // Draw button text
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_move_to(cr, button_x + 15, button_y + button_height/2 + 4); // Adjusted text position
            cairo_show_text(cr, action_label);
            
            button_x += button_width + button_spacing;
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(cairo_surface);

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    if (!pool) {
        fprintf(stderr, "Failed to create shm pool\n");
        munmap(data, size);
        close(fd);
        shm_unlink(shm_file);
        exit(1);
    }

    *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    munmap(data, size);
    close(fd);
    shm_unlink(shm_file);
}

void attach_buffer_and_commit(struct wl_surface *surface, struct wl_buffer *buffer) {
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, WIDTH, HEIGHT);
    wl_surface_commit(surface);
    printf("Buffer attached and surface committed!\n");
}

static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height) {
    printf("Layer surface configured: width = %d, height = %d\n", width, height);
    zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
};

static void registry_handler(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        layer_shell = wl_registry_bind(registry, id, &zwlr_layer_shell_v1_interface, 1);
    }
}

static void registry_remover(void *data, struct wl_registry *registry, uint32_t id) {
    // No-op for now
}

static const struct wl_registry_listener registry_listener = {
    registry_handler,
    registry_remover,
};

void update_notification_positions() {
    for (int i = 0; i < num_notifications; i++) {
        if (notifications[i].active) {
            int top_margin = i * (HEIGHT + NOTIFICATION_SPACING);
            zwlr_layer_surface_v1_set_margin(notifications[i].layer_surface,
                                           top_margin,  // Top margin
                                           10,         // Right margin
                                           0,          // Bottom margin
                                           0);         // Left margin
            // Update exclusive zone for each notification
            zwlr_layer_surface_v1_set_exclusive_zone(notifications[i].layer_surface, HEIGHT);
            
            // Commit the surface to apply changes
            wl_surface_commit(notifications[i].surface);
            
            // Force a roundtrip to ensure changes are applied
            wl_display_roundtrip(display);
        }
    }
}

void remove_notification(uint32_t id) {
    printf("Removing notification with ID: %u\n", id);
    
    // Find the notification to remove
    int index = -1;
    for (int i = 0; i < num_notifications; i++) {
        if (notifications[i].id == id && notifications[i].active) {
            index = i;
            break;
        }
    }
    
    if (index == -1) {
        printf("Notification with ID %u not found or already removed\n", id);
        return;
    }
    
    // Clean up Wayland resources
    if (notifications[index].buffer) {
        wl_buffer_destroy(notifications[index].buffer);
    }
    if (notifications[index].layer_surface) {
        zwlr_layer_surface_v1_destroy(notifications[index].layer_surface);
    }
    if (notifications[index].surface) {
        wl_surface_destroy(notifications[index].surface);
    }
    
    // Close timer fd
    if (notifications[index].timer_fd != -1) {
        close(notifications[index].timer_fd);
    }
    
    // Free allocated strings
    free(notifications[index].app_name);
    free(notifications[index].summary);
    
    // Mark as inactive
    notifications[index].active = false;
    
    // Remove the notification from the array
    if (index < num_notifications - 1) {
        memmove(&notifications[index], &notifications[index + 1],
                (num_notifications - index - 1) * sizeof(struct notification));
    }
    num_notifications--;
    
    // Update positions of remaining notifications
    update_notification_positions();
    
    // Force a roundtrip to ensure changes are applied
    wl_display_roundtrip(display);
}

void create_layer_surface(struct notification *notification) {
    printf("Creating layer surface\n");
    notification->surface = wl_compositor_create_surface(compositor);
    if (!notification->surface) {
        fprintf(stderr, "Failed to create surface\n");
        exit(1);
    }
    printf("Surface created successfully\n");

    notification->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        layer_shell, notification->surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "notification");
    if (!notification->layer_surface) {
        fprintf(stderr, "Failed to create layer surface\n");
        exit(1);
    }
    printf("Layer surface created successfully\n");

    zwlr_layer_surface_v1_add_listener(notification->layer_surface, &layer_surface_listener, NULL);
    zwlr_layer_surface_v1_set_size(notification->layer_surface, WIDTH, HEIGHT);
    zwlr_layer_surface_v1_set_anchor(notification->layer_surface, 
                                    ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                    ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    
    // Set exclusive zone to make the surface visible
    zwlr_layer_surface_v1_set_exclusive_zone(notification->layer_surface, HEIGHT);
    
    // Set keyboard interactivity
    zwlr_layer_surface_v1_set_keyboard_interactivity(notification->layer_surface, 0);
    
    // Set layer surface properties
    zwlr_layer_surface_v1_set_margin(notification->layer_surface, 0, 10, 0, 0);
    
    // Commit the surface
    wl_surface_commit(notification->surface);
    printf("Layer surface committed\n");
    
    // Force a roundtrip to ensure the surface is configured
    wl_display_roundtrip(display);
    printf("Display roundtrip completed\n");
}

void new_notification(char *app_name, char *app_icon, char *summary, char *body, int urgency) {
    printf("Creating new notification with ID: %u\n", next_notification_id);
    
    // If we have too many notifications, remove the oldest one
    if (num_notifications >= MAX_NOTIFICATIONS) {
        printf("Maximum notifications reached, removing oldest\n");
        remove_notification(notifications[0].id);
    }

    // Allocate memory for a new notification
    struct notification *notification = malloc(sizeof(struct notification));
    if (!notification) {
        fprintf(stderr, "Failed to allocate memory for notification\n");
        exit(1);
    }

    // Initialize the notification
    notification->id = next_notification_id++;
    notification->active = true;
    notification->app_name = strdup(app_name);
    notification->app_icon = app_icon ? strdup(app_icon) : NULL;
    notification->summary = strdup(summary);
    notification->body = body ? strdup(body) : NULL;
    notification->urgency = urgency;
    notification->timer_fd = -1;
    notification->actions = NULL;  // Will be set by the caller
    notification->hints = NULL;

    if (!notification->app_name || !notification->summary || 
        (app_icon && !notification->app_icon) || 
        (body && !notification->body)) {
        fprintf(stderr, "Failed to allocate memory for notification strings\n");
        free(notification->app_name);
        free(notification->app_icon);
        free(notification->summary);
        free(notification->body);
        free(notification);
        exit(1);
    }

    // Create the layer surface
    create_layer_surface(notification);

    // Create shared memory buffer and attach it
    create_shm_buffer(WIDTH, HEIGHT, notification, &notification->buffer);
    attach_buffer_and_commit(notification->surface, notification->buffer);

    // Add to notifications array
    struct notification *temp = realloc(notifications, (num_notifications + 1) * sizeof(struct notification));
    if (!temp) {
        fprintf(stderr, "Failed to reallocate memory for notifications\n");
        free(notification->app_name);
        free(notification->app_icon);
        free(notification->summary);
        free(notification->body);
        free(notification);
        exit(1);
    }

    notifications = temp;
    notifications[num_notifications] = *notification;
    num_notifications++;

    // Update positions of all notifications
    update_notification_positions();

    // Free the temporary notification
    free(notification);
    
    printf("Notification created successfully with ID: %u\n", notifications[num_notifications - 1].id);
}

int get_notification_timer_fd(uint32_t id) {
    for (int i = 0; i < num_notifications; i++) {
        if (notifications[i].id == id && notifications[i].active) {
            return notifications[i].timer_fd;
        }
    }
    return -1;
}

void set_notification_timer(uint32_t id, int timer_fd) {
    for (int i = 0; i < num_notifications; i++) {
        if (notifications[i].id == id && notifications[i].active) {
            notifications[i].timer_fd = timer_fd;
            break;
        }
    }
}

struct wl_display *init_wayland() {
    printf("Initializing Wayland connection\n");
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to display\n");
        exit(1);
    }
    printf("Connected to Wayland display\n");

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display); // Ensure the registry is fully handled
    if (!compositor || !shm || !layer_shell) {
        fprintf(stderr, "Compositor, shm, or layer shell not available. Exiting.\n");
        exit(1);
    }
    printf("Wayland initialization complete\n");
    wl_display_roundtrip(display); // Ensure the registry is fully handled

    return display;
}