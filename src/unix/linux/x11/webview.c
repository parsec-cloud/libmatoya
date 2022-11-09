#include <string.h>
#include <math.h>

#include "matoya.h"
#include "webview.h"
#include "dl/webkit.h"

#define DISPATCH(func, param, should_free) g_idle_add((GSourceFunc) (GCallback) func, mty_webview_create_event(ctx, (void *) (param), should_free))

MTY_Thread *thread = NULL;

struct webview {
	struct webview_common common;

	Display *display;
	Window window;
	GtkWindow *gtk_window;
	WebKitWebView *webview;
};

struct mty_webview_event {
	struct webview *context;
	void *data;
	bool should_free;
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

static bool mty_webview_navigate(struct mty_webview_event *event)
{
	webkit_web_view_load_html(event->context->webview, event->data, NULL);

	mty_webview_destroy_event(&event);

	return false;
}

static bool mty_webview_enable_dev_tools(struct mty_webview_event *event)
{
	WebKitSettings *settings = webkit_web_view_get_settings(event->context->webview);
	webkit_settings_set_enable_developer_extras(settings, (bool) event->data);

	mty_webview_destroy_event(&event);

	return false;
}

static void mty_webview_handle_event(struct webview *ctx, const char *message)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_WEBVIEW;
	evt.message = message;

	ctx->common.event(&evt, ctx->common.opaque);
}

static void handle_script_message(WebKitUserContentManager *manager, WebKitJavascriptResult *result, void *opaque)
{
	struct webview *ctx = opaque;

	JSCValue *value = webkit_javascript_result_get_js_value(result);
	char *message = jsc_value_to_string(value);
	mty_webview_handle_event(ctx, message);
}

static bool mty_webview_dispatch_javascript_init(struct mty_webview_event *event)
{
	WebKitUserContentInjectedFrames injected_frames = WEBKIT_USER_CONTENT_INJECT_TOP_FRAME;
	WebKitUserScriptInjectionTime injection_time = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START;

	WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(event->context->webview);
	WebKitUserScript *script = webkit_user_script_new(event->data, injected_frames, injection_time, NULL, NULL);
	webkit_user_content_manager_add_script(manager, script);

	mty_webview_destroy_event(&event);

	return false;
}

static bool mty_webview_dispatch_javascript_eval(struct mty_webview_event *event)
{
	webkit_web_view_run_javascript(event->context->webview, event->data, NULL, NULL, NULL);

	mty_webview_destroy_event(&event);

	return false;
}

static bool mty_webview_add_new(struct mty_webview_event *event)
{
	struct webview *ctx = event->context;

	ctx->gtk_window = (GtkWindow *) gtk_window_new(GTK_WINDOW_POPUP);
	gtk_widget_realize((GtkWidget *) ctx->gtk_window);

	GdkWindow *gdk_window = gtk_widget_get_window((GtkWidget *) ctx->gtk_window);
	Display *display = gdk_x11_display_get_xdisplay(gdk_window_get_display(gdk_window));
	Window window = gdk_x11_window_get_xid(gdk_window);
	XReparentWindow(display, window, ctx->window, 0, 0);

	WebKitWebContext *context = webkit_web_context_new();

	ctx->webview = (WebKitWebView *) webkit_web_view_new_with_context(context);
	gtk_container_add((GtkContainer *) ctx->gtk_window, (GtkWidget *) ctx->webview);

	gtk_widget_set_app_paintable((GtkWidget *) ctx->gtk_window, true);
	webkit_web_view_set_background_color(ctx->webview, & (GdkRGBA) {0});

	WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(ctx->webview);
	g_signal_connect_data(manager, "script-message-received::external", (GCallback) handle_script_message, ctx, NULL, 0);
	webkit_user_content_manager_register_script_message_handler(manager, "external");

	gtk_widget_show_all((GtkWidget *) ctx->gtk_window);

	DISPATCH(mty_webview_dispatch_javascript_init, MTY_Strdup("window.external = { \
		invoke: function (s) { window.webkit.messageHandlers.external.postMessage(s); } \
	}"), true);

	mty_webview_destroy_event(&event);

	return false;
}

static bool mty_webview_dispatch_resize(struct mty_webview_event *event)
{
	struct webview *ctx = event->context;

	XWindowAttributes attr = {0};
	XGetWindowAttributes(ctx->display, ctx->window, &attr);

	float width = ctx->common.hidden ? 0 : attr.width;
	float height = ctx->common.hidden ? 0 : attr.height;

	GdkWindow *window = gtk_widget_get_window((GtkWidget *) ctx->gtk_window);

	if (width == 0 || height == 0) {
		gdk_window_hide(window);
		return false;
	}

	gdk_window_show(window);
	gtk_window_move(ctx->gtk_window, 0, 0);
	gtk_window_resize(ctx->gtk_window, lrint(width), lrint(height));

	mty_webview_destroy_event(&event);

	return false;
}

static void *mty_webview_thread_func(void *opaque)
{
	gtk_init_check(0, NULL);

	gtk_main();

	return NULL;
}

struct webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque)
{
	if (!webkit_global_init())
		return NULL;

	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	ctx->common.html = MTY_Strdup(html);
	ctx->common.debug = debug;
	ctx->common.event = event;
	ctx->common.opaque = opaque;

	ctx->display = (Display *) ((void **) handle)[0];
	ctx->window = (Window) ((void **) handle)[1];

	g_setenv("GDK_BACKEND", "x11", true);

	if (!thread)
		thread = MTY_ThreadCreate(mty_webview_thread_func, NULL);

	DISPATCH(mty_webview_add_new, NULL, false);
	DISPATCH(mty_webview_dispatch_resize, NULL, false);
	DISPATCH(mty_webview_enable_dev_tools, ctx->common.debug, false);
	DISPATCH(mty_webview_navigate, MTY_Strdup(ctx->common.html), true);

	return ctx;
}

static bool mty_webview_dispatch_destroy(struct mty_webview_event *event)
{
	struct webview *ctx = event->context;

	if (!ctx)
		return false;

	gtk_window_close(ctx->gtk_window);
	gtk_widget_destroy((GtkWidget *) ctx->webview);
	gtk_widget_destroy((GtkWidget *) ctx->gtk_window);
	gtk_main_quit();

	MTY_Free(ctx->common.html);

	MTY_Free(ctx);

	mty_webview_destroy_event(&event);

	return false;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;

	DISPATCH(mty_webview_dispatch_destroy, NULL, false);

	MTY_ThreadDestroy(&thread);
	*webview = NULL;
}

void mty_webview_resize(struct webview *ctx)
{
	if (!ctx)
		return;

	DISPATCH(mty_webview_dispatch_resize, NULL, false);
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

	DISPATCH(mty_webview_dispatch_javascript_eval, MTY_Strdup(javascript), true);

	MTY_Free(javascript);
}
