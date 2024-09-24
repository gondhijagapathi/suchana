#ifndef SUCHANA_H
#define SUCHANA_H

#include <stdbool.h>
#include <wayland-client.h>
#include "wlr-layer-shell-v1-protocol.h"
#if defined(HAVE_LIBSYSTEMD)
#include <systemd/sd-bus.h>
#elif defined(HAVE_LIBELOGIND)
#include <elogind/sd-bus.h>
#elif defined(HAVE_BASU)
#include <basu/sd-bus.h>
#endif


struct suchana_state;

struct suchana_surface {
  struct suchana_state *state;
  struct wl_surface *surface;
  struct zwlr_layer_surface_v1 *layer_surface;
  int32_t width, height;
};

struct suchana_state {
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct zwlr_layer_shell_v1 *layer_shell;
  struct wl_list notifications;
  struct wl_list surfaces;
};

#endif
