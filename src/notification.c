#include "notification.h"
#include "wayland.h"

int method_notify(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *app_name, *app_icon, *summary, *body;
    uint32_t replaces_id;
    int32_t expire_timeout;
    sd_bus_message *actions;
    sd_bus_message *hints;
    int r;

    /* Read the basic parameters */
    r = sd_bus_message_read(m, "susss",
                            &app_name,        /* app_name */
                            &replaces_id,     /* replaces_id */
                            &app_icon,        /* app_icon */
                            &summary,         /* summary */
                            &body            /* body */
    );
    
    if (r < 0) {
        fprintf(stderr, "Failed to parse basic parameters: %s\n", strerror(-r));
        return r;
    }

    /* Print the received notification details */
    printf("Notification received:\n");
    printf("  App Name: %s\n", app_name);
    printf("  Replaces ID: %u\n", replaces_id);
    printf("  App Icon: %s\n", app_icon);
    printf("  Summary: %s\n", summary);
    printf("  Body: %s\n", body);
    printf("  Expire Timeout: %d ms\n", expire_timeout);

    new_notification(app_name, summary);

    /* Reply with a dummy response or handle further */
    return sd_bus_reply_method_return(m, "u", replaces_id);
}