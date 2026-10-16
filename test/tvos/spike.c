// tvOS spike: render from a background thread (as Parsec does), print every event,
// and change the clear color on remote presses / controller buttons.
#include <stdio.h>
#include <math.h>

#include "matoya.h"

void spike_diag_controllers(void);

struct context {
	MTY_App *app;
	MTY_Thread *thread;
	MTY_Atomic32 hue_offset;
	MTY_Atomic32 frames;
	bool quit;
};

static void event_func(const MTY_Event *evt, void *opaque)
{
	struct context *ctx = opaque;

	MTY_PrintEvent(evt);

	if (evt->type == MTY_EVENT_KEY && evt->key.pressed)
		MTY_Atomic32Set(&ctx->hue_offset, MTY_Atomic32Get(&ctx->hue_offset) + 60);

	if (evt->type == MTY_EVENT_CONTROLLER && evt->controller.buttons[MTY_CBUTTON_A])
		MTY_Atomic32Set(&ctx->hue_offset, MTY_Atomic32Get(&ctx->hue_offset) + 120);

	if (evt->type == MTY_EVENT_CLOSE)
		ctx->quit = true;
}

static bool app_func(void *opaque)
{
	struct context *ctx = opaque;

	static MTY_Time last;
	MTY_Time now = MTY_GetTime();

	if (MTY_TimeDiff(last, now) > 2000.0f) {
		MTY_Size size = MTY_WindowGetSize(ctx->app, 0);
		printf("[spike] main thread alive, frames presented: %d, window %ux%u @ %ux scale, %u Hz, platform 0x%X\n",
			MTY_Atomic32Get(&ctx->frames), size.w, size.h, (unsigned) MTY_WindowGetScreenScale(ctx->app, 0),
			MTY_WindowGetRefreshRate(ctx->app, 0), MTY_GetPlatform());
		fflush(stdout);
		spike_diag_controllers();
		last = now;
	}

	return !ctx->quit;
}

static void hue_to_rgb(float h, float *r, float *g, float *b)
{
	*r = 0.5f + 0.5f * cosf(h);
	*g = 0.5f + 0.5f * cosf(h - 2.094f);
	*b = 0.5f + 0.5f * cosf(h + 2.094f);
}

static void *render_thread(void *opaque)
{
	struct context *ctx = opaque;

	MTY_WindowSetGFX(ctx->app, 0, MTY_GetDefaultGFX(), true);
	printf("[spike] render thread: GFX api %d\n", MTY_WindowGetGFX(ctx->app, 0));
	fflush(stdout);

	MTY_Time start = MTY_GetTime();

	while (!ctx->quit) {
		float t = MTY_TimeDiff(start, MTY_GetTime()) / 1000.0f;
		float h = t * 0.5f + MTY_Atomic32Get(&ctx->hue_offset) * 3.14159f / 180.0f;

		float r, g, b;
		hue_to_rgb(h, &r, &g, &b);

		MTY_WindowClear(ctx->app, 0, r, g, b, 1.0f);
		MTY_WindowPresent(ctx->app, 0);
		MTY_Atomic32Set(&ctx->frames, MTY_Atomic32Get(&ctx->frames) + 1);
	}

	MTY_WindowSetGFX(ctx->app, 0, MTY_GFX_NONE, false);

	return NULL;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	printf("[spike] main() entered\n");

	struct context ctx = {0};
	ctx.app = MTY_AppCreate(0, app_func, event_func, &ctx);
	if (!ctx.app) {
		printf("[spike] MTY_AppCreate failed\n");
		return 1;
	}

	MTY_WindowCreate(ctx.app, "tvOS Spike", NULL, 0);
	printf("[spike] window created\n");

	ctx.thread = MTY_ThreadCreate(render_thread, &ctx);

	MTY_AppSetTimeout(ctx.app, 8);
	MTY_AppRun(ctx.app);

	MTY_ThreadDestroy(&ctx.thread);
	MTY_AppDestroy(&ctx.app);

	return 0;
}
