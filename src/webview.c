#include "webview.h"

#include <stdio.h>
#include <string.h>

#include "unix/web/keymap.h"

void mty_webview_base_create(struct webview_base *ctx, MTY_App *app, MTY_Window window, const char *dir,
	bool debug, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func, WEBVIEW_KEY key_func)
{
    ctx->app = app;
	ctx->window = window;
	ctx->ready_func = ready_func;
	ctx->text_func = text_func;
	ctx->key_func = key_func;
	ctx->keys = web_keymap_hash();
	ctx->pushq = MTY_QueueCreate(50, 0);
    ctx->debug = debug;
    ctx->dir = dir;
}

void mty_webview_base_destroy(struct webview_base *ctx)
{
    if (ctx->pushq)
		MTY_QueueFlush(ctx->pushq, MTY_Free);

	MTY_QueueDestroy(&ctx->pushq);
	MTY_HashDestroy(&ctx->keys, NULL);
}

void mty_webview_base_handle_event(struct webview_base *ctx, const char *str)
{
    MTY_JSON *j = NULL;

	switch (str[0]) {
		// MTY_EVENT_WEBVIEW_READY
		case 'R':
			ctx->ready = true;

			// Send any queued messages before the WebView became ready
			for (char *msg = NULL; MTY_QueuePopPtr(ctx->pushq, 0, (void **) &msg, NULL);) {
				mty_webview_send_text((struct webview *) ctx, msg);
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
}

char *mty_webview_base_format_text(const char *msg)
{
	uint32_t msg_len = strlen(msg);
	uint32_t b64_len = msg_len * 4;
	char *b64 = MTY_Alloc(b64_len, 1);

	MTY_BytesToBase64(msg, msg_len, b64, b64_len);

	char *script = MTY_SprintfD("window.postMessage(atob('%s'));", b64);

	MTY_Free(b64);

	return script;
}
