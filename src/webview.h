#pragma once

#include "matoya.h"

#define JAVASCRIPT_EVENT_DISPATCH "window.dispatchEvent(new CustomEvent('%s', { detail: %s } ));"

struct webview_common {
	bool debug;
	bool hidden;
	char *html;
	void *opaque;
	MTY_EventFunc event;
};

struct webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque);
void mty_webview_destroy(struct webview **webview);
void mty_webview_show(struct webview *ctx, bool show);
bool mty_webview_is_visible(struct webview *ctx);
void mty_webview_event(struct webview *ctx, const char *name, const char *message);
void mty_webview_resize(struct webview *ctx);

#if defined(_WIN32)
bool mty_webview_has_focus(struct webview *ctx);
#endif
