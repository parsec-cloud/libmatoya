// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_d3d11_)

#define COBJMACROS
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>

#define DXGI_FATAL(e) ( \
	(e) == DXGI_ERROR_DEVICE_REMOVED || \
	(e) == DXGI_ERROR_DEVICE_HUNG    || \
	(e) == DXGI_ERROR_DEVICE_RESET \
)

#define D3D11_SWFLAGS ( \
	DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | \
	DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING \
)

#define D3D11_CTX_WAIT 2000

struct d3d11_ctx {
	HWND hwnd;
	uint32_t width;
	uint32_t height;
	MTY_Renderer *renderer;
	DXGI_FORMAT format;
	DXGI_FORMAT format_new;
	DXGI_COLOR_SPACE_TYPE colorspace;
	DXGI_COLOR_SPACE_TYPE colorspace_new;
	ID3D11Device *device;
	ID3D11DeviceContext *context;
	ID3D11Texture2D *back_buffer;
	IDXGISwapChain2 *swap_chain2;
	IDXGISwapChain3 *swap_chain3;
	HANDLE waitable;
};

static void d3d11_ctx_get_size(struct d3d11_ctx *ctx, uint32_t *width, uint32_t *height)
{
	RECT rect = {0};
	GetClientRect(ctx->hwnd, &rect);

	*width = rect.right - rect.left;
	*height = rect.bottom - rect.top;
}

static void mty_validate_format_colorspace(struct d3d11_ctx *ctx, MTY_ColorFormat format, MTY_ColorSpace colorspace, DXGI_FORMAT *format_out, DXGI_COLOR_SPACE_TYPE *colorspace_out)
{
	DXGI_FORMAT format_new = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_COLOR_SPACE_TYPE colorspace_new = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

	// Use the last known value if unspecified
	if (format == MTY_COLOR_FORMAT_UNKNOWN) {
		format_new = ctx->format;
	}
	if (colorspace == MTY_COLOR_SPACE_UNKNOWN) {
		colorspace_new = ctx->colorspace;
	}

	switch (format) {
		case MTY_COLOR_FORMAT_RGBA16F:
			format_new = DXGI_FORMAT_R16G16B16A16_FLOAT;
			break;
		case MTY_COLOR_FORMAT_RGB10A2:
			format_new = DXGI_FORMAT_R10G10B10A2_UNORM;
			break;
	}

	switch (colorspace) {
		case MTY_COLOR_SPACE_SRGB:
			colorspace_new = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
			break;
		case MTY_COLOR_SPACE_SCRGB_LINEAR:
			colorspace_new = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
			break;
		case MTY_COLOR_SPACE_HDR10:
			colorspace_new = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
			break;
	}

	// Ensure that the format and colorspace are a valid pairing
	// TODO: An improvement would be to log an error as well instead of only forcing the values
	switch (colorspace_new) {
		case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
			format_new = DXGI_FORMAT_R16G16B16A16_FLOAT;
			break;
		case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
			format_new = DXGI_FORMAT_R10G10B10A2_UNORM;
			break;
		default:
			break;
	}

	*format_out = format_new;
	*colorspace_out = colorspace_new;
}

static void d3d11_ctx_free(struct d3d11_ctx *ctx)
{
	if (ctx->back_buffer)
		ID3D11Texture2D_Release(ctx->back_buffer);

	if (ctx->waitable)
		CloseHandle(ctx->waitable);

	if (ctx->swap_chain2)
		IDXGISwapChain2_Release(ctx->swap_chain2);

	if (ctx->swap_chain3)
		IDXGISwapChain3_Release(ctx->swap_chain3);

	if (ctx->context)
		ID3D11DeviceContext_Release(ctx->context);

	if (ctx->device)
		ID3D11Device_Release(ctx->device);

	ctx->back_buffer = NULL;
	ctx->waitable = NULL;
	ctx->swap_chain2 = NULL;
	ctx->swap_chain3 = NULL;
	ctx->context = NULL;
	ctx->device = NULL;
}

static bool d3d11_ctx_init(struct d3d11_ctx *ctx)
{
	IDXGIDevice2 *device2 = NULL;
	IUnknown *unknown = NULL;
	IDXGIAdapter *adapter = NULL;
	IDXGIFactory2 *factory2 = NULL;
	IDXGISwapChain1 *swap_chain1 = NULL;

	ctx->format = MTY_COLOR_FORMAT_BGRA;
	ctx->colorspace = MTY_COLOR_SPACE_SRGB;

	DXGI_SWAP_CHAIN_DESC1 sd = {0};
	sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // TODO: Sync this with ctx->format initial value
	// sd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // TODO: Need to make this an input parameter
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.SampleDesc.Count = 1;
	sd.BufferCount = 2;
	sd.Flags = D3D11_SWFLAGS;

	D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	HRESULT e = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels,
		sizeof(levels) / sizeof(D3D_FEATURE_LEVEL), D3D11_SDK_VERSION, &ctx->device, NULL, &ctx->context);
	if (e != S_OK) {
		MTY_Log("'D3D11CreateDevice' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D11Device_QueryInterface(ctx->device, &IID_IDXGIDevice2, &device2);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D11Device_QueryInterface(ctx->device, &IID_IUnknown, &unknown);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGIDevice2_GetParent(device2, &IID_IDXGIAdapter, &adapter);
	if (e != S_OK) {
		MTY_Log("'IDXGIDevice2_GetParent' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory2, &factory2);
	if (e != S_OK) {
		MTY_Log("'IDXGIAdapter_GetParent' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGIFactory2_CreateSwapChainForHwnd(factory2, unknown, ctx->hwnd, &sd, NULL, NULL, &swap_chain1);
	if (e != S_OK) {
		MTY_Log("'IDXGIFactory2_CreateSwapChainForHwnd' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain2, &ctx->swap_chain2);
	if (e != S_OK) {
		MTY_Log("'IDXGISwapChain1_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain3, &ctx->swap_chain3);
	if (e != S_OK) {
		MTY_Log("'IDXGISwapChain1_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	ctx->waitable = IDXGISwapChain2_GetFrameLatencyWaitableObject(ctx->swap_chain2);
	if (!ctx->waitable) {
		e = !S_OK;
		MTY_Log("'IDXGISwapChain2_GetFrameLatencyWaitableObject' failed");
		goto except;
	}

	e = IDXGISwapChain2_SetMaximumFrameLatency(ctx->swap_chain2, 1);
	if (e != S_OK) {
		MTY_Log("'IDXGISwapChain2_SetMaximumFrameLatency' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGIFactory2_MakeWindowAssociation(factory2, ctx->hwnd, DXGI_MWA_NO_WINDOW_CHANGES);
	if (e != S_OK) {
		MTY_Log("'IDXGIFactory2_MakeWindowAssociation' failed with HRESULT 0x%X", e);
		goto except;
	}

	// The docs say that the app should wait on this handle even before the first frame
	DWORD we = WaitForSingleObjectEx(ctx->waitable, D3D11_CTX_WAIT, TRUE);
	if (we != WAIT_OBJECT_0)
		MTY_Log("'WaitForSingleObjectEx' failed with error 0x%X", we);

	except:

	if (swap_chain1)
		IDXGISwapChain1_Release(swap_chain1);

	if (factory2)
		IDXGIFactory2_Release(factory2);

	if (adapter)
		IDXGIAdapter_Release(adapter);

	if (unknown)
		IUnknown_Release(unknown);

	if (device2)
		IDXGIDevice2_Release(device2);

	if (e != S_OK)
		d3d11_ctx_free(ctx);

	return e == S_OK;
}

struct gfx_ctx *mty_d3d11_ctx_create(void *native_window, bool vsync)
{
	struct d3d11_ctx *ctx = MTY_Alloc(1, sizeof(struct d3d11_ctx));
	ctx->hwnd = (HWND) native_window;
	ctx->renderer = MTY_RendererCreate();

	d3d11_ctx_get_size(ctx, &ctx->width, &ctx->height);

	if (!d3d11_ctx_init(ctx))
		mty_d3d11_ctx_destroy((struct gfx_ctx **) &ctx);

	return (struct gfx_ctx *) ctx;
}

void mty_d3d11_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct d3d11_ctx *ctx = (struct d3d11_ctx *) *gfx_ctx;

	MTY_RendererDestroy(&ctx->renderer);
	d3d11_ctx_free(ctx);

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

MTY_Device *mty_d3d11_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	return (MTY_Device *) ctx->device;
}

MTY_Context *mty_d3d11_ctx_get_context(struct gfx_ctx *gfx_ctx)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	return (MTY_Context *) ctx->context;
}

static void d3d11_ctx_refresh(struct d3d11_ctx *ctx)
{
	uint32_t width = ctx->width;
	uint32_t height = ctx->height;

	d3d11_ctx_get_size(ctx, &width, &height);

	if (ctx->width != width || ctx->height != height) {
		HRESULT e = IDXGISwapChain2_ResizeBuffers(ctx->swap_chain2, 0, 0, 0,
			DXGI_FORMAT_UNKNOWN, D3D11_SWFLAGS);  // unknown format will resize without changing the existing format

		if (e == S_OK) {
			ctx->width = width;
			ctx->height = height;
		}

		if (DXGI_FATAL(e)) {
			MTY_Log("'IDXGISwapChain2_ResizeBuffers' failed with HRESULT 0x%X", e);
			d3d11_ctx_free(ctx);
			d3d11_ctx_init(ctx);
		}
	}

	DXGI_FORMAT format = ctx->format_new;
	DXGI_COLOR_SPACE_TYPE colorspace = ctx->colorspace_new;

	if (ctx->format != format || ctx->colorspace != colorspace) {
		HRESULT e = IDXGISwapChain2_ResizeBuffers(ctx->swap_chain2, 0, 0, 0,
			format, D3D11_SWFLAGS);
		// TODO: Need to query for display capabilities via CheckColorSpaceSupport before calling SetColorSpace1
		e = IDXGISwapChain3_SetColorSpace1(ctx->swap_chain3, colorspace);

		if (e == S_OK) {
			ctx->format = format;
			ctx->colorspace = colorspace;
		}

		if (DXGI_FATAL(e)) {
			MTY_Log("'IDXGISwapChain2_ResizeBuffers' failed with HRESULT 0x%X", e);
			d3d11_ctx_free(ctx);
			d3d11_ctx_init(ctx);
		}
	}
}

MTY_Surface *mty_d3d11_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	if (!ctx->swap_chain2)
		return (MTY_Surface *) ctx->back_buffer;

	if (!ctx->back_buffer) {
		d3d11_ctx_refresh(ctx);

		HRESULT e = IDXGISwapChain2_GetBuffer(ctx->swap_chain2, 0, &IID_ID3D11Texture2D, &ctx->back_buffer);
		if (e != S_OK)
			MTY_Log("'IDXGISwapChain2_GetBuffer' failed with HRESULT 0x%X", e);
	}

	return (MTY_Surface *) ctx->back_buffer;
}

void mty_d3d11_ctx_present(struct gfx_ctx *gfx_ctx, uint32_t interval)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	if (ctx->back_buffer) {
		UINT flags = interval == 0 ? DXGI_PRESENT_ALLOW_TEARING : 0;

		HRESULT e = IDXGISwapChain2_Present(ctx->swap_chain2, interval, flags);

		ID3D11Texture2D_Release(ctx->back_buffer);
		ctx->back_buffer = NULL;

		if (DXGI_FATAL(e)) {
			MTY_Log("'IDXGISwapChain2_Present' failed with HRESULT 0x%X", e);
			d3d11_ctx_free(ctx);
			d3d11_ctx_init(ctx);

		} else {
			DWORD we = WaitForSingleObjectEx(ctx->waitable, D3D11_CTX_WAIT, TRUE);
			if (we != WAIT_OBJECT_0)
				MTY_Log("'WaitForSingleObjectEx' failed with error 0x%X", we);
		}
	}
}

void mty_d3d11_ctx_draw_quad(struct gfx_ctx *gfx_ctx, const void *image, const MTY_RenderDesc *desc)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_COLOR_SPACE_TYPE colorspace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
	mty_validate_format_colorspace(ctx, desc->format, desc->colorspace, &format, &colorspace);
	ctx->format_new = format;
	ctx->colorspace_new = colorspace;

	mty_d3d11_ctx_get_surface(gfx_ctx);

	if (ctx->back_buffer) {
		MTY_RenderDesc mutated = *desc;
		mutated.viewWidth = ctx->width;
		mutated.viewHeight = ctx->height;

		MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_D3D11, (MTY_Device *) ctx->device,
			(MTY_Context *) ctx->context, image, &mutated, (MTY_Surface *) ctx->back_buffer);
	}
}

void mty_d3d11_ctx_draw_ui(struct gfx_ctx *gfx_ctx, const MTY_DrawData *dd)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	// TODO: Always render the UI in SDR and composite it on top of the quad

	mty_d3d11_ctx_get_surface(gfx_ctx);

	if (ctx->back_buffer)
		MTY_RendererDrawUI(ctx->renderer, MTY_GFX_D3D11, (MTY_Device *) ctx->device,
			(MTY_Context *) ctx->context, dd, (MTY_Surface *) ctx->back_buffer);
}

bool mty_d3d11_ctx_set_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id, const void *rgba,
	uint32_t width, uint32_t height)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	return MTY_RendererSetUITexture(ctx->renderer, MTY_GFX_D3D11, (MTY_Device *) ctx->device,
		(MTY_Context *) ctx->context, id, rgba, width, height);
}

bool mty_d3d11_ctx_has_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	return MTY_RendererHasUITexture(ctx->renderer, id);
}

bool mty_d3d11_ctx_make_current(struct gfx_ctx *gfx_ctx, bool current)
{
	return false;
}
