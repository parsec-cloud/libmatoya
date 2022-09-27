#pragma once

#include "matoya.h"

struct webview {
	char *html;
	char *origin;

	MTY_EventFunc event;

	bool debug;
	bool hidden;
	bool has_focus;

	void *opaque;
};

MTY_Webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque);
void mty_webview_create_common(MTY_Webview *ctx, const char *html, bool debug, MTY_EventFunc event, void *opaque);
void mty_webview_destroy(MTY_Webview **webview);
void mty_webview_destroy_common(MTY_Webview *ctx);
void mty_webview_show(MTY_Webview *ctx, bool show);
bool mty_webview_is_visible(MTY_Webview *ctx);
void mty_webview_event(MTY_Webview *ctx, const char *name, const char *message);
void mty_webview_resize(MTY_Webview *ctx);
bool mty_webview_has_focus(MTY_Webview *ctx);
void mty_webview_handle_event(MTY_Webview *ctx, const char *message);
void mty_webview_javascript_eval(MTY_Webview *ctx, const char *js);
