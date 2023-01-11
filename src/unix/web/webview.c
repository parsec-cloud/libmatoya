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

static void mty_webview_handle_event(struct webview *ctx, const char *message)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_WEBVIEW;
	evt.message = message;

	ctx->common.event(&evt, ctx->common.opaque);
}

struct webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	ctx->common.html = MTY_Strdup(html);
	ctx->common.debug = debug;
	ctx->common.event = event;
	ctx->common.opaque = opaque;

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

	MTY_Free(ctx->common.html);

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_resize(struct webview *ctx)
{
	if (!ctx)
		return;

	web_view_resize(ctx->common.hidden);
}

void mty_webview_show(struct webview *ctx, bool show)
{
	if (!ctx)
		return;

	ctx->common.hidden = !show;

	mty_webview_resize(ctx);
}

bool mty_webview_is_visible(struct webview *ctx)
{
	return ctx && !ctx->common.hidden;
}

void mty_webview_event(struct webview *ctx, const char *name, const char *message)
{
	if (!ctx)
		return;

	char *javascript = MTY_SprintfD(JAVASCRIPT_EVENT_DISPATCH, name, message);

	web_view_javascript_eval(javascript);

	MTY_Free(javascript);
}
