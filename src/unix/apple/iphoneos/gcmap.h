// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <math.h>

#include <GameController/GameController.h>

#include "matoya.h"

static void gcmap_set_axis(MTY_ControllerEvent *c, MTY_CAxis axis, uint16_t usage, float value, bool trigger)
{
	c->axes[axis].usage = usage;

	if (trigger) {
		c->axes[axis].value = lrintf(value * UINT8_MAX);
		c->axes[axis].min = 0;
		c->axes[axis].max = UINT8_MAX;

	} else {
		c->axes[axis].value = lrintf(value * INT16_MAX);
		c->axes[axis].min = INT16_MIN;
		c->axes[axis].max = INT16_MAX;
	}
}

// Reported in the XInput layout, which all GameController extended gamepads map onto
static void gcmap_extended_gamepad(GCExtendedGamepad *gp, MTY_ControllerEvent *c)
{
	c->type = MTY_CTYPE_XINPUT;
	c->numButtons = 17;
	c->numAxes = 6;

	c->buttons[MTY_CBUTTON_X] = gp.buttonX.pressed;
	c->buttons[MTY_CBUTTON_A] = gp.buttonA.pressed;
	c->buttons[MTY_CBUTTON_B] = gp.buttonB.pressed;
	c->buttons[MTY_CBUTTON_Y] = gp.buttonY.pressed;
	c->buttons[MTY_CBUTTON_LEFT_SHOULDER] = gp.leftShoulder.pressed;
	c->buttons[MTY_CBUTTON_RIGHT_SHOULDER] = gp.rightShoulder.pressed;
	c->buttons[MTY_CBUTTON_LEFT_TRIGGER] = gp.leftTrigger.value > 0.0f;
	c->buttons[MTY_CBUTTON_RIGHT_TRIGGER] = gp.rightTrigger.value > 0.0f;
	c->buttons[MTY_CBUTTON_BACK] = gp.buttonOptions.pressed;
	c->buttons[MTY_CBUTTON_START] = gp.buttonMenu.pressed;
	c->buttons[MTY_CBUTTON_LEFT_THUMB] = gp.leftThumbstickButton.pressed;
	c->buttons[MTY_CBUTTON_RIGHT_THUMB] = gp.rightThumbstickButton.pressed;
	c->buttons[MTY_CBUTTON_DPAD_UP] = gp.dpad.up.pressed;
	c->buttons[MTY_CBUTTON_DPAD_RIGHT] = gp.dpad.right.pressed;
	c->buttons[MTY_CBUTTON_DPAD_DOWN] = gp.dpad.down.pressed;
	c->buttons[MTY_CBUTTON_DPAD_LEFT] = gp.dpad.left.pressed;

	if (@available(iOS 14.0, tvOS 14.0, macOS 11.0, *))
		c->buttons[MTY_CBUTTON_GUIDE] = gp.buttonHome.pressed;

	gcmap_set_axis(c, MTY_CAXIS_THUMB_LX, 0x30, gp.leftThumbstick.xAxis.value, false);
	gcmap_set_axis(c, MTY_CAXIS_THUMB_LY, 0x31, gp.leftThumbstick.yAxis.value, false);
	gcmap_set_axis(c, MTY_CAXIS_THUMB_RX, 0x32, gp.rightThumbstick.xAxis.value, false);
	gcmap_set_axis(c, MTY_CAXIS_THUMB_RY, 0x35, gp.rightThumbstick.yAxis.value, false);
	gcmap_set_axis(c, MTY_CAXIS_TRIGGER_L, 0x33, gp.leftTrigger.value, true);
	gcmap_set_axis(c, MTY_CAXIS_TRIGGER_R, 0x34, gp.rightTrigger.value, true);
}
