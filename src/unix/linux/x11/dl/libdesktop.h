#pragma once

#include "libwayland.h"
#include "libx11.h"

struct desktop_info {
    bool wayland;
    union {
        struct xinfo xinfo;
        struct wl_info wl_info;
    };
};