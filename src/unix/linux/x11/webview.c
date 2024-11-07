// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "webview.h"

#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <webkit2/webkit2.h>

#include "matoya.h"

#define DISPATCH(func, param, should_free) g_idle_add(G_SOURCE_FUNC(func), mty_webview_create_event(ctx, (void *) (param), should_free))

struct webview {
	struct webview_base base;

	MTY_Thread *thread;
	Display *display;
	Window x11_window;
	GtkWindow *gtk_window;
	WebKitWebView *webview;
};

struct mty_webview_event {
	struct webview *context;
	void *data;
	bool should_free;
};

struct xinfo {
	Display *display;
	XVisualInfo *vis;
	Window window;
};

static struct mty_webview_event *mty_webview_create_event(struct webview *ctx, void *data, bool should_free)
{
	struct mty_webview_event *event = MTY_Alloc(1, sizeof(struct mty_webview_event));

	event->context = ctx;
	event->data = data;
	event->should_free = should_free;

	return event;
}

static void mty_webview_destroy_event(struct mty_webview_event **event)
{
	if (!event || !*event)
		return;

	struct mty_webview_event *ev = *event;

	if (ev->should_free)
		MTY_Free(ev->data);

	MTY_Free(ev);
	*event = NULL;
}

static void handle_script_message(WebKitUserContentManager *manager, WebKitJavascriptResult *result, void *opaque)
{
	struct webview *ctx = opaque;

	JSCValue *value = webkit_javascript_result_get_js_value(result);
	char *str = jsc_value_to_string(value);
	mty_webview_base_handle_event(&ctx->base, str);
	MTY_Free(str);
}

static int32_t _mty_webview_resize(void *opaque)
{
	struct webview *ctx = opaque;

	XWindowAttributes attr = {0};
	XGetWindowAttributes(ctx->display, ctx->x11_window, &attr);

	int32_t width = 0, height = 0;
	gtk_window_get_size(ctx->gtk_window, &width, &height);

	if (width != attr.width || height != attr.height)
		gtk_window_resize(ctx->gtk_window, attr.width, attr.height);

	return true;
}

static bool _mty_webview_create(struct mty_webview_event *event)
{
	struct webview *ctx = event->context;

	ctx->gtk_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_POPUP));
	gtk_widget_realize(GTK_WIDGET(ctx->gtk_window));

	GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(ctx->gtk_window));
	XReparentWindow(GDK_WINDOW_XDISPLAY(gdk_window), GDK_WINDOW_XID(gdk_window), ctx->x11_window, 0, 0);

	ctx->webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
	gtk_container_add(GTK_CONTAINER(ctx->gtk_window), GTK_WIDGET(ctx->webview));

	gtk_widget_set_app_paintable(GTK_WIDGET(ctx->gtk_window), TRUE);
	webkit_web_view_set_background_color(ctx->webview, & (GdkRGBA) {0});

	WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(ctx->webview);
	g_signal_connect(manager, "script-message-received::native", G_CALLBACK(handle_script_message), ctx);
	webkit_user_content_manager_register_script_message_handler(manager, "native");

	const char *javascript = "window.native = {"
		"postMessage: (message) => window.webkit.messageHandlers.native.postMessage(message),"
		"addEventListener: (listener) => window.addEventListener('message', listener),"
	"}";
	WebKitUserContentInjectedFrames injected_frames = WEBKIT_USER_CONTENT_INJECT_TOP_FRAME;
	WebKitUserScriptInjectionTime injection_time = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START;
	WebKitUserScript *script = webkit_user_script_new(javascript, injected_frames, injection_time, NULL, NULL);
	webkit_user_content_manager_add_script(manager, script);

	WebKitSettings *settings = webkit_web_view_get_settings(event->context->webview);
	webkit_settings_set_enable_developer_extras(settings, ctx->base.debug);

	g_idle_add(_mty_webview_resize, ctx);

	gtk_widget_show_all(GTK_WIDGET(ctx->gtk_window));

	mty_webview_destroy_event(&event);

	return false;
}

static void *mty_webview_thread_func(void *opaque)
{
	gtk_init_check(0, NULL);
	gtk_main();

	return NULL;
}

struct webview *mty_webview_create(MTY_App *app, MTY_Window window, const char *dir,
	bool debug, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func, WEBVIEW_KEY key_func)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	g_setenv("GDK_BACKEND", "x11", true);

	mty_webview_base_create(&ctx->base, app, window, dir, debug, ready_func, text_func, key_func);

	ctx->thread = MTY_ThreadCreate(mty_webview_thread_func, NULL);

	struct xinfo *info = MTY_WindowGetNative(ctx->base.app, ctx->base.window);
	ctx->display = info->display;
	ctx->x11_window = info->window;

	DISPATCH(_mty_webview_create, NULL, false);

	return ctx;
}

static bool _mty_webview_destroy(struct mty_webview_event *event)
{
	struct webview *ctx = event->context;

	if (!ctx)
		return false;

	gtk_window_close(ctx->gtk_window);
	gtk_widget_destroy(GTK_WIDGET(ctx->webview));
	gtk_widget_destroy(GTK_WIDGET(ctx->gtk_window));
	gtk_main_quit();

	mty_webview_destroy_event(&event);

	return false;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;
	*webview = NULL;

	DISPATCH(_mty_webview_destroy, NULL, false);

	MTY_ThreadDestroy(&ctx->thread);

	mty_webview_base_destroy(&ctx->base);

	MTY_Free(ctx);
}

static bool _mty_webview_navigate_url(struct mty_webview_event *event)
{
	webkit_web_view_load_uri(event->context->webview, event->data);
	mty_webview_destroy_event(&event);

	return false;
}

static bool _mty_webview_navigate_html(struct mty_webview_event *event)
{
	webkit_web_view_load_html(event->context->webview, event->data, NULL);
	mty_webview_destroy_event(&event);

	return false;
}

static bool _mty_webview_show(struct mty_webview_event *event)
{
	GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(event->context->gtk_window));

	if (event->data) {
		gdk_window_show(window);

	} else {
		gdk_window_hide(window);
	}

	return false;
}

static bool _mty_webview_send_text(struct mty_webview_event *event)
{
	char *script = mty_webview_base_format_text(event->data);
	webkit_web_view_evaluate_javascript(event->context->webview, script, -1, NULL, NULL, NULL, NULL, NULL);
	MTY_Free(script);

	return false;
}

static bool _mty_webview_reload(struct mty_webview_event *event)
{
	webkit_web_view_reload(event->context->webview);

	return false;
}

void mty_webview_navigate(struct webview *ctx, const char *source, bool url)
{
	if (url) {
		DISPATCH(_mty_webview_navigate_url, MTY_Strdup(source), true);

	} else {
		DISPATCH(_mty_webview_navigate_html, MTY_Strdup(source), true);
	}
}

void mty_webview_show(struct webview *ctx, bool show)
{
	DISPATCH(_mty_webview_show, show, false);
}

bool mty_webview_is_visible(struct webview *ctx)
{
	return gdk_window_is_visible(gtk_widget_get_window(GTK_WIDGET(ctx->gtk_window)));
}

void mty_webview_send_text(struct webview *ctx, const char *msg)
{
	DISPATCH(_mty_webview_send_text, MTY_Strdup(msg), true);
}

void mty_webview_reload(struct webview *ctx)
{
	DISPATCH(_mty_webview_reload, NULL, false);
}

void mty_webview_set_input_passthrough(struct webview *ctx, bool passthrough)
{
	ctx->base.passthrough = passthrough;
}

bool mty_webview_event(struct webview *ctx, MTY_Event *evt)
{
	return false;
}

void mty_webview_run(struct webview *ctx)
{
}

void mty_webview_render(struct webview *ctx)
{
}

bool mty_webview_is_focussed(struct webview *ctx)
{
	return true;
}

bool mty_webview_is_steam(void)
{
	return false;
}

bool mty_webview_is_available(void)
{
	return true;
}
