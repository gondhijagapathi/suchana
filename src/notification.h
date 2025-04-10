#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <stdlib.h>
#include <errno.h>
#include <systemd/sd-bus.h>
#include <stdint.h>
#include <systemd/sd-event.h>

#define NOTIFICATION_TIMEOUT 5000 // 5 seconds in milliseconds

int method_notify(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
void new_notification(char *app_name, char *app_icon, char *summary, char *body, int urgency);
void remove_notification(uint32_t id);
void update_notification_positions();
void set_notification_timer(uint32_t id, int timer_fd);
int get_notification_timer_fd(uint32_t id);
void add_notification_action(uint32_t id, const char *action_id, const char *action_label);
void add_notification_hint(uint32_t id, const char *key, const char *value);

extern uint32_t next_notification_id;

// Timer callback declaration
int timer_callback(sd_event_source *source, int fd, uint32_t revents, void *userdata);

#endif