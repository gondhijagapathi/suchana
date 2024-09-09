#include <stdio.h>
#include <unistd.h>
#include "dbus.h"
#include "wayland.h"

void handle_notify(DBusMessage *msg) {
    DBusMessageIter args, array_iter, dict_iter;
    const char *app_name = "";
    const char *app_icon = "";
    const char *summary = "";
    const char *body = "";
    int replaces_id = 0;
    int timeout = 0;
    char **actions = NULL;

    // Initialize the message iterator
    dbus_message_iter_init(msg, &args);

    // Extract arguments one by one, checking their types
    if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
        dbus_message_iter_get_basic(&args, &app_name);
        dbus_message_iter_next(&args);
    }

    if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_UINT32) {
        dbus_message_iter_get_basic(&args, &replaces_id);
        dbus_message_iter_next(&args);
    }

    if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
        dbus_message_iter_get_basic(&args, &app_icon);
        dbus_message_iter_next(&args);
    }

    if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
        dbus_message_iter_get_basic(&args, &summary);
        dbus_message_iter_next(&args);
    }

    if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
        dbus_message_iter_get_basic(&args, &body);
        dbus_message_iter_next(&args);
    }

    // Handle actions (array of strings)
    if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_ARRAY) {
        dbus_message_iter_recurse(&args, &array_iter);
        printf("  Actions:\n");
        while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID) {
            if (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_STRING) {
                char *action;
                dbus_message_iter_get_basic(&array_iter, &action);
                printf("    - %s\n", action);
            }
            dbus_message_iter_next(&array_iter);
        }
        dbus_message_iter_next(&args);
    }

    // Handle hints (dictionary of strings to variants)
    if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_ARRAY) {
        dbus_message_iter_recurse(&args, &dict_iter);
        printf("  Hints:\n");
        while (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_INVALID) {
            DBusMessageIter dict_entry;
            dbus_message_iter_recurse(&dict_iter, &dict_entry);

            if (dbus_message_iter_get_arg_type(&dict_entry) == DBUS_TYPE_STRING) {
                char *key;
                dbus_message_iter_get_basic(&dict_entry, &key);
                dbus_message_iter_next(&dict_entry);

                if (dbus_message_iter_get_arg_type(&dict_entry) == DBUS_TYPE_VARIANT) {
                    DBusMessageIter variant_iter;
                    dbus_message_iter_recurse(&dict_entry, &variant_iter);
                    if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_STRING) {
                        char *value;
                        dbus_message_iter_get_basic(&variant_iter, &value);
                        printf("    %s: %s\n", key, value);
                    }
                }
            }
            dbus_message_iter_next(&dict_iter);
        }
        dbus_message_iter_next(&args);
    }

    if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INT32) {
        dbus_message_iter_get_basic(&args, &timeout);
    }

    // Print the notification details
    printf("Received Notification:\n");
    printf("  App Name: %s\n", app_name);
    printf("  Replaces ID: %d\n", replaces_id);
    printf("  App Icon: %s\n", app_icon);
    printf("  Summary: %s\n", summary);
    printf("  Body: %s\n", body);
    new_notification(summary);

    // Print actions (array of strings)
    printf("  Actions:\n");
    if (actions != NULL) {
        for (char **action = actions; *action != NULL; action++) {
            printf("    - %s\n", *action);
        }
    } else {
        printf("    (None)\n");
    }

    printf("  Timeout: %d\n", timeout);
}



// Handle the "GetServerInformation" method
void handle_get_server_info(DBusMessage *msg, DBusConnection *conn) {
    DBusMessage *reply;
    DBusMessageIter args;
    const char *server_name = "suchana";
    const char *vendor = "Jagapathi";
    const char *version = "1.0";
    const char *spec_version = "1.2";

    // Create a reply message
    reply = dbus_message_new_method_return(msg);
    if (!reply) {
        fprintf(stderr, "Out of memory!\n");
        return;
    }

    // Append the server information to the reply
    dbus_message_iter_init_append(reply, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &server_name) ||
        !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &vendor) ||
        !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &version) ||
        !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &spec_version)) {
        fprintf(stderr, "Out of memory while constructing reply!\n");
        dbus_message_unref(reply);
        return;
    }

    // Send the reply
    if (!dbus_connection_send(conn, reply, NULL)) {
        fprintf(stderr, "Out of memory while sending reply!\n");
    }
    dbus_connection_flush(conn);

    // Free the reply
    dbus_message_unref(reply);
}

// Set up a DBus connection and handle messages
void listen_dbus() {
    DBusConnection *conn;
    DBusError err;
    DBusMessage *msg;

    // Initialize the errors
    dbus_error_init(&err);

    // Connect to the session bus
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err);
        return;
    }
    if (!conn) {
        return;
    }

    // Request a well-known name on the bus
    int ret = dbus_bus_request_name(conn, "org.freedesktop.Notifications", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Name Error (%s)\n", err.message);
        dbus_error_free(&err);
        return;
    }
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fprintf(stderr, "Failed to request name on the bus\n");
        return;
    }

    // Main loop to process incoming messages
    while (1) {
        dbus_connection_read_write(conn, 0);
        msg = dbus_connection_pop_message(conn);

        // Check if the message is NULL
        if (msg == NULL) {
            usleep(100000);
            continue;
        }

        // Handle "Notify" method
        if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "Notify")) {
            handle_notify(msg);
        }
        // Handle "GetServerInformation" method
        else if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "GetServerInformation")) {
            handle_get_server_info(msg, conn);
        }

        // Unreference message after processing
        dbus_message_unref(msg);

        // Add a short sleep to avoid busy looping
        usleep(100000);
    }
}