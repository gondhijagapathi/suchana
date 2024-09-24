#ifndef SUCHANA_NOTIFICATION_H
#define SUCHANA_NOTIFICATION_H

#include <stdlib.h>
#include <errno.h>
#include <systemd/sd-bus.h>

int method_notify(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

#endif