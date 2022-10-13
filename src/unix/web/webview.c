#include <string.h>
#include <math.h>

#include "matoya.h"
#include "webview.h"

typedef void (*on_message_func)(struct webview *ctx, const char *message);

void web_view_show(struct webview *ctx, on_message_func on_message);
void web_view_destroy();
void web_view_resize(bool hidden);
void web_view_navigate_url(const char *url);
void web_view_navigate_html(const char *html);
void web_view_javascript_eval(const char *js);

struct webview {
	struct webview_common common;
};

struct webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	mty_webview_create_common(ctx, html, debug, event, opaque);

	web_view_show(ctx, mty_webview_handle_event);

	web_view_navigate_html(ctx->common.html);

	return ctx;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;

	web_view_destroy();

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_resize(struct webview *ctx)
{
	web_view_resize(ctx->common.hidden);
}

void mty_webview_javascript_eval(struct webview *ctx, const char *js)
{
	web_view_javascript_eval(js);
}
