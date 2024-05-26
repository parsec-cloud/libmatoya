#pragma once 

#include <wayland-client.h>
#include "xdg-shell.h"

struct wl_info {
	struct wl_display *display;
	struct wl_surface *surface;
};