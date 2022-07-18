#pragma once

#include "matoya.h"

struct webview {
	MTY_Hash *bindings;
	MTY_Hash *resources;
	MTY_Rect bounds;
	uint32_t identifier;
	const char *scheme;
	const char *vpath;
	bool has_focus;
	bool auto_size;
	void *opaque;
};

MTY_Webview *mty_window_create_webview(MTY_Hash *webviews, void *handle, void *opaque);
void mty_window_destroy_webview(MTY_Hash *webviews, MTY_Webview **webview);
MTY_Webview *mty_webview_create(uint32_t identifier, void *handle, void *opaque);
void mty_webview_create_common(MTY_Webview *ctx, uint32_t identifier, void *opaque);
void mty_webview_destroy(MTY_Webview *ctx);
void mty_webview_destroy_common(MTY_Webview *ctx);
void mty_webview_resize(MTY_Webview *ctx);
uint32_t mty_webview_get_identifier(MTY_Webview *ctx);
bool mty_webview_has_focus(MTY_Webview *ctx);
void mty_webview_handle_event(MTY_Webview *ctx, const char *message);

bool mty_webviews_has_focus(MTY_Hash *webviews);
void mty_webviews_resize(MTY_Hash *webviews);
