#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <webkit2/webkit2.h>

#include "matoya.h"
#include "webview.h"

#define DISPATCH(func, param, should_free) g_idle_add(G_SOURCE_FUNC(func), mty_webview_create_event(ctx, (void *) (param), should_free))

MTY_Thread *thread = NULL;
uint32_t webviews = 0;

struct MTY_Webview {
	struct webview common;

	Display *display;
	Window window;
	GtkWindow *gtk_window;
	WebKitWebView *webview;
};

struct mty_webview_event {
	MTY_Webview *context;
	void *data;
	bool should_free;
};

static struct mty_webview_event *mty_webview_create_event(MTY_Webview *ctx, void *data, bool should_free)
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

MTY_Webview *mty_webview_create(uint32_t identifier, void *handle, void *opaque)
{
	MTY_Webview *ctx = MTY_Alloc(1, sizeof(MTY_Webview));

	mty_webview_create_common(ctx, identifier, opaque);

	ctx->display = (Display *) ((void **) handle)[0];
	ctx->window = (Window) ((void **) handle)[1];

	webviews++;

	return ctx;
}

static bool mty_webview_dispatch_destroy(struct mty_webview_event *event)
{
	MTY_Webview *ctx = event->context;

	if (!ctx)
		return false;

	gtk_window_close(ctx->gtk_window);
	gtk_widget_destroy(GTK_WIDGET(ctx->webview));
	gtk_widget_destroy(GTK_WIDGET(ctx->gtk_window));

	if (!webviews)
		gtk_main_quit();

	MTY_Free(ctx);

	mty_webview_destroy_event(&event);

	return false;
}

void mty_webview_destroy(MTY_Webview *ctx)
{
	webviews--;

	DISPATCH(mty_webview_dispatch_destroy, NULL, false);

	if (!webviews)
		MTY_ThreadDestroy(&thread);
}

static bool mty_webview_dispatch_resize(struct mty_webview_event *event)
{
	MTY_Webview *ctx = event->context;

	float x = ctx->common.bounds.left;
	float y = ctx->common.bounds.top;
	float width = ctx->common.bounds.right;
	float height = ctx->common.bounds.bottom;

	if (ctx->common.auto_size) {
		XWindowAttributes attr = {0};
		XGetWindowAttributes(ctx->display, ctx->window, &attr);

		x = 0;
		y = 0;
		width = attr.width;
		height = attr.height;
	}

	GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(ctx->gtk_window));

	if (width == 0 || height == 0) {
		gdk_window_hide(window);
		return false;
	}

	gdk_window_show(window);
	gtk_window_move(ctx->gtk_window, lrint(x), lrint(y));
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

static void mty_webview_handle_request(WebKitURISchemeRequest *request, void *opaque)
{
	MTY_Webview *ctx = opaque;

	size_t size = 0;
	const char *mime = NULL;
	const char *uri = webkit_uri_scheme_request_get_uri(request);
	const void *res = MTY_WebviewGetResource(ctx, uri, &size, &mime);

	if (!res) {
		GError error = { .code = 404, .message = "Not Found" };
		webkit_uri_scheme_request_finish_error(request, &error);
		return;
	}

	SoupMessageHeaders *headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE);
	soup_message_headers_append(headers, "Access-Control-Allow-Origin", "*");

	GInputStream *stream = g_memory_input_stream_new_from_data(res, size, NULL);

	WebKitURISchemeResponse *response = webkit_uri_scheme_response_new(stream, size);
 	webkit_uri_scheme_response_set_content_type(response, MTY_MIMEGetType(uri));
	webkit_uri_scheme_response_set_status(response, 200, "OK");
	webkit_uri_scheme_response_set_http_headers(response, headers);

	webkit_uri_scheme_request_finish_with_response(request, response);

	g_object_unref(response);
	g_object_unref(stream);
}

static void handle_script_message(WebKitUserContentManager *manager, WebKitJavascriptResult *result, void *opaque)
{
	MTY_Webview *ctx = opaque;

	JSCValue *value = webkit_javascript_result_get_js_value(result);
	char *message = jsc_value_to_string(value);
	mty_webview_handle_event(ctx, message);
}

static bool mty_webview_add_new(struct mty_webview_event *event)
{
	MTY_Webview *ctx = event->context;

	ctx->gtk_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_POPUP));
	gtk_widget_realize(GTK_WIDGET(ctx->gtk_window));

	GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(ctx->gtk_window));
	XReparentWindow(GDK_WINDOW_XDISPLAY(gdk_window), GDK_WINDOW_XID(gdk_window), ctx->window, 0, 0);

	WebKitWebContext *context = webkit_web_context_new();
	if (ctx->common.scheme)
		webkit_web_context_register_uri_scheme(context, ctx->common.scheme, mty_webview_handle_request, ctx, NULL);

	ctx->webview = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(context));
	gtk_container_add(GTK_CONTAINER(ctx->gtk_window), GTK_WIDGET(ctx->webview));

	gtk_widget_set_app_paintable(GTK_WIDGET(ctx->gtk_window), TRUE);
	webkit_web_view_set_background_color(ctx->webview, & (GdkRGBA) {0});

	WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(ctx->webview);
	g_signal_connect(manager, "script-message-received::external", G_CALLBACK(handle_script_message), ctx);
	webkit_user_content_manager_register_script_message_handler(manager, "external");

	gtk_widget_show_all(GTK_WIDGET(ctx->gtk_window));

	MTY_WebviewJavascriptInit(ctx, "window.external = { \
		invoke: function (s) { window.webkit.messageHandlers.external.postMessage(s); } \
	}");

	mty_webview_destroy_event(&event);

	return false;
}

void MTY_WebviewShow(MTY_Webview *ctx, MTY_WebviewOnCreated callback)
{
	g_setenv("GDK_BACKEND", "x11", true);

	if (!thread)
		thread = MTY_ThreadCreate(mty_webview_thread_func, NULL);

	DISPATCH(mty_webview_add_new, NULL, false);
	DISPATCH(mty_webview_dispatch_resize, NULL, false);

	callback(ctx, ctx->common.opaque);
}

static bool mty_webview_enable_dev_tools(struct mty_webview_event *event)
{
	WebKitSettings *settings = webkit_web_view_get_settings(event->context->webview);
	webkit_settings_set_enable_developer_extras(settings, (bool) event->data);

	mty_webview_destroy_event(&event);

	return false;
}

void MTY_WebviewOpenDevTools(MTY_Webview *ctx)
{
	// XXX Can't be done programmatically
}

static bool mty_webview_navigate_url(struct mty_webview_event *event)
{
	webkit_web_view_load_uri(event->context->webview, event->data);

	mty_webview_destroy_event(&event);

	return false;
}

static bool mty_webview_navigate_html(struct mty_webview_event *event)
{
	webkit_web_view_load_html(event->context->webview, event->data, NULL);

	mty_webview_destroy_event(&event);

	return false;
}

static bool mty_webview_javascript_init(struct mty_webview_event *event)
{
	WebKitUserContentInjectedFrames injected_frames = WEBKIT_USER_CONTENT_INJECT_TOP_FRAME;
	WebKitUserScriptInjectionTime injection_time = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START;

	WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(event->context->webview);
	WebKitUserScript *script = webkit_user_script_new(event->data, injected_frames, injection_time, NULL, NULL);
	webkit_user_content_manager_add_script(manager, script);

	mty_webview_destroy_event(&event);

	return false;
}

static bool mty_webview_javascript_eval(struct mty_webview_event *event)
{
	webkit_web_view_run_javascript(event->context->webview, event->data, NULL, NULL, NULL);

	mty_webview_destroy_event(&event);

	return false;
}

void mty_webview_resize(MTY_Webview *ctx)
{
	DISPATCH(mty_webview_dispatch_resize, NULL, false);
}

void MTY_WebviewEnableDevTools(MTY_Webview *ctx, bool enable)
{
	DISPATCH(mty_webview_enable_dev_tools, enable, false);
}

void MTY_WebviewNavigateURL(MTY_Webview *ctx, const char *url)
{
	DISPATCH(mty_webview_navigate_url, MTY_Strdup(url), true);
}

void MTY_WebviewNavigateHTML(MTY_Webview *ctx, const char *html)
{
	DISPATCH(mty_webview_navigate_html, MTY_Strdup(html), true);
}

void MTY_WebviewJavascriptInit(MTY_Webview *ctx, const char *js)
{
	DISPATCH(mty_webview_javascript_init, MTY_Strdup(js), true);
}

void MTY_WebviewJavascriptEval(MTY_Webview *ctx, const char *js)
{
	DISPATCH(mty_webview_javascript_eval, MTY_Strdup(js), true);
}
