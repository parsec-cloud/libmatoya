// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod.h"
GFX_PROTOTYPES(_metal_)

#include <Metal/Metal.h>

#include "gfx/viewport.h"
#include "gfx/fmt-metal.h"
#include "gfx/fmt.h"

#include "shaders/metal/quad.h"

#define METAL_NUM_STAGING 3

struct metal_cb {
	float width;
	float height;
	float vp_height;
	float pad0;
	uint32_t effects[4];
	float levels[4];
	uint32_t planes;
	uint32_t rotation;
	uint32_t conversion;
	uint32_t pad1;
};

struct metal_res {
	MTLPixelFormat format;
	id<MTLTexture> texture;
	uint32_t width;
	uint32_t height;
};

struct metal {
	MTY_ColorFormat format;
	struct metal_cb fcb;
	struct metal_res staging[METAL_NUM_STAGING];
	id<MTLLibrary> library;
	id<MTLFunction> fs;
	id<MTLFunction> vs;
	id<MTLBuffer> vb;
	id<MTLBuffer> ib;
	id<MTLRenderPipelineState> pipeline;
	id<MTLSamplerState> ss_nearest;
	id<MTLSamplerState> ss_linear;
	id<MTLTexture> niltex;
};

struct gfx *mty_metal_create(MTY_Device *device)
{
	struct metal *ctx = MTY_Alloc(1, sizeof(struct metal));
	id<MTLDevice> _device = (__bridge id<MTLDevice>) device;

	bool r = true;

	MTLVertexDescriptor *vdesc = [MTLVertexDescriptor vertexDescriptor];
	MTLRenderPipelineDescriptor *pdesc = [MTLRenderPipelineDescriptor new];
	MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
	MTLTextureDescriptor *tdesc = [MTLTextureDescriptor new];

	NSError *nse = nil;
	ctx->library = [_device newLibraryWithSource:[NSString stringWithUTF8String:MTL_LIBRARY] options:nil error:&nse];
	if (nse) {
		r = false;
		MTY_Log("%s", [[nse localizedDescription] UTF8String]);
		goto except;
	}

	ctx->vs = [ctx->library newFunctionWithName:@"vs"];
	ctx->fs = [ctx->library newFunctionWithName:@"fs"];

	float vdata[] = {
		-1.0f,  1.0f,	// Position 0
		 0.0f,  0.0f,	// TexCoord 0
		-1.0f, -1.0f,	// Position 1
		 0.0f,  1.0f,	// TexCoord 1
		 1.0f, -1.0f,	// Position 2
		 1.0f,  1.0f,	// TexCoord 2
		 1.0f,  1.0f,	// Position 3
		 1.0f,  0.0f	// TexCoord 3
	};

	ctx->vb = [_device newBufferWithBytes:vdata length:sizeof(vdata) options:MTLResourceCPUCacheModeWriteCombined];

	uint16_t idata[] = {
		0, 1, 2,
		2, 3, 0
	};

	ctx->ib = [_device newBufferWithBytes:idata length:sizeof(idata) options:MTLResourceCPUCacheModeWriteCombined];

	vdesc.attributes[0].offset = 0;
	vdesc.attributes[0].format = MTLVertexFormatFloat2;
	vdesc.attributes[1].offset = 2 * sizeof(float);
	vdesc.attributes[1].format = MTLVertexFormatFloat2;
	vdesc.layouts[0].stepRate = 1;
	vdesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
	vdesc.layouts[0].stride = 4 * sizeof(float);

	pdesc.sampleCount = 1;
	pdesc.vertexFunction = ctx->vs;
	pdesc.fragmentFunction = ctx->fs;
	pdesc.vertexDescriptor = vdesc;
	pdesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

	ctx->pipeline = [_device newRenderPipelineStateWithDescriptor:pdesc error:&nse];
	if (nse) {
		r = false;
		MTY_Log("%s", [[nse localizedDescription] UTF8String]);
		goto except;
	}

	sd.minFilter = sd.magFilter =  MTLSamplerMinMagFilterLinear;
	sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
	sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
	ctx->ss_linear = [_device newSamplerStateWithDescriptor:sd];

	sd.minFilter = sd.magFilter =  MTLSamplerMinMagFilterNearest;
	ctx->ss_nearest = [_device newSamplerStateWithDescriptor:sd];

	tdesc.pixelFormat = MTLPixelFormatRGBA8Unorm;
	tdesc.storageMode = MTLStorageModePrivate;
	tdesc.width = 64;
	tdesc.height = 64;
	ctx->niltex = [_device newTextureWithDescriptor:tdesc];

	except:

	if (!r)
		mty_metal_destroy((struct gfx **) &ctx);

	return (struct gfx *) ctx;
}

static void metal_refresh_resource(struct metal_res *res, id<MTLDevice> device,
	MTLPixelFormat format, uint32_t width, uint32_t height)
{
	if (!res->texture || width != res->width || height != res->height || format != res->format) {
		res->texture = nil;

		MTLTextureDescriptor *tdesc = [MTLTextureDescriptor new];
		tdesc.pixelFormat = format;
		tdesc.cpuCacheMode = MTLCPUCacheModeWriteCombined;
		tdesc.width = width;
		tdesc.height = height;

		res->texture = [device newTextureWithDescriptor:tdesc];

		res->width = width;
		res->height = height;
		res->format = format;
	}
}

static void metal_reload_textures(struct metal *ctx, id<MTLDevice> device, const void *image, const MTY_RenderDesc *desc)
{
	int8_t bpp = FMT_BPP[desc->format];
	uint32_t div = FMT_DIV[desc->format];
	MTLPixelFormat fmt0 = FMT_PLANE0[desc->format];
	MTLPixelFormat fmt1 = FMT_PLANE1[desc->format];

	switch (FMT_PLANES[desc->format]) {
		case FMT_1_PLANE: {
			// 16-bit packed pixel formats were not available until Big Sur
			if (fmt0 == MTLPixelFormatB5G6R5Unorm || fmt0 == MTLPixelFormatBGR5A1Unorm)
				if (!@available(iOS 8.0, tvOS 9.0, macOS 11.0, *))
					break;

			metal_refresh_resource(&ctx->staging[0], device, fmt0, desc->cropWidth, desc->cropHeight);

			MTLRegion region = MTLRegionMake2D(0, 0, desc->cropWidth, desc->cropHeight);
			[ctx->staging[0].texture replaceRegion:region mipmapLevel:0 withBytes:image bytesPerRow:bpp * desc->imageWidth];
			break;
		}
		case FMT_2_PLANE: {
			// Y
			metal_refresh_resource(&ctx->staging[0], device, fmt0, desc->cropWidth, desc->cropHeight);

			MTLRegion region = MTLRegionMake2D(0, 0, desc->cropWidth, desc->cropHeight);
			[ctx->staging[0].texture replaceRegion:region mipmapLevel:0 withBytes:image bytesPerRow:bpp * desc->imageWidth];

			// UV
			metal_refresh_resource(&ctx->staging[1], device, fmt1, desc->cropWidth / 2, desc->cropHeight / div);

			const void *p = (uint8_t *) image + desc->imageWidth * desc->imageHeight * bpp;
			region = MTLRegionMake2D(0, 0, desc->cropWidth / 2, desc->cropHeight / div);
			[ctx->staging[1].texture replaceRegion:region mipmapLevel:0 withBytes:p bytesPerRow:bpp * desc->imageWidth];
			break;
		}
		case FMT_3_PLANE: {
			// Y
			metal_refresh_resource(&ctx->staging[0], device, fmt0, desc->cropWidth, desc->cropHeight);

			MTLRegion region = MTLRegionMake2D(0, 0, desc->cropWidth, desc->cropHeight);
			[ctx->staging[0].texture replaceRegion:region mipmapLevel:0 withBytes:image bytesPerRow:bpp * desc->imageWidth];

			// U
			uint8_t *p = (uint8_t *) image + desc->imageWidth * desc->imageHeight * bpp;
			metal_refresh_resource(&ctx->staging[1], device, fmt0, desc->cropWidth / div, desc->cropHeight / div);

			region = MTLRegionMake2D(0, 0, desc->cropWidth / div, desc->cropHeight / div);
			[ctx->staging[1].texture replaceRegion:region mipmapLevel:0 withBytes:p bytesPerRow:bpp * (desc->imageWidth / div)];

			// V
			p += (desc->imageWidth / div) * (desc->imageHeight / div) * bpp;
			metal_refresh_resource(&ctx->staging[2], device, fmt0, desc->cropWidth / div, desc->cropHeight / div);

			region = MTLRegionMake2D(0, 0, desc->cropWidth / div, desc->cropHeight / div);
			[ctx->staging[2].texture replaceRegion:region mipmapLevel:0 withBytes:p bytesPerRow:bpp * (desc->imageWidth / div)];
			break;
		}
	}
}

bool mty_metal_render(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Surface *dest)
{
	struct metal *ctx = (struct metal *) gfx;
	id<MTLDevice> _device = (__bridge id<MTLDevice>) device;
	id<MTLCommandQueue> cq = (__bridge id<MTLCommandQueue>) context;
	id<MTLTexture> _dest = (__bridge id<MTLTexture>) dest;

	// Don't do anything until we have real data
	if (desc->format != MTY_COLOR_FORMAT_UNKNOWN)
		ctx->format = desc->format;

	if (ctx->format == MTY_COLOR_FORMAT_UNKNOWN)
		return true;

	metal_reload_textures(ctx, _device, image, desc);

	// Begin render pass
	MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
	rpd.colorAttachments[0].texture = _dest;
	rpd.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
	rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
	rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

	id<MTLCommandBuffer> cb = [cq commandBuffer];
	id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];

	// Set both vertex and fragment shaders as part of the pipeline object
	[re setRenderPipelineState:ctx->pipeline];

	// Viewport
	float vpx, vpy, vpw, vph;
	mty_viewport(desc->rotation, desc->cropWidth, desc->cropHeight,
		desc->viewWidth, desc->viewHeight, desc->aspectRatio, desc->scale,
		&vpx, &vpy, &vpw, &vph);

	MTLViewport vp = {0};
	vp.originX = vpx;
	vp.originY = vpy;
	vp.width = vpw;
	vp.height = vph;
	[re setViewport:vp];

	// Vertex shader
	[re setVertexBuffer:ctx->vb offset:0 atIndex:0];

	// Fragment shader
	id<MTLSamplerState> sampler = desc->filter == MTY_FILTER_NEAREST ? ctx->ss_nearest : ctx->ss_linear;
	[re setFragmentSamplerState:sampler atIndex:0];

	// Uniforms
	for (uint8_t x = 0; x < METAL_NUM_STAGING; x++) {
		id<MTLTexture> tex = ctx->staging[x].texture ? ctx->staging[x].texture : ctx->niltex;
		[re setFragmentTexture:tex atIndex:x];
	}

	ctx->fcb.width = desc->cropWidth;
	ctx->fcb.height = desc->cropHeight;
	ctx->fcb.vp_height = vph;
	ctx->fcb.effects[0] = desc->effects[0];
	ctx->fcb.effects[1] = desc->effects[1];
	ctx->fcb.levels[0] = desc->levels[0];
	ctx->fcb.levels[1] = desc->levels[1];
	ctx->fcb.planes = FMT_PLANES[ctx->format];
	ctx->fcb.rotation = desc->rotation;
	ctx->fcb.conversion = FMT_CONVERSION(ctx->format, desc->fullRangeYUV);
	[re setFragmentBytes:&ctx->fcb length:sizeof(struct metal_cb) atIndex:0];

	// Draw
	[re drawIndexedPrimitives:MTLPrimitiveTypeTriangleStrip indexCount:6
		indexType:MTLIndexTypeUInt16 indexBuffer:ctx->ib indexBufferOffset:0];

	[re endEncoding];
	[cb commit];

	return true;
}

void mty_metal_destroy(struct gfx **gfx)
{
	if (!gfx || !*gfx)
		return;

	struct metal *ctx = (struct metal *) *gfx;

	for (uint8_t x = 0; x < METAL_NUM_STAGING; x++)
		ctx->staging[x].texture = nil;

	ctx->library = nil;
	ctx->fs = nil;
	ctx->vs = nil;
	ctx->vb = nil;
	ctx->ib = nil;
	ctx->pipeline = nil;
	ctx->ss_linear = nil;
	ctx->ss_nearest = nil;
	ctx->niltex = nil;

	MTY_Free(ctx);
	*gfx = NULL;
}


// State

void *mty_metal_get_state(MTY_Device *device, MTY_Context *context)
{
	return NULL;
}

void mty_metal_set_state(MTY_Device *device, MTY_Context *context, void *state)
{
}

void mty_metal_free_state(void **state)
{
}
