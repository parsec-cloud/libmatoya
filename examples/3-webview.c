#include <stdio.h>
#include <string.h>

#include "matoya.h"

struct context {
	MTY_App *app;
	MTY_Window window;
	bool quit;
};

static void event_func(const MTY_Event *evt, void *opaque)
{
	struct context *ctx = opaque;

	MTY_PrintEvent(evt);

	if (evt->type == MTY_EVENT_CLOSE)
		ctx->quit = true;
}

static bool app_func(void *opaque)
{
	struct context *ctx = opaque;

	MTY_WindowPresent(ctx->app, ctx->window);

	return !ctx->quit;
}

static void app_log(const char *message, void *opaque)
{
	printf("%s\n", message);
}

int main(int argc, char **argv)
{
	MTY_SetLogFunc(app_log, NULL);

	struct context ctx = {0};
	ctx.app = MTY_AppCreate(app_func, event_func, &ctx);
	if (!ctx.app)
		return 1;

	ctx.window = MTY_WindowCreate(ctx.app, "My Window", NULL, 0);
	MTY_WindowSetGFX(ctx.app, 0, MTY_GFX_GL, true);
	MTY_Frame frame = MTY_MakeDefaultFrame(0, 0, 1000, 700, 0.75);
	MTY_WindowSetFrame(ctx.app, ctx.window, &frame);

	const char *html =
		"<html>"
		"  <head>"
		"    <style> body { background-color: darkslategrey; } </style>"
		"  </head>"
		"  <body>"
		"    <span id='my_id'>Hello World!</span>"
		"    <script> document.getElementById('my_id').style.color = 'goldenrod'; </script>"
		"  </body>"
		"</html>";

	MTY_WebviewCreate(ctx.app, ctx.window, html, true);

	MTY_AppRun(ctx.app);
	MTY_AppDestroy(&ctx.app);

	return 0;
}
