// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "webview.h"

#include <string.h>
#include <math.h>

#include "matoya.h"

struct webview {
	struct webview_base base;
};

void web_webview_create(struct webview *ctx);
void web_webview_destroy();
void web_webview_navigate(const char *source, bool url);
void web_webview_show(bool show);
bool web_webview_is_visible();
void web_webview_send_text(const char *message);
void web_webview_reload();

struct webview *mty_webview_create(MTY_App *app, MTY_Window window, const char *dir,
	bool debug, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func, WEBVIEW_KEY key_func)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	mty_webview_base_create(&ctx->base, app, window, dir, debug, ready_func, text_func, key_func);
	web_webview_create(ctx);

	return ctx;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;
	*webview = NULL;

	web_webview_destroy();
	mty_webview_base_destroy(&ctx->base);

	MTY_Free(ctx);
}

void mty_webview_navigate(struct webview *ctx, const char *source, bool url)
{
	web_webview_navigate(source, url);
}

void mty_webview_show(struct webview *ctx, bool show)
{
	web_webview_show(show);
}

bool mty_webview_is_visible(struct webview *ctx)
{
	return web_webview_is_visible();
}

void mty_webview_send_text(struct webview *ctx, const char *msg)
{
	if (!ctx->base.ready) {
		MTY_QueuePushPtr(ctx->base.pushq, MTY_Strdup(msg), 0);

	} else {
		web_webview_send_text(msg);
	}
}

void mty_webview_reload(struct webview *ctx)
{
	web_webview_reload();
}

void mty_webview_set_input_passthrough(struct webview *ctx, bool passthrough)
{
	ctx->base.passthrough = passthrough;
}

bool mty_webview_event(struct webview *ctx, MTY_Event *evt)
{
	return false;
}

void mty_webview_run(struct webview *ctx)
{
}

void mty_webview_render(struct webview *ctx)
{
}

bool mty_webview_is_focussed(struct webview *ctx)
{
	return true;
}

bool mty_webview_is_steam(void)
{
	return false;
}

bool mty_webview_is_available(void)
{
	return true;
}

__attribute__((export_name("mty_webview_handle_event")))
void mty_webview_handle_event(struct webview *ctx, char *str)
{
	mty_webview_base_handle_event(&ctx->base, str);
	MTY_Free(str);
}
