// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "webview.h"

#include <string.h>
#include <math.h>

#include "matoya.h"
#include "web/keymap.h"

struct webview {
	MTY_App *app;
	MTY_Window window;
	WEBVIEW_READY ready_func;
	WEBVIEW_TEXT text_func;
	WEBVIEW_KEY key_func;
	MTY_Hash *keys;
	MTY_Queue *pushq;
	bool ready;
	bool passthrough;
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

	ctx->app = app;
	ctx->window = window;
	ctx->ready_func = ready_func;
	ctx->text_func = text_func;
	ctx->key_func = key_func;

	ctx->keys = web_keymap_hash();
	ctx->pushq = MTY_QueueCreate(50, 0);

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

	if (ctx->pushq)
		MTY_QueueFlush(ctx->pushq, MTY_Free);

	MTY_QueueDestroy(&ctx->pushq);
	MTY_HashDestroy(&ctx->keys, NULL);

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
	web_webview_send_text(msg);
}

void mty_webview_reload(struct webview *ctx)
{
	web_webview_reload();
}

void mty_webview_set_input_passthrough(struct webview *ctx, bool passthrough)
{
	ctx->passthrough = passthrough;
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
	MTY_JSON *j = NULL;

	switch (str[0]) {
		// MTY_EVENT_WEBVIEW_READY
		case 'R':
			ctx->ready = true;

			// Send any queued messages before the WebView became ready
			for (char *msg = NULL; MTY_QueuePopPtr(ctx->pushq, 0, (void **) &msg, NULL);) {
				mty_webview_send_text(ctx, msg);
				MTY_Free(msg);
			}

			ctx->ready_func(ctx->app, ctx->window);
			break;

		// MTY_EVENT_WEBVIEW_TEXT
		case 'T':
			ctx->text_func(ctx->app, ctx->window, str + 1);
			break;

		// MTY_EVENT_KEY
		case 'D':
		case 'U':
			if (!ctx->passthrough)
				break;

			j = MTY_JSONParse(str + 1);
			if (!j)
				break;

			const char *code = MTY_JSONObjGetStringPtr(j, "code");
			if (!code)
				break;

			uint32_t jmods = 0;
			if (!MTY_JSONObjGetInt(j, "mods", (int32_t *) &jmods))
				break;

			MTY_Key key = (MTY_Key) (uintptr_t) MTY_HashGet(ctx->keys, code) & 0xFFFF;
			if (key == MTY_KEY_NONE)
				break;

			MTY_Mod mods = web_keymap_mods(jmods);

			ctx->key_func(ctx->app, ctx->window, str[0] == 'D', key, mods);
			break;
	}

	MTY_JSONDestroy(&j);
	MTY_Free(str);
}
