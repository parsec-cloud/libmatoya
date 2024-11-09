// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

struct xboxw_state {
	bool rumble;
	uint16_t low;
	uint16_t high;
};

// Rumble

static void xboxw_rumble(struct hid_dev *device, uint16_t low, uint16_t high)
{
    struct xbox_state *ctx = mty_hid_device_get_state(device);

		printf("Got here 1");
		uint8_t rumble_packet[12] = {0x00, 0x01, 0x0F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        rumble_packet[5] = ctx->low >> 8;
        rumble_packet[6] = ctx->high >> 8;
		printf("Got here 2");

        mty_hid_device_write(device, rumble_packet, sizeof(rumble_packet));
		printf("Got here 3");

}

static void xboxw_do_rumble(struct hid_dev *device)
{
    struct xbox_state *ctx = mty_hid_device_get_state(device);

    if (ctx->rumble) {
        uint8_t rumble_packet[12] = {0x00, 0x01, 0x0F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        rumble_packet[5] = ctx->low >> 8;
        rumble_packet[6] = ctx->high >> 8;
		printf("Got here 2");

        mty_hid_device_write(device, rumble_packet, sizeof(rumble_packet));
		printf("Got here 3");
        ctx->rumble = false;
    }
}

static void xboxw_init(struct hid_dev *device)
{
	printf("Got to xboxw_init");
	struct xboxw_state *ctx = mty_hid_device_get_state(device);
	ctx->rumble = true;
}

static bool xboxw_state(struct hid_dev *device, const void *data, size_t size, MTY_ControllerEvent *c)
{
	const uint8_t *d = data;

	c->vid = mty_hid_device_get_vid(device);
	c->pid = mty_hid_device_get_pid(device);
	c->numAxes = 6;
	c->numButtons = 17;
	c->type = MTY_CTYPE_XBOXW;
	c->id = mty_hid_device_get_id(device);

	c->buttons[MTY_CBUTTON_X] = d[3] & 0x40;
	c->buttons[MTY_CBUTTON_A] = d[3] & 0x10;
	c->buttons[MTY_CBUTTON_B] = d[3] & 0x20;
	c->buttons[MTY_CBUTTON_Y] = d[3] & 0x80;
	c->buttons[MTY_CBUTTON_LEFT_SHOULDER] = d[3] & 0x01;
	c->buttons[MTY_CBUTTON_RIGHT_SHOULDER] = d[3] & 0x02;
	c->buttons[MTY_CBUTTON_BACK] = d[2] & 0x20;
	c->buttons[MTY_CBUTTON_START] = d[2] & 0x10;
	c->buttons[MTY_CBUTTON_LEFT_THUMB] = d[2] & 0x40;
	c->buttons[MTY_CBUTTON_RIGHT_THUMB] = d[2] & 0x80;
	c->buttons[MTY_CBUTTON_GUIDE] = d[3] & 0x04;
	c->buttons[MTY_CBUTTON_DPAD_UP] = d[2] & 0x1;
	c->buttons[MTY_CBUTTON_DPAD_RIGHT] = d[2] & 0x8;
	c->buttons[MTY_CBUTTON_DPAD_DOWN] = d[2] & 0x2;
	c->buttons[MTY_CBUTTON_DPAD_LEFT] = d[2] & 0x4;

	c->axes[MTY_CAXIS_THUMB_LX].value = *((int16_t *) (d + 6));
	c->axes[MTY_CAXIS_THUMB_LX].usage = 0x30;
	c->axes[MTY_CAXIS_THUMB_LX].min = INT16_MIN;
	c->axes[MTY_CAXIS_THUMB_LX].max = INT16_MAX;

	int16_t ly = *((int16_t *) (d + 8));
	c->axes[MTY_CAXIS_THUMB_LY].value = (int16_t) ((int32_t) (ly + 1) * -1);
	c->axes[MTY_CAXIS_THUMB_LY].usage = 0x31;
	c->axes[MTY_CAXIS_THUMB_LY].min = INT16_MIN;
	c->axes[MTY_CAXIS_THUMB_LY].max = INT16_MAX;

	c->axes[MTY_CAXIS_THUMB_RX].value = *((int16_t *) (d + 10));
	c->axes[MTY_CAXIS_THUMB_RX].usage = 0x32;
	c->axes[MTY_CAXIS_THUMB_RX].min = INT16_MIN;
	c->axes[MTY_CAXIS_THUMB_RX].max = INT16_MAX;

	int16_t ry = *((int16_t *) (d + 12));
	c->axes[MTY_CAXIS_THUMB_RY].value = (int16_t) ((int32_t) (ry + 1) * -1);
	c->axes[MTY_CAXIS_THUMB_RY].usage = 0x35;
	c->axes[MTY_CAXIS_THUMB_RY].min = INT16_MIN;
	c->axes[MTY_CAXIS_THUMB_RY].max = INT16_MAX;

	c->axes[MTY_CAXIS_TRIGGER_L].value = d[4];
	c->axes[MTY_CAXIS_TRIGGER_L].usage = 0x33;
	c->axes[MTY_CAXIS_TRIGGER_L].min = 0;
	c->axes[MTY_CAXIS_TRIGGER_L].max = UINT8_MAX;

	c->axes[MTY_CAXIS_TRIGGER_R].value = d[5];
	c->axes[MTY_CAXIS_TRIGGER_R].usage = 0x34;
	c->axes[MTY_CAXIS_TRIGGER_R].min = 0;
	c->axes[MTY_CAXIS_TRIGGER_R].max = UINT8_MAX;

	c->buttons[MTY_CBUTTON_LEFT_TRIGGER] = c->axes[MTY_CAXIS_TRIGGER_L].value > 0;
	c->buttons[MTY_CBUTTON_RIGHT_TRIGGER] = c->axes[MTY_CAXIS_TRIGGER_R].value > 0;
	xboxw_do_rumble(device);

	return true;
}
