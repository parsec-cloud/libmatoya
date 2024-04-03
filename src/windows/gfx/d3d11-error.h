#pragma once

#include "matoya.h"

#include "error.h"

static MTY_Error d3d11_map_error(int32_t e)
{
	switch (e) {
		case DXGI_ERROR_DEVICE_REMOVED:
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
			return MTY_ERROR_GFX_DEVICE_REMOVED;

		case DXGI_ERROR_DEVICE_HUNG:
		case DXGI_ERROR_DEVICE_RESET:
			return MTY_ERROR_GFX_ERROR;

		case DXGI_STATUS_OCCLUDED:
			return MTY_ERROR_GFX_INVISIBLE_CONTENT;

		case DXGI_STATUS_UNOCCLUDED:
			return MTY_ERROR_GFX_REVISIBLE_CONTENT;

		default:
			return MTY_ERROR_OK;
	}
}

#define d3d11_push_local_error(e) error_local_push_error(d3d11_map_error(e))
