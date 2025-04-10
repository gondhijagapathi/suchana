#include "notification.h"
#include "wayland.h"
#include "suchana_state.h"
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <systemd/sd-event.h>

extern sd_event *event;

uint32_t next_notification_id = 1;

int method_notify(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *app_name, *app_icon, *summary, *body;
    uint32_t replaces_id;
    int32_t expire_timeout;
    sd_bus_message *actions;
    sd_bus_message *hints;
    int r;

    printf("Received notification request\n");

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

    printf("Notification details:\n");
    printf("  App Name: %s\n", app_name);
    printf("  Replaces ID: %u\n", replaces_id);
    printf("  App Icon: %s\n", app_icon);
    printf("  Summary: %s\n", summary);
    printf("  Body: %s\n", body);

    /* Read actions array */
    r = sd_bus_message_enter_container(m, 'a', "s");
    if (r < 0) {
        fprintf(stderr, "Failed to enter actions array: %s\n", strerror(-r));
        return r;
    }

    // Allocate memory for actions
    struct notification_action *notification_actions = NULL;
    int action_count = 0;
    const char *action_id = NULL;
    const char *action_label = NULL;

    // Read and store actions
    while ((r = sd_bus_message_read(m, "s", &action_id)) > 0) {
        // Read the action label
        r = sd_bus_message_read(m, "s", &action_label);
        if (r < 0) {
            fprintf(stderr, "Failed to read action label: %s\n", strerror(-r));
            free(notification_actions);
            return r;
        }

        // Reallocate memory for the new action
        struct notification_action *temp = realloc(notification_actions, (action_count + 1) * sizeof(struct notification_action));
        if (!temp) {
            fprintf(stderr, "Failed to allocate memory for actions\n");
            free(notification_actions);
            return -ENOMEM;
        }
        notification_actions = temp;

        // Store the action
        notification_actions[action_count].id = strdup(action_id);
        notification_actions[action_count].label = strdup(action_label);
        if (!notification_actions[action_count].id || !notification_actions[action_count].label) {
            fprintf(stderr, "Failed to allocate memory for action strings\n");
            free(notification_actions);
            return -ENOMEM;
        }

        action_count++;
    }

    if (r < 0) {
        fprintf(stderr, "Failed to read action: %s\n", strerror(-r));
        free(notification_actions);
        return r;
    }

    r = sd_bus_message_exit_container(m);
    if (r < 0) {
        fprintf(stderr, "Failed to exit actions array: %s\n", strerror(-r));
        free(notification_actions);
        return r;
    }

    // Add a NULL terminator to the actions array
    if (notification_actions) {
        struct notification_action *temp = realloc(notification_actions, (action_count + 1) * sizeof(struct notification_action));
        if (!temp) {
            fprintf(stderr, "Failed to allocate memory for actions terminator\n");
            free(notification_actions);
            return -ENOMEM;
        }
        notification_actions = temp;
        notification_actions[action_count].id = NULL;
        notification_actions[action_count].label = NULL;
    }

    /* Read hints dictionary */
    r = sd_bus_message_enter_container(m, 'a', "{sv}");
    if (r < 0) {
        fprintf(stderr, "Failed to enter hints dictionary: %s\n", strerror(-r));
        return r;
    }

    while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
        const char *key;
        const char *value;
        char type;

        r = sd_bus_message_read(m, "s", &key);
        if (r < 0) {
            fprintf(stderr, "Failed to read hint key: %s\n", strerror(-r));
            return r;
        }

        r = sd_bus_message_peek_type(m, &type, NULL);
        if (r < 0) {
            fprintf(stderr, "Failed to peek hint type: %s\n", strerror(-r));
            return r;
        }

        if (type == 's') {
            r = sd_bus_message_read(m, "v", "s", &value);
            if (r < 0) {
                fprintf(stderr, "Failed to read hint value: %s\n", strerror(-r));
                return r;
            }
            printf("  Hint: %s = %s\n", key, value);
        } else {
            // Skip non-string hints for now
            r = sd_bus_message_skip(m, "v");
            if (r < 0) {
                fprintf(stderr, "Failed to skip hint: %s\n", strerror(-r));
                return r;
            }
        }

        r = sd_bus_message_exit_container(m);
        if (r < 0) {
            fprintf(stderr, "Failed to exit hint entry: %s\n", strerror(-r));
            return r;
        }
    }

    if (r < 0) {
        fprintf(stderr, "Failed to read hint: %s\n", strerror(-r));
        return r;
    }

    r = sd_bus_message_exit_container(m);
    if (r < 0) {
        fprintf(stderr, "Failed to exit hints dictionary: %s\n", strerror(-r));
        return r;
    }

    /* Read expire timeout */
    r = sd_bus_message_read(m, "i", &expire_timeout);
    if (r < 0) {
        fprintf(stderr, "Failed to parse expire timeout: %s\n", strerror(-r));
        return r;
    }

    /* If replaces_id is not 0, remove the old notification first */
    if (replaces_id != 0) {
        printf("Removing notification with ID: %u\n", replaces_id);
        remove_notification(replaces_id);
    }

    /* Create a timer for the new notification */
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd == -1) {
        perror("timerfd_create");
        return -1;
    }

    struct itimerspec timer = {
        .it_value = {
            .tv_sec = expire_timeout > 0 ? expire_timeout / 1000 : NOTIFICATION_TIMEOUT / 1000,
            .tv_nsec = expire_timeout > 0 ? (expire_timeout % 1000) * 1000000 : (NOTIFICATION_TIMEOUT % 1000) * 1000000
        },
        .it_interval = {0, 0}
    };

    if (timerfd_settime(timer_fd, 0, &timer, NULL) == -1) {
        perror("timerfd_settime");
        close(timer_fd);
        return -1;
    }

    /* Create the new notification */
    printf("Creating new notification\n");
    new_notification((char *)app_name, (char *)app_icon, (char *)summary, (char *)body, 1); // Default urgency to normal

    // Set the actions for the notification
    if (notification_actions) {
        notifications[num_notifications - 1].actions = notification_actions;
    }

    /* Add the timer to the event loop */
    sd_event_source *timer_source;
    r = sd_event_add_io(event, &timer_source, timer_fd, EPOLLIN, timer_callback, (void *)(uintptr_t)next_notification_id - 1);
    if (r < 0) {
        fprintf(stderr, "Failed to add timer to event loop: %s\n", strerror(-r));
        close(timer_fd);
        return r;
    }

    /* Store the timer fd in the notification */
    set_notification_timer(next_notification_id - 1, timer_fd);

    /* Reply with the notification ID */
    return sd_bus_reply_method_return(m, "u", next_notification_id - 1);
}