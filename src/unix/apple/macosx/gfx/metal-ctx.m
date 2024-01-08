// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_metal_)

#include <Metal/Metal.h>
#include <AppKit/AppKit.h>
#include <QuartzCore/CAMetalLayer.h>

#include "gfx/sync.h"
#include "display-link.h"
#include "scale.h"

struct metal_ctx {
	NSWindow *window;
	CAMetalLayer *layer;
	id<CAMetalDrawable> back_buffer;
	id<MTLCommandQueue> cq;
	CGSize size;

	struct sync sync;
	struct display_link dlink;
	int64_t pcount;
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

	if (!device || ![device supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v1])
		return NULL;

	struct metal_ctx *ctx = MTY_Alloc(1, sizeof(struct metal_ctx));
	ctx->window = (__bridge NSWindow *) native_window;

	metal_ctx_mt_block(^{
		ctx->layer = [CAMetalLayer layer];
		ctx->layer.device = device;
		ctx->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

		if (vsync) {
			sync_set_interval(&ctx->sync, 100);
			ctx->layer.displaySyncEnabled = YES;

		} else {
			ctx->layer.displaySyncEnabled = NO;
		}

		ctx->cq = [ctx->layer.device newCommandQueue];

		ctx->window.contentView.wantsLayer = YES;
		ctx->window.contentView.layer = ctx->layer;
	});

	return (struct gfx_ctx *) ctx;
}

void mty_metal_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct metal_ctx *ctx = (struct metal_ctx *) *gfx_ctx;

	display_link_destroy(&ctx->dlink);

	ctx->window = nil;
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

static void metal_ctx_refresh(struct metal_ctx *ctx)
{
	CGSize size = ctx->window.contentView.frame.size;
	CGFloat scale = mty_screen_scale(ctx->window.screen);

	size.width *= scale;
	size.height *= scale;

	if (size.width != ctx->size.width || size.height != ctx->size.height) {
		ctx->layer.drawableSize = size;
		ctx->size = size;
	}
}

MTY_Surface *mty_metal_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	@autoreleasepool {
		if (!ctx->back_buffer) {
			metal_ctx_refresh(ctx);
			ctx->back_buffer = [ctx->layer nextDrawable];
		}
	}

	return (__bridge MTY_Surface *) ctx->back_buffer.texture;
}

void mty_metal_ctx_set_sync_interval(struct gfx_ctx *gfx_ctx, uint32_t interval)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	sync_set_interval(&ctx->sync, interval);
}

void mty_metal_ctx_present(struct gfx_ctx *gfx_ctx)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	if (ctx->back_buffer) {
		int64_t interval = sync_next_interval(&ctx->sync);

		id<MTLCommandBuffer> cb = [ctx->cq commandBuffer];
		[cb presentDrawable:ctx->back_buffer];
		[cb commit];
		[cb waitUntilCompleted];

		int64_t elapsed = display_link_get_counter(&ctx->dlink) - ctx->pcount;

		for (int64_t x = 0; x < interval - elapsed; x++)
			display_link_wait(&ctx->dlink);

		ctx->pcount = display_link_get_counter(&ctx->dlink);
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
