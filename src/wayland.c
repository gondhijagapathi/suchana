#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cairo/cairo.h>
#include <time.h>
#include "wlr-layer-shell-v1-protocol.h"
#include "wayland.h"

#define WIDTH 200
#define HEIGHT 100

struct wl_display *display;
struct wl_compositor *compositor = NULL;
struct wl_shm *shm = NULL;
struct zwlr_layer_shell_v1 *layer_shell = NULL;

struct notification {
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct wl_buffer *buffer;
    char *app_name;
    char *summary;
};

struct notification *notifications;
int num_notifications = 0;

void create_shm_buffer(int width, int height, char *app_name, char *summary, struct wl_buffer **buffer) {
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

    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2); // Dark gray background
    cairo_paint(cr);

    cairo_set_source_rgb(cr, 1, 1, 1); // White text
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 15);

    cairo_move_to(cr, 20, 40);
    cairo_show_text(cr, app_name);

    cairo_move_to(cr, 20, 80);
    cairo_show_text(cr, summary);

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
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    printf("Layer surface configured: width = %d, height = %d\n", width, height);
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

void create_layer_surface(struct notification *notification) {
    notification->surface = wl_compositor_create_surface(compositor);
    if (!notification->surface) {
        fprintf(stderr, "Failed to create surface\n");
        exit(1);
    }

    notification->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        layer_shell, notification->surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "example");
    if (!notification->layer_surface) {
        fprintf(stderr, "Failed to create layer surface\n");
        exit(1);
    }

    zwlr_layer_surface_v1_add_listener(notification->layer_surface, &layer_surface_listener, NULL);
    zwlr_layer_surface_v1_set_size(notification->layer_surface, WIDTH, HEIGHT);
    zwlr_layer_surface_v1_set_anchor(notification->layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                                        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_margin(notification->layer_surface,
                                     num_notifications * HEIGHT + (num_notifications * 10), // Top margin
                                     10,                          // Right margin
                                     0,                          // Bottom margin
                                     0);    
    //zwlr_layer_surface_v1_set_margin(notification->layer_surface, 10, 10, 0, 0);
    wl_surface_commit(notification->surface);
    printf("Layer surface committed!\n");
}

void new_notification(char *app_name, char *summary) {
    // Allocate memory for a new notification
    struct notification *notification = malloc(sizeof(struct notification));
    if (!notification) {
        fprintf(stderr, "Failed to allocate memory for notification\n");
        exit(1);
    }

    // Copy app_name and summary into the new notification
    notification->app_name = strdup(app_name);
    if (!notification->app_name) {
        fprintf(stderr, "Failed to allocate memory for app_name\n");
        free(notification);
        exit(1);
    }

    notification->summary = strdup(summary);
    if (!notification->summary) {
        fprintf(stderr, "Failed to allocate memory for summary\n");
        free(notification->app_name);
        free(notification);
        exit(1);
    }

    // Create the layer surface and handle display roundtrip
    create_layer_surface(notification);
    wl_display_roundtrip(display);

    // Create shared memory buffer and attach it
    create_shm_buffer(WIDTH, HEIGHT, app_name, summary, &notification->buffer);
    attach_buffer_and_commit(notification->surface, notification->buffer);
    wl_display_roundtrip(display);

    // Reallocate the notifications array to fit the new notification
    struct notification *temp = realloc(notifications, (num_notifications + 1) * sizeof(struct notification));
    if (!temp) {
        fprintf(stderr, "Failed to reallocate memory for notifications\n");
        // Free the memory for the current notification
        free(notification->app_name);
        free(notification->summary);
        free(notification);
        exit(1);
    }

    // Update the notifications array and add the new notification
    notifications = temp;
    notifications[num_notifications] = *notification; // Copy the value, not the pointer
    num_notifications++;

    // Free the dynamically allocated notification (since the struct is copied into the array)
    free(notification);
}



struct wl_display *init_wayland() {
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to display\n");
        exit(1);
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display); // Ensure the registry is fully handled
    if (!compositor || !shm || !layer_shell) {
        fprintf(stderr, "Compositor, shm, or layer shell not available. Exiting.\n");
        exit(1);
    }
    wl_display_roundtrip(display); // Ensure the registry is fully handled

    return display;
}