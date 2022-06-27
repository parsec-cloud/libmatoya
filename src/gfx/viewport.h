// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <math.h>

static void mty_viewport(const MTY_RenderDesc *desc, float *vp_x, float *vp_y, float *vp_w, float *vp_h, bool transform_origin)
{
	uint32_t w = desc->cropWidth;
	uint32_t h = desc->cropHeight;
	float ar = desc->aspectRatio > 0.0f ? desc->aspectRatio : (float) w / h;

	if (desc->rotation == MTY_ROTATION_90 || desc->rotation == MTY_ROTATION_270) {
		uint32_t tmp = h;
		h = w;
		w = tmp;
		ar = 1.0f / ar;
	}

	uint32_t scaled_w = lrint(desc->scale * w);
	uint32_t scaled_h = lrint(desc->scale * h);

	switch (desc->position) {
		default:
		case MTY_POSITION_AUTO:
			if (scaled_w == 0 || scaled_h == 0 || desc->viewWidth < scaled_w || desc->viewHeight < scaled_h)
				scaled_w = desc->viewWidth;

			*vp_w = (float) scaled_w;
			*vp_h = roundf(*vp_w / ar);

			if (*vp_w > (float) desc->viewWidth) {
				*vp_w = (float) desc->viewWidth;
				*vp_h = roundf(*vp_w / ar);
			}

			if (*vp_h > (float) desc->viewHeight) {
				*vp_h = (float) desc->viewHeight;
				*vp_w = roundf(*vp_h * ar);
			}

			*vp_x = roundf(((float) desc->viewWidth - *vp_w) / 2.0f);
			*vp_y = roundf(((float) desc->viewHeight - *vp_h) / 2.0f);
			break;
		case MTY_POSITION_FIXED:
			*vp_w = (float) scaled_w;
			*vp_h = (float) scaled_h;

			*vp_x = (float) desc->imageX;
			*vp_y = (float) desc->imageY;

			if (transform_origin)
				*vp_y = (float) desc->displayHeight - *vp_h - *vp_y;
			break;
	}
}
