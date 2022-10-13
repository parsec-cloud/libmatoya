#pragma once

#include "matoya.h"

struct webview_common {
	char *html;
	char *origin;

	MTY_EventFunc event;

	bool debug;
	bool hidden;
	bool has_focus;

	void *opaque;
};

struct webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque);
void mty_webview_create_common(struct webview *ctx, const char *html, bool debug, MTY_EventFunc event, void *opaque);
void mty_webview_destroy(struct webview **webview);
void mty_webview_destroy_common(struct webview *ctx);
void mty_webview_show(struct webview *ctx, bool show);
bool mty_webview_is_visible(struct webview *ctx);
void mty_webview_event(struct webview *ctx, const char *name, const char *message);
void mty_webview_resize(struct webview *ctx);
bool mty_webview_has_focus(struct webview *ctx);
void mty_webview_handle_event(struct webview *ctx, const char *message);
void mty_webview_javascript_eval(struct webview *ctx, const char *js);
