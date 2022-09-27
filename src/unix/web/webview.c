#include <string.h>
#include <math.h>

#include "matoya.h"
#include "webview.h"

typedef void (*mty_webview_on_message)(MTY_Webview *ctx, const char *message);
typedef const void *(*mty_webview_get_resource)(MTY_Webview *ctx, const char *url, size_t *size, const char **mime);

void web_view_show(MTY_Webview *ctx, mty_webview_on_message on_message, mty_webview_get_resource get_resource);
void web_view_destroy(MTY_Webview *ctx);
void web_view_resize(MTY_Webview *ctx, int32_t x, int32_t y, uint32_t width, uint32_t height, bool auto_size);
void web_view_navigate_url(MTY_Webview *ctx, const char *url);
void web_view_navigate_html(MTY_Webview *ctx, const char *html);
void web_view_javascript_init(MTY_Webview *ctx, const char *js);
void web_view_javascript_eval(MTY_Webview *ctx, const char *js);

struct MTY_Webview {
	struct webview common;
};

MTY_Webview *mty_webview_create(uint32_t identifier, void *handle, void *opaque)
{
	MTY_Webview *ctx = MTY_Alloc(1, sizeof(MTY_Webview));

	mty_webview_create_common(ctx, identifier, opaque);

	return ctx;
}

void mty_webview_destroy(MTY_Webview *ctx)
{
	if (!ctx)
		return;

	web_view_destroy(ctx);

	MTY_Free(ctx);
}

void mty_webview_resize(MTY_Webview *ctx)
{
	int32_t x = lrint(ctx->common.bounds.left);
	int32_t y = lrint(ctx->common.bounds.top);
	uint32_t width = lrint(ctx->common.bounds.right);
	uint32_t height = lrint(ctx->common.bounds.bottom);

	web_view_resize(ctx, x, y, width, height, ctx->common.auto_size);
}

void MTY_WebviewShow(MTY_Webview *ctx, MTY_WebviewOnCreated callback)
{
	web_view_show(ctx, mty_webview_handle_event, MTY_WebviewGetResource);

	callback(ctx, ctx->common.opaque);
}

void MTY_WebviewEnableDevTools(MTY_Webview *ctx, bool enable)
{
	// XXX Can't be done programmatically
}

void MTY_WebviewOpenDevTools(MTY_Webview *ctx)
{
	// XXX Can't be done programmatically
}

void MTY_WebviewNavigateURL(MTY_Webview *ctx, const char *url)
{
	web_view_navigate_url(ctx, url);
}

void MTY_WebviewNavigateHTML(MTY_Webview *ctx, const char *html)
{
	web_view_navigate_html(ctx, html);
}

void MTY_WebviewJavascriptInit(MTY_Webview *ctx, const char *js)
{
	web_view_javascript_init(ctx, js);
}

void MTY_WebviewJavascriptEval(MTY_Webview *ctx, const char *js)
{
	web_view_javascript_eval(ctx, js);
}
