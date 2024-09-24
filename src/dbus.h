#ifndef SUCHANA_DBUS_H
#define SUCHANA_DBUS_H

#include <systemd/sd-bus.h>
#include "notification.h"

int init_dbus(sd_bus **bus, sd_bus_slot **slot);
static int method_get_server_information(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

#endif