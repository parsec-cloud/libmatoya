// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod.h"
GFX_PROTOTYPES(_d3d11_)

#define COBJMACROS
#include <d3d11.h>

#include "gfx/viewport.h"
#include "gfx/fmt-dxgi.h"
#include "gfx/fmt.h"

static
#include "shaders/ps.h"

static
#include "shaders/vs.h"

#define D3D11_NUM_STAGING 3

struct d3d11_res {
	DXGI_FORMAT format;
	ID3D11Resource *resource;
	ID3D11ShaderResourceView *srv;
	uint32_t width;
	uint32_t height;
	bool was_hardware;
};

struct d3d11 {
	MTY_ColorFormat format;
	struct d3d11_res staging[D3D11_NUM_STAGING];
	struct gfx_uniforms ub;

	ID3D11VertexShader *vs;
	ID3D11PixelShader *ps;
	ID3D11Buffer *vb;
	ID3D11Buffer *ib;
	ID3D11Buffer *psb;
	ID3D11Resource *psbres;
	ID3D11InputLayout *il;
	ID3D11SamplerState *ss_nearest;
	ID3D11SamplerState *ss_linear;
	ID3D11BlendState *bs;
	ID3D11RasterizerState *rs;
	ID3D11DepthStencilState *dss;
};

struct gfx *mty_d3d11_create(MTY_Device *device, uint8_t layer)
{
	struct d3d11 *ctx = MTY_Alloc(1, sizeof(struct d3d11));
	ID3D11Device *_device = (ID3D11Device *) device;

	HRESULT e = ID3D11Device_CreateVertexShader(_device, vs, sizeof(vs), NULL, &ctx->vs);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateVertexShader' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D11Device_CreatePixelShader(_device, ps, sizeof(ps), NULL, &ctx->ps);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreatePixelShader' failed with HRESULT 0x%X", e);
		goto except;
	}

	float vertex_data[] = {
		-1.0f, -1.0f,  // position0 (bottom-left)
		 0.0f,  1.0f,  // texcoord0
		-1.0f,  1.0f,  // position1 (top-left)
		 0.0f,  0.0f,  // texcoord1
		 1.0f,  1.0f,  // position2 (top-right)
		 1.0f,  0.0f,  // texcoord2
		 1.0f, -1.0f,  // position3 (bottom-right)
		 1.0f,  1.0f   // texcoord3
	};

	D3D11_BUFFER_DESC bd = {0};
	bd.ByteWidth = sizeof(vertex_data);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA srd = {0};
	srd.pSysMem = vertex_data;
	e = ID3D11Device_CreateBuffer(_device, &bd, &srd, &ctx->vb);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateBuffer' failed with HRESULT 0x%X", e);
		goto except;
	}

	DWORD index_data[] = {
		0, 1, 2,
		2, 3, 0
	};

	D3D11_BUFFER_DESC ibd = {0};
	ibd.ByteWidth = sizeof(index_data);
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA isrd = {0};
	isrd.pSysMem = index_data;
	e = ID3D11Device_CreateBuffer(_device, &ibd, &isrd, &ctx->ib);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateBuffer' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_BUFFER_DESC psbd = {0};
	psbd.ByteWidth = sizeof(struct gfx_uniforms);
	psbd.Usage = D3D11_USAGE_DYNAMIC;
	psbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	psbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	e = ID3D11Device_CreateBuffer(_device, &psbd, NULL, &ctx->psb);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateBuffer' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D11Buffer_QueryInterface(ctx->psb, &IID_ID3D11Resource, &ctx->psbres);
	if (e != S_OK) {
		MTY_Log("'ID3D11Buffer_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_INPUT_ELEMENT_DESC ied[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 2 * sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	e = ID3D11Device_CreateInputLayout(_device, ied, 2, vs, sizeof(vs), &ctx->il);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateInputLayout' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_SAMPLER_DESC sdesc = {0};
	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sdesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sdesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sdesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	e = ID3D11Device_CreateSamplerState(_device, &sdesc, &ctx->ss_nearest);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateSamplerState' failed with HRESULT 0x%X", e);
		goto except;
	}

	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	e = ID3D11Device_CreateSamplerState(_device, &sdesc, &ctx->ss_linear);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateSamplerState' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_BLEND_DESC bdesc = {
		.AlphaToCoverageEnable = FALSE,
		.RenderTarget[0] = {
			.BlendEnable = TRUE,
			.SrcBlend = D3D11_BLEND_SRC_ALPHA,
			.DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
			.BlendOp = D3D11_BLEND_OP_ADD,
			.SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
			.DestBlendAlpha = D3D11_BLEND_ZERO,
			.BlendOpAlpha = D3D11_BLEND_OP_ADD,
			.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
		},
	};

	e = ID3D11Device_CreateBlendState(_device, &bdesc, &ctx->bs);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateBlendState' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_RASTERIZER_DESC rdesc = {0};
	rdesc.FillMode = D3D11_FILL_SOLID;
	rdesc.CullMode = D3D11_CULL_NONE;
	e = ID3D11Device_CreateRasterizerState(_device, &rdesc, &ctx->rs);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateRasterizerState' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_DEPTH_STENCIL_DESC dssdesc = {0};
	e = ID3D11Device_CreateDepthStencilState(_device, &dssdesc, &ctx->dss);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateDepthStencilState' failed with HRESULT 0x%X", e);
		goto except;
	}

	except:

	if (e != S_OK)
		mty_d3d11_destroy((struct gfx **) &ctx, device);

	return (struct gfx *) ctx;
}

static void d3d11_destroy_resource(struct d3d11_res *res)
{
	if (res->srv)
		ID3D11ShaderResourceView_Release(res->srv);

	if (res->resource)
		ID3D11Resource_Release(res->resource);

	memset(res, 0, sizeof(struct d3d11_res));
}

static bool d3d11_crop_copy(struct d3d11_res *res, MTY_Context *_context, const uint8_t *image, uint32_t full_w,
	uint32_t w, uint32_t h, uint8_t bpp)
{
	ID3D11DeviceContext *context = (ID3D11DeviceContext *) _context;

	D3D11_MAPPED_SUBRESOURCE m = {0};
	HRESULT e = ID3D11DeviceContext_Map(context, res->resource, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
	if (e != S_OK) {
		MTY_Log("'ID3D11DeviceContext_Map' failed with HRESULT 0x%X", e);
		return false;
	}

	for (uint32_t y = 0; y < h; y++)
		memcpy((uint8_t *) m.pData + (y * m.RowPitch), image + (y * full_w * bpp), w * bpp);

	ID3D11DeviceContext_Unmap(context, res->resource, 0);

	return true;
}

static bool d3d11_refresh_resource(struct gfx *gfx, MTY_Device *_device, MTY_Context *context, MTY_ColorFormat fmt,
	uint8_t plane, const uint8_t *image, uint32_t full_w, uint32_t w, uint32_t h, uint8_t bpp)
{
	struct d3d11 *ctx = (struct d3d11 *) gfx;

	bool r = true;

	struct d3d11_res *res = &ctx->staging[plane];
	DXGI_FORMAT format = FMT_PLANES[fmt][plane];

	ID3D11Texture2D *texture = NULL;

	// Resize
	if (!res->resource || w != res->width || h != res->height || format != res->format || res->was_hardware) {
		ID3D11Device *device = (ID3D11Device *) _device;

		d3d11_destroy_resource(res);

		D3D11_TEXTURE2D_DESC desc = {0};
		desc.Width = w;
		desc.Height = h;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT e = ID3D11Device_CreateTexture2D(device, &desc, NULL, &texture);
		if (e != S_OK) {
			MTY_Log("'ID3D11Device_CreateTexture2D' failed with HRESULT 0x%X", e);
			r = false;
			goto except;
		}

		e = ID3D11Texture2D_QueryInterface(texture, &IID_ID3D11Resource, &res->resource);
		if (e != S_OK) {
			MTY_Log("'ID3D11Texture2D_QueryInterface' failed with HRESULT 0x%X", e);
			r = false;
			goto except;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {0};
		srvd.Format = format;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MipLevels = 1;

		e = ID3D11Device_CreateShaderResourceView(device, res->resource, &srvd, &res->srv);
		if (e != S_OK) {
			MTY_Log("'ID3D11Device_CreateShaderResourceView' failed with HRESULT 0x%X", e);
			r = false;
			goto except;
		}

		res->width = w;
		res->height = h;
		res->format = format;
	}

	except:

	if (texture)
		ID3D11Texture2D_Release(texture);

	if (r) {
		// Upload
		r = d3d11_crop_copy(res, context, image, full_w, w, h, bpp);

	} else {
		d3d11_destroy_resource(res);
	}

	return r;
}

static bool d3d11_map_shared_resource(struct gfx *gfx, MTY_Device *_device, MTY_Context *context, MTY_ColorFormat fmt,
	uint8_t plane, const uint8_t *image, uint32_t full_w, uint32_t w, uint32_t h, uint8_t bpp)
{

	// We do all textures at once so we dont want to deal with any later planes.
	if (plane > 0)
		return true;

	struct d3d11 *ctx = (struct d3d11 *) gfx;

	HANDLE *shared_handle = (HANDLE *) image;
	ID3D11Resource *res_d3d = NULL;
	ID3D11Texture2D *texture = NULL;

	for (uint32_t x = 0; x < 3 ; x++) {
		if (!shared_handle[x])
			continue;
		struct d3d11_res *res = &ctx->staging[x];

		ID3D11Device *device = (ID3D11Device *) _device;

		if (res->srv)
			ID3D11ShaderResourceView_Release(res->srv);

		if (res->resource)
			ID3D11Resource_Release(res->resource);

		res->srv = NULL;
		res->resource = NULL;

		HRESULT e = ID3D11Device_OpenSharedResource(device, shared_handle[x], &IID_IDXGIResource, &res_d3d);
		if (e != S_OK) {
			MTY_Log("Failure for index %d shared pointer %p", x, shared_handle[x]);
			MTY_Log("'ID3D11Device_OpenSharedResource' failed with HRESULT 0x%X", e);
			goto except;
		}

		e = IDXGIResource_QueryInterface(res_d3d, &IID_ID3D11Texture2D, &texture);
		if (e != S_OK) {
			MTY_Log("'IDXGIResource_QueryInterface' failed with HRESULT 0x%X", e);
			goto except;
		}

		if (res_d3d) {
			IDXGIResource_Release(res_d3d);
			res_d3d = NULL;
		}

		e = ID3D11Texture2D_QueryInterface(texture, &IID_ID3D11Resource, &res->resource);
		if (e != S_OK) {
			MTY_Log("'ID3D11Texture2D_QueryInterface' failed with HRESULT 0x%X", e);
			goto except;
		}
		D3D11_TEXTURE2D_DESC desc = {0};
		ID3D11Texture2D_GetDesc(texture, &desc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {0};
		srvd.Format = desc.Format;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MipLevels = 1;

		e = ID3D11Device_CreateShaderResourceView(device, res->resource, &srvd, &res->srv);
		if (e != S_OK) {
			MTY_Log("'ID3D11Device_CreateShaderResourceView' failed for index %d with HRESULT 0x%X", x, e);
			goto except;
		}

		bool use_half_width = (desc.Format == DXGI_FORMAT_R16G16_UNORM || desc.Format == DXGI_FORMAT_R8G8_UNORM);
		res->width = use_half_width ? (desc.Width / 2) : desc.Width;
		res->height = desc.Height;
		res->format = desc.Format;
		res->was_hardware = true;

		if (texture) {
			ID3D11Texture2D_Release(texture);
			texture = NULL;
		}
	}

	return true;

	except:

	if (texture)
		ID3D11Texture2D_Release(texture);

	if (res_d3d) {
		IDXGIResource_Release(res_d3d);
		res_d3d = NULL;
	}

	for (uint32_t x = 0; x < 3 ; x++) {
		struct d3d11_res *res = &ctx->staging[x];
		d3d11_destroy_resource(res);
	}

	return false;
}

bool mty_d3d11_valid_hardware_frame(MTY_Device *device, MTY_Context *context, const void *shared_resource)
{
	ID3D11Device *d3d_device = (ID3D11Device *) device;
	HANDLE shared_handle =  NULL;
	memcpy(&shared_handle, shared_resource, sizeof(HANDLE));
	bool r = true;

	IDXGIResource *resource = NULL;

	HRESULT e = ID3D11Device_OpenSharedResource(d3d_device, shared_handle, &IID_IDXGIResource, &resource);
	if (e != S_OK) {
		r = false;
	}

	if (resource)
		IDXGIResource_Release(resource);

	return r;
}

bool mty_d3d11_render(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Surface *dest)
{
	struct d3d11 *ctx = (struct d3d11 *) gfx;
	ID3D11DeviceContext *_context = (ID3D11DeviceContext *) context;
	ID3D11RenderTargetView *_dest = (ID3D11RenderTargetView *) dest;

	// Don't do anything until we have real data
	if (desc->format != MTY_COLOR_FORMAT_UNKNOWN)
		ctx->format = desc->format;

	if (ctx->format == MTY_COLOR_FORMAT_UNKNOWN)
		return true;

	// Refresh textures and load texture data
	// If format == MTY_COLOR_FORMAT_UNKNOWN, texture refreshing/loading is skipped and the previous frame is rendered

	if (desc->hardware) {
		if (!fmt_reload_textures(gfx, device, context, image, desc, d3d11_map_shared_resource))
			return false;
	} else {
		if (!fmt_reload_textures(gfx, device, context, image, desc, d3d11_refresh_resource))
			return false;
	}

	// Viewport
	D3D11_VIEWPORT vp = {0};
	mty_viewport(desc, &vp.TopLeftX, &vp.TopLeftY, &vp.Width, &vp.Height);

	ID3D11DeviceContext_RSSetViewports(_context, 1, &vp);

	// Begin render pass
	ID3D11DeviceContext_OMSetRenderTargets(_context, 1, &_dest, NULL);

	if (desc->layer == 0) {
		FLOAT clear_color[4] = {0, 0, 0, 1};
		ID3D11DeviceContext_ClearRenderTargetView(_context, _dest, clear_color);
		ID3D11DeviceContext_OMSetBlendState(_context, NULL, NULL, 0xFFFFFFFF);

	} else {
		const float blend_factor[4] = {0, 0, 0, 0};
		ID3D11DeviceContext_OMSetBlendState(_context, ctx->bs, blend_factor, 0xFFFFFFFF);
	}

	// Vertex shader
	UINT stride = 4 * sizeof(float);
	UINT offset = 0;

	ID3D11DeviceContext_VSSetShader(_context, ctx->vs, NULL, 0);
	ID3D11DeviceContext_IASetVertexBuffers(_context, 0, 1, &ctx->vb, &stride, &offset);
	ID3D11DeviceContext_IASetIndexBuffer(_context, ctx->ib, DXGI_FORMAT_R32_UINT, 0);
	ID3D11DeviceContext_IASetInputLayout(_context, ctx->il);
	ID3D11DeviceContext_IASetPrimitiveTopology(_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11DeviceContext_OMSetDepthStencilState(_context, ctx->dss, 0);
	ID3D11DeviceContext_RSSetState(_context, ctx->rs);

	// Pixel shader
	ID3D11DeviceContext_PSSetShader(_context, ctx->ps, NULL, 0);
	ID3D11DeviceContext_PSSetSamplers(_context, 0, 1, desc->filter == MTY_FILTER_NEAREST ? &ctx->ss_nearest : &ctx->ss_linear);

	for (uint8_t x = 0; x < D3D11_NUM_STAGING; x++)
		if (ctx->staging[x].srv)
			ID3D11DeviceContext_PSSetShaderResources(_context, x, 1, &ctx->staging[x].srv);

	struct gfx_uniforms cb = {
		.width = (float) desc->cropWidth,
		.height = (float) desc->cropHeight,
		.vp_height = (float) vp.Height,
		.effects[0] = desc->effects[0],
		.effects[1] = desc->effects[1],
		.levels[0] = desc->levels[0],
		.levels[1] = desc->levels[1],
		.planes = FMT_INFO[ctx->format].planes,
		.rotation = desc->rotation,
		.conversion = FMT_CONVERSION(ctx->format, desc->fullRangeYUV, desc->multiplyYUV),
	};

	if (memcmp(&ctx->ub, &cb, sizeof(struct gfx_uniforms))) {
		D3D11_MAPPED_SUBRESOURCE res = {0};
		HRESULT e = ID3D11DeviceContext_Map(_context, ctx->psbres, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
		if (e != S_OK) {
			MTY_Log("'ID3D11DeviceContext_Map' failed with HRESULT 0x%X", e);
			return false;
		}

		memcpy(res.pData, &cb, sizeof(struct gfx_uniforms));

		ID3D11DeviceContext_Unmap(_context, ctx->psbres, 0);

		ctx->ub = cb;
	}

	ID3D11DeviceContext_PSSetConstantBuffers(_context, 0, 1, &ctx->psb);

	// Draw
	ID3D11DeviceContext_DrawIndexed(_context, 6, 0, 0);

	return true;
}

void mty_d3d11_clear(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	uint32_t width, uint32_t height, float r, float g, float b, float a, MTY_Surface *dest)
{
	ID3D11DeviceContext *_context = (ID3D11DeviceContext *) context;
	ID3D11RenderTargetView *_dest = (ID3D11RenderTargetView *) dest;

	FLOAT clear_color[4] = {r, g, b, a};
	ID3D11DeviceContext_ClearRenderTargetView(_context, _dest, clear_color);
}

void mty_d3d11_destroy(struct gfx **gfx, MTY_Device *device)
{
	if (!gfx || !*gfx)
		return;

	struct d3d11 *ctx = (struct d3d11 *) *gfx;

	for (uint8_t x = 0; x < D3D11_NUM_STAGING; x++)
		d3d11_destroy_resource(&ctx->staging[x]);

	if (ctx->rs)
		ID3D11RasterizerState_Release(ctx->rs);

	if (ctx->dss)
		ID3D11DepthStencilState_Release(ctx->dss);

	if (ctx->bs)
		ID3D11BlendState_Release(ctx->bs);

	if (ctx->ss_linear)
		ID3D11SamplerState_Release(ctx->ss_linear);

	if (ctx->ss_nearest)
		ID3D11SamplerState_Release(ctx->ss_nearest);

	if (ctx->il)
		ID3D11InputLayout_Release(ctx->il);

	if (ctx->psbres)
		ID3D11Resource_Release(ctx->psbres);

	if (ctx->psb)
		ID3D11Buffer_Release(ctx->psb);

	if (ctx->ib)
		ID3D11Buffer_Release(ctx->ib);

	if (ctx->vb)
		ID3D11Buffer_Release(ctx->vb);

	if (ctx->ps)
		ID3D11PixelShader_Release(ctx->ps);

	if (ctx->vs)
		ID3D11VertexShader_Release(ctx->vs);

	MTY_Free(ctx);
	*gfx = NULL;
}
