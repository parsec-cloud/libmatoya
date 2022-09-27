#include <string.h>
#include <math.h>

#include "matoya.h"
#include "webview.h"

typedef void (*on_message_func)(MTY_Webview *ctx, const char *message);

void web_view_show(MTY_Webview *ctx, on_message_func on_message);
void web_view_destroy();
void web_view_resize(bool hidden);
void web_view_navigate_url(const char *url);
void web_view_navigate_html(const char *html);
void web_view_javascript_eval(const char *js);

struct MTY_Webview {
	struct webview common;
};

MTY_Webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque)
{
	MTY_Webview *ctx = MTY_Alloc(1, sizeof(MTY_Webview));

	mty_webview_create_common(ctx, html, debug, event, opaque);

	web_view_show(ctx, mty_webview_handle_event);

	web_view_navigate_html(ctx->common.html);

	return ctx;
}

void mty_webview_destroy(MTY_Webview **webview)
{
	if (!webview || !*webview)
		return;

	MTY_Webview *ctx = *webview;

	web_view_destroy();

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_resize(MTY_Webview *ctx)
{
	web_view_resize(ctx->common.hidden);
}

void mty_webview_javascript_eval(MTY_Webview *ctx, const char *js)
{
	web_view_javascript_eval(js);
}
