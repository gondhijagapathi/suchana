#ifndef SUCHANA_DBUS_H
#define SUCHANA_DBUS_H

#include <dbus/dbus.h>

// Function declarations
void handle_notify(DBusMessage *msg);
void handle_get_server_info(DBusMessage *msg, DBusConnection *conn);
void listen_dbus();

#endif
