#include <string.h>

#include "matoya.h"
#include "webview.h"

struct webview_resource {
	void *data;
	size_t size;
};

void mty_webview_create_common(MTY_Webview *ctx, const char *html, bool debug, MTY_EventFunc event, void *opaque)
{
	struct webview *common = (struct webview *) ctx;

	common->html = MTY_Strdup(html);
	common->debug = debug;

	common->event   = event;
	common->opaque  = opaque;
}

void mty_webview_destroy_common(MTY_Webview *ctx)
{
	struct webview *common = (struct webview *) ctx;

	MTY_Free(common->html);
}

bool mty_webview_has_focus(MTY_Webview *ctx)
{
	struct webview *common = (struct webview *) ctx;

	return common && common->has_focus;
}

void mty_webview_handle_event(MTY_Webview *ctx, const char *message)
{
	struct webview *common = (struct webview *) ctx;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_WEBVIEW;
	evt.message = message;

	common->event(&evt, common->opaque);
}

void mty_webview_show(MTY_Webview *ctx, bool show)
{
	struct webview *common = (struct webview *) ctx;

	if (!common)
		return;

	common->hidden = !show;

	mty_webview_resize(ctx);
}

bool mty_webview_is_visible(MTY_Webview *ctx)
{
	struct webview *common = (struct webview *) ctx;

	return common && !common->hidden;
}

void mty_webview_event(MTY_Webview *ctx, const char *name, const char *message)
{
	char *javascript = MTY_SprintfD(
		"window.dispatchEvent("
			"new CustomEvent('%s', { detail: '%s' } )"
		");"
	, name, message);

	mty_webview_javascript_eval(ctx, javascript);

	MTY_Free(javascript);
}
