// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

struct webview;

typedef void (*WEBVIEW_READY)(MTY_App *app, MTY_Window window);
typedef void (*WEBVIEW_TEXT)(MTY_App *app, MTY_Window window, const char *text);
typedef void (*WEBVIEW_KEY)(MTY_App *app, MTY_Window window, bool pressed, MTY_Key key, MTY_Mod mods);

struct webview_base {
	MTY_App *app;
	MTY_Window window;
	WEBVIEW_READY ready_func;
	WEBVIEW_TEXT text_func;
	WEBVIEW_KEY key_func;
	MTY_Queue *pushq;
	MTY_Hash *keys;
	const char *dir;
	bool ready;
	bool passthrough;
	bool focussed;
	bool debug;
};

void mty_webview_base_create(struct webview_base *ctx, MTY_App *app, MTY_Window window, const char *dir,
	bool debug, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func, WEBVIEW_KEY key_func);
void mty_webview_base_destroy(struct webview_base *ctx);
void mty_webview_base_handle_event(struct webview_base *ctx, const char *str);
char *mty_webview_base_format_text(const char *msg);

struct webview *mty_webview_create(MTY_App *app, MTY_Window window, const char *dir,
	bool debug, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func, WEBVIEW_KEY key_func);
void mty_webview_destroy(struct webview **webview);
void mty_webview_navigate(struct webview *ctx, const char *source, bool url);
void mty_webview_show(struct webview *ctx, bool show);
bool mty_webview_is_visible(struct webview *ctx);
void mty_webview_send_text(struct webview *ctx, const char *msg);
void mty_webview_reload(struct webview *ctx);
void mty_webview_set_input_passthrough(struct webview *ctx, bool passthrough);
bool mty_webview_event(struct webview *ctx, MTY_Event *evt);
void mty_webview_run(struct webview *ctx);
void mty_webview_render(struct webview *ctx);
bool mty_webview_is_focussed(struct webview *ctx);
bool mty_webview_is_steam(void);
bool mty_webview_is_available(void);
