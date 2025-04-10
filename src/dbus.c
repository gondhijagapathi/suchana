#include "dbus.h"
#include "notification.h"
#include <string.h>

static int method_get_server_information(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    /* Return server information such as name, vendor, version, and spec version */
    const char *name = "CustomNotificationServer";
    const char *vendor = "JagapathiVendor";
    const char *version = "1.0";
    const char *spec_version = "1.2";

    return sd_bus_reply_method_return(m, "ssss", name, vendor, version, spec_version);
}

static int method_get_capabilities(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    /* Return the capabilities of this notification server */
    const char *capabilities[] = {
        "actions",
        "body",
        "icon-static",
        "persistence",
        NULL
    };

    return sd_bus_reply_method_return(m, "as", capabilities);
}

static int method_action_invoked(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    uint32_t id;
    const char *action_key;
    int r;

    r = sd_bus_message_read(m, "us", &id, &action_key);
    if (r < 0) {
        fprintf(stderr, "Failed to parse action invocation: %s\n", strerror(-r));
        return r;
    }

    printf("Action invoked: id=%u, action=%s\n", id, action_key);

    // Emit the ActionInvoked signal
    r = sd_bus_emit_signal(sd_bus_message_get_bus(m),
                          "/org/freedesktop/Notifications",
                          "org.freedesktop.Notifications",
                          "ActionInvoked",
                          "us", id, action_key);
    if (r < 0) {
        fprintf(stderr, "Failed to emit ActionInvoked signal: %s\n", strerror(-r));
        return r;
    }

    return sd_bus_reply_method_return(m, "");
}

/* The vtable of the org.freedesktop.Notifications interface */
static const sd_bus_vtable notifications_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Notify", "susssasa{sv}i", "u", method_notify, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetServerInformation", "", "ssss", method_get_server_information, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetCapabilities", "", "as", method_get_capabilities, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ActionInvoked", "us", "", method_action_invoked, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("ActionInvoked", "us", 0),
    SD_BUS_VTABLE_END
};

int init_dbus(sd_bus **bus, sd_bus_slot **slot) {
    int r;

    /* Connect to the user bus */
    r = sd_bus_open_user(bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to user bus: %s\n", strerror(-r));
        return r;
    }

    /* Install the org.freedesktop.Notifications object */
    r = sd_bus_add_object_vtable(*bus,
                                 slot,
                                 "/org/freedesktop/Notifications",  /* object path */
                                 "org.freedesktop.Notifications",   /* interface name */
                                 notifications_vtable,
                                 NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to add object vtable: %s\n", strerror(-r));
        return r;
    }

    /* Take the well-known name org.freedesktop.Notifications so clients can find us */
    r = sd_bus_request_name(*bus, "org.freedesktop.Notifications", SD_BUS_NAME_ALLOW_REPLACEMENT | SD_BUS_NAME_REPLACE_EXISTING);
    if (r < 0) {
        fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
        return r;
    }

    /* Make the service activatable */
    r = sd_bus_add_object_manager(*bus, NULL, "/org/freedesktop/Notifications");
    if (r < 0) {
        fprintf(stderr, "Failed to add object manager: %s\n", strerror(-r));
        return r;
    }

    return r;
}