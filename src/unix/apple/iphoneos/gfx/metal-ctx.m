// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_metal_)

#include <math.h>

#include <Metal/Metal.h>
#include <UIKit/UIKit.h>
#include <QuartzCore/CAMetalLayer.h>

struct metal_ctx {
	CAMetalLayer *layer;
	id<CAMetalDrawable> back_buffer;
	id<MTLCommandQueue> cq;
	CGSize size;
};

static void metal_ctx_mt_block(void (^block)(void))
{
	if ([NSThread isMainThread]) {
		block();

	} else {
		dispatch_sync(dispatch_get_main_queue(), block);
	}
}

struct gfx_ctx *mty_metal_ctx_create(void *native_window, bool vsync)
{
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (!device)
		return NULL;

	struct metal_ctx *ctx = MTY_Alloc(1, sizeof(struct metal_ctx));
	UIWindow *window = (__bridge UIWindow *) native_window;

	// The UIKit work happens on the main thread, the context itself may be used from any
	// thread. UIKit presents are always synced to the display, so vsync has no effect.
	metal_ctx_mt_block(^{
		UIView *view = window.rootViewController.view;
		CGFloat scale = window.screen.nativeScale;

		ctx->layer = [CAMetalLayer layer];
		ctx->layer.device = device;
		ctx->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		ctx->layer.framebufferOnly = YES;
		ctx->layer.frame = view.bounds;
		ctx->layer.contentsScale = scale;

		// tvOS windows always fill the screen, so the size is fixed for the life of the context
		ctx->size = CGSizeMake(view.bounds.size.width * scale, view.bounds.size.height * scale);
		ctx->layer.drawableSize = ctx->size;

		[view.layer addSublayer:ctx->layer];
	});

	ctx->cq = [device newCommandQueue];

	return (struct gfx_ctx *) ctx;
}

void mty_metal_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct metal_ctx *ctx = (struct metal_ctx *) *gfx_ctx;

	CAMetalLayer *layer = ctx->layer;
	metal_ctx_mt_block(^{
		[layer removeFromSuperlayer];
	});

	ctx->layer = nil;
	ctx->cq = nil;
	ctx->back_buffer = nil;

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

void mty_metal_ctx_get_size(struct gfx_ctx *gfx_ctx, uint32_t *w, uint32_t *h)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	*w = lrint(ctx->size.width);
	*h = lrint(ctx->size.height);
}

MTY_Device *mty_metal_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	return (__bridge MTY_Device *) ctx->cq.device;
}

MTY_Context *mty_metal_ctx_get_context(struct gfx_ctx *gfx_ctx)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	return (__bridge MTY_Context *) ctx->cq;
}

MTY_Surface *mty_metal_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	@autoreleasepool {
		if (!ctx->back_buffer)
			ctx->back_buffer = [ctx->layer nextDrawable];
	}

	return (__bridge MTY_Surface *) ctx->back_buffer.texture;
}

void mty_metal_ctx_set_sync_interval(struct gfx_ctx *gfx_ctx, uint32_t interval)
{
}

void mty_metal_ctx_present(struct gfx_ctx *gfx_ctx)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	if (ctx->back_buffer) {
		id<MTLCommandBuffer> cb = [ctx->cq commandBuffer];
		[cb presentDrawable:ctx->back_buffer];
		[cb commit];
		[cb waitUntilCompleted];

		ctx->back_buffer = nil;
	}
}

bool mty_metal_ctx_lock(struct gfx_ctx *gfx_ctx)
{
	return true;
}

void mty_metal_ctx_unlock(void)
{
}
