#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <systemd/sd-event.h>
#include <sys/timerfd.h>
#include <poll.h>
#include "dbus.h"
#include "wayland.h"
#include "suchana_state.h"
#include "notification.h"

sd_event *event = NULL;

// Callback function for processing bus events
static int bus_callback(sd_event_source *source, int fd, uint32_t revents, void *userdata) {
    sd_bus *bus = userdata;
    int r;

    // Process incoming bus requests
    r = sd_bus_process(bus, NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
        return r;
    }

    if (r > 0) {
        // We processed a request, so return immediately to allow processing another one
        return 1;
    }

    return 0;
}

static int wayland_callback(sd_event_source *source, int fd, uint32_t revents, void *userdata) {
    struct wl_display *display = userdata;
    int r;

    // Process incoming Wayland events
    r = wl_display_dispatch_pending(display);
    if (r < 0) {
        fprintf(stderr, "Failed to dispatch Wayland events: %s\n", strerror(-r));
        return r;
    }

    if (r > 0) {
        // We processed an event, so return immediately to allow processing another one
        return 1;
    }

    return 0;
}

int timer_callback(sd_event_source *source, int fd, uint32_t revents, void *userdata) {
    uint32_t notification_id = (uint32_t)(uintptr_t)userdata;
    uint64_t expirations;
    
    printf("Timer expired for notification ID: %u\n", notification_id);
    
    // Read the timer expiration
    if (read(fd, &expirations, sizeof(expirations)) < 0) {
        perror("Failed to read timer expiration");
        return -1;
    }
    
    // Check if the notification still exists before trying to remove it
    int notification_exists = 0;
    for (int i = 0; i < num_notifications; i++) {
        if (notifications[i].id == notification_id && notifications[i].active) {
            notification_exists = 1;
            break;
        }
    }
    
    if (notification_exists) {
        // Remove the notification
        remove_notification(notification_id);
    } else {
        printf("Notification with ID %u already removed\n", notification_id);
    }
    
    // Close the timer fd
    close(fd);
    
    // Unreference the event source
    sd_event_source_unref(source);
    
    return 0;
}

int main(int argc, char *argv[]) {
    sd_bus_slot *slot = NULL;
    sd_bus *bus = NULL;
    sd_event_source *wayland_event = NULL;
    int r;

    // Initialize the D-Bus connection
    r = init_dbus(&bus, &slot);
    if (r < 0) {
        fprintf(stderr, "Failed to init dbus: %s\n", strerror(-r));
        goto finish;
    }

    // Create a new event loop
    r = sd_event_default(&event);
    if (r < 0) {
        fprintf(stderr, "Failed to create event loop: %s\n", strerror(-r));
        goto finish;
    }

    // Attach the bus to the event loop
    r = sd_bus_attach_event(bus, event, 0);
    if (r < 0) {
        fprintf(stderr, "Failed to attach bus to event loop: %s\n", strerror(-r));
        goto finish;
    }

    struct wl_display *display = init_wayland();

    r = sd_event_add_io(event, &wayland_event, wl_display_get_fd(display), EPOLLIN, wayland_callback, display);
    if (r < 0) {
        fprintf(stderr, "Failed to add Wayland display to event loop: %s\n", strerror(-r));
        goto finish;
    }

    // Run the event loop
    r = sd_event_loop(event);
    if (r < 0) {
        fprintf(stderr, "Failed to run event loop: %s\n", strerror(-r));
        goto finish;
    }

finish:
    sd_event_unref(event);
    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
