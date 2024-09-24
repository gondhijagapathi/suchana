#include "dbus.h"

static int method_get_server_information(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    /* Return server information such as name, vendor, version, and spec version */
    const char *name = "CustomNotificationServer";
    const char *vendor = "JagapathiVendor";
    const char *version = "1.0";
    const char *spec_version = "1.2";

    return sd_bus_reply_method_return(m, "ssss", name, vendor, version, spec_version);
}

/* The vtable of the org.freedesktop.Notifications interface */
static const sd_bus_vtable notifications_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Notify", "susssasa{sv}i", "u", method_notify, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetServerInformation", "", "ssss", method_get_server_information, SD_BUS_VTABLE_UNPRIVILEGED),
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
    r = sd_bus_request_name(*bus, "org.freedesktop.Notifications", 0);
    if (r < 0) {
        fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
        return r;
    }
    return r;
}