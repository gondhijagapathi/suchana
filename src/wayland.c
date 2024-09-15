#include <wayland-client.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <cairo/cairo.h>
#include "wlr-layer-shell-v1-protocol.h"  // Include the generated header for zwlr_layer_shell_v1
#include "wayland.h"

#define WIDTH 200
#define HEIGHT 100

struct wl_display *display;
struct wl_compositor *compositor = NULL;
struct wl_shm *shm = NULL;
struct zwlr_layer_shell_v1 *layer_shell = NULL;
struct wl_surface *surface;
struct zwlr_layer_surface_v1 *layer_surface;
struct wl_buffer *buffer;
int configured = 0;  // Flag to check if the surface has been configured

void create_shm_buffer(int width, int height, char* body) {
    int stride = width * 4;
    int size = stride * height;
    int fd = shm_open("/test-shm", O_RDWR | O_CREAT | O_EXCL, 0600);
    shm_unlink("/test-shm");

    if (fd < 0) {
        perror("shm_open failed");
        exit(1);
    }

    if (ftruncate(fd, size) < 0) {
        perror("ftruncate failed");
        exit(1);
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        exit(1);
    }

    // Create Cairo surface with the mapped shared memory
    cairo_surface_t *cairo_surface = cairo_image_surface_create_for_data(
        (unsigned char *)data, CAIRO_FORMAT_ARGB32, width, height, stride);
    cairo_t *cr = cairo_create(cairo_surface);

    // Fill the background with a color
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);  // Dark gray background
    cairo_paint(cr);

    // Set text color and font
    cairo_set_source_rgb(cr, 1, 1, 1);  // White text
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 20);

    // Draw the app name, summary, and body
    cairo_move_to(cr, 20, 40);
    cairo_show_text(cr, "Notify");

    cairo_move_to(cr, 20, 80);
    cairo_show_text(cr, body);

    cairo_move_to(cr, 20, 120);
    cairo_show_text(cr, "");

    // Cleanup Cairo objects
    cairo_destroy(cr);
    cairo_surface_destroy(cairo_surface);

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    if (!pool) {
        fprintf(stderr, "Failed to create shm pool\n");
        munmap(data, size);
        close(fd);
        exit(1);
    }

    buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    munmap(data, size);
    close(fd);
}

void attach_buffer_and_commit() {
    if (!configured) {
        fprintf(stderr, "Cannot attach buffer until layer surface is configured\n");
        return;
    }

    if (!buffer) {
        fprintf(stderr, "Buffer is null\n");
        exit(1);
    }

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, WIDTH, HEIGHT);
    wl_surface_commit(surface);
    printf("Buffer attached and surface committed!\n");
}

// Event listener for the zwlr_layer_surface_v1's configure event
static void layer_surface_configure(void *data,
                                    struct zwlr_layer_surface_v1 *surface,
                                    uint32_t serial,
                                    uint32_t width,
                                    uint32_t height) {
    zwlr_layer_surface_v1_ack_configure(surface, serial);

    // Mark as configured
    configured = 1;

    printf("Layer surface configured: width = %d, height = %d\n", width, height);

    // Attach the buffer now that the surface is configured
    attach_buffer_and_commit();
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
};

static void registry_handler(void *data, struct wl_registry *registry,
                             uint32_t id, const char *interface, uint32_t version) {
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
    registry_remover
};

void create_layer_surface() {
    
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to display\n");
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);  // Ensure the registry is fully handled

    if (!compositor || !shm || !layer_shell) {
        fprintf(stderr, "Compositor, shm, or layer shell not available. Exiting.\n");
    }

    surface = wl_compositor_create_surface(compositor);
    if (!surface) {
        fprintf(stderr, "Failed to create surface\n");
        exit(1);
    }

    layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        layer_shell, surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "example");

    if (!layer_surface) {
        fprintf(stderr, "Failed to create layer surface\n");
        exit(1);
    }

    zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, NULL);
    zwlr_layer_surface_v1_set_size(layer_surface, WIDTH, HEIGHT);
    zwlr_layer_surface_v1_set_anchor(layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                                      ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_margin(layer_surface, 10, 10, 0, 0);
    wl_surface_commit(surface);
    printf("Layer surface committed!\n");
}

int new_notification(char* body) {
    create_layer_surface();  // Create the layer surface

    create_shm_buffer(WIDTH, HEIGHT, body);  // Create the buffer with contents

    while (wl_display_dispatch(display) != -1) {
        // Main event loop
    }

    wl_display_disconnect(display);
    return 0;
}
