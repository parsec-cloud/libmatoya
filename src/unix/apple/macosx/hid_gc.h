// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Send rumble to an Xbox controller using GCController framework
// device: IOHIDDeviceRef pointer
// low: Low frequency rumble intensity (0-65535)
// high: High frequency rumble intensity (0-65535)
// Returns true if rumble was sent successfully
bool mty_hid_gc_rumble(void *device, uint16_t low, uint16_t high);

// Clean up rumble context for a device
void mty_hid_gc_cleanup(void *device);

// Check if GCController rumble is available for a device
bool mty_hid_gc_rumble_available(void *device);

#ifdef __cplusplus
}
#endif