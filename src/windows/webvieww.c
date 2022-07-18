#include "matoya.h"
#include "webview.h"

#define COBJMACROS
#include <windows.h>
#include <shlwapi.h>
#include <ole2.h>
#include <math.h>

#include "webview2.h"

#define WV_SIZE             (WM_APP + 1)
#define WV_SCRIPT_INIT      (WM_APP + 2)
#define WV_SCRIPT_EVAL      (WM_APP + 3)
#define WV_NAVIGATE         (WM_APP + 4)
#define WV_DEV_TOOLS_ENABLE (WM_APP + 5)
#define WV_DEV_TOOLS_OPEN   (WM_APP + 6)

struct environment_completed {
	ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler handler;
	MTY_Webview *context;
};

struct controller_completed {
	ICoreWebView2CreateCoreWebView2ControllerCompletedHandler handler;
	MTY_Webview *context;
};

struct web_message_received {
	ICoreWebView2WebMessageReceivedEventHandler handler;
	MTY_Webview *context;
};

struct web_resource_requested {
	ICoreWebView2WebResourceRequestedEventHandler handler;
	MTY_Webview *context;
};

struct focus_changed {
	ICoreWebView2FocusChangedEventHandler handler;
	MTY_Webview *context;
};

struct MTY_Webview {
	struct webview common;
	
	MTY_WebviewOnCreated on_created;
	
	HWND hwnd;
	MTY_Thread *thread;
	DWORD thread_id;

	ICoreWebView2 *webview;
	ICoreWebView2Environment *environment;
	ICoreWebView2Controller3 *controller;
	ICoreWebView2Settings *settings;

	struct environment_completed environment_completed;
	struct controller_completed controller_completed;
	struct web_message_received web_message_received;
	struct web_resource_requested web_resource_requested;
	struct focus_changed focus_changed;
};

// Implementation

static HRESULT STDMETHODCALLTYPE mty_webview_query_interface(IUnknown *this, REFIID riid, void **ppvObject)
{
	*ppvObject = NULL;

	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE mty_webview_add_ref(IUnknown *this)
{
	return 1;
}

static ULONG STDMETHODCALLTYPE mty_webview_release(IUnknown *this)
{
	return 0;
}

static HRESULT STDMETHODCALLTYPE mty_webview_environment_completed(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *this, HRESULT code, ICoreWebView2Environment *env)
{
	MTY_Webview *ctx = ((struct environment_completed *) this)->context;

	ctx->environment = env;

	return ICoreWebView2Environment3_CreateCoreWebView2Controller(env, ctx->hwnd, &ctx->controller_completed.handler);
}

static HRESULT STDMETHODCALLTYPE mty_webview_controller_completed(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *this, HRESULT code, ICoreWebView2Controller *controller)
{
	MTY_Webview *ctx = ((struct controller_completed *) this)->context;

	ctx->controller = (ICoreWebView2Controller3 *) controller;
	ICoreWebView2Controller_AddRef(ctx->controller);

	ICoreWebView2Controller_get_CoreWebView2(ctx->controller, &ctx->webview);
	ICoreWebView2_AddRef(ctx->webview);

	ICoreWebView2Controller2_put_DefaultBackgroundColor(ctx->controller, (COREWEBVIEW2_COLOR) {0});

	ICoreWebView2Controller_add_GotFocus(ctx->controller, &ctx->focus_changed.handler, NULL);
	ICoreWebView2Controller_add_LostFocus(ctx->controller, &ctx->focus_changed.handler, NULL);
	ICoreWebView2_add_WebMessageReceived(ctx->webview, &ctx->web_message_received.handler, NULL);
	ICoreWebView2_add_WebResourceRequested(ctx->webview, &ctx->web_resource_requested.handler, NULL);
	ICoreWebView2_AddWebResourceRequestedFilter(ctx->webview, L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

	// https://github.com/MicrosoftEdge/WebView2Feedback/issues/547
	// https://github.com/MicrosoftEdge/WebView2Feedback/issues/1907
	// https://github.com/MicrosoftEdge/WebView2Feedback/issues/468#issuecomment-1016993455

	mty_webview_resize(ctx);

	MTY_WebviewEnableDevTools(ctx, false);
	MTY_WebviewJavascriptInit(ctx, "window.external = { \
		invoke: function (s) { window.chrome.webview.postMessage(s); } \
	}");

	ctx->on_created(ctx, ctx->common.opaque);

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mty_webview_message_received(ICoreWebView2WebMessageReceivedEventHandler *this, ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args)
{
	MTY_Webview *ctx = ((struct web_message_received *) this)->context;

	wchar_t *wide_message = NULL;
	ICoreWebView2WebMessageReceivedEventArgs_TryGetWebMessageAsString(args, &wide_message);

	char *message = MTY_WideToMultiD(wide_message);

	mty_webview_handle_event(ctx, message);

	MTY_Free(message);

	return 0;
}

static HRESULT STDMETHODCALLTYPE mty_webview_resource_requested(ICoreWebView2WebResourceRequestedEventHandler *this, ICoreWebView2 *sender, ICoreWebView2WebResourceRequestedEventArgs *args)
{
	MTY_Webview *ctx = ((struct web_resource_requested *) this)->context;

	ICoreWebView2WebResourceRequest *request = NULL;
	ICoreWebView2WebResourceRequestedEventArgs_get_Request(args, &request);

	wchar_t *url_w = NULL;
	ICoreWebView2WebResourceRequest_get_Uri(request, &url_w);

	size_t size = 0;
	const char *mime = NULL;
	char *url = MTY_WideToMultiD(url_w);
	const void *resource = MTY_WebviewGetResource(ctx, url, &size, &mime);
	MTY_Free(url);

	if (!resource)
		return S_OK;

	char *header = MTY_SprintfD("Content-Type: %s", mime);
	wchar_t *header_w = MTY_MultiToWideD(header);

	ICoreWebView2WebResourceResponse *response = NULL;
	IStream *stream = SHCreateMemStream(resource, (UINT) size);
	ICoreWebView2Environment_CreateWebResourceResponse(ctx->environment, stream, 200, L"OK", header_w, &response);
	ICoreWebView2WebResourceRequestedEventArgs_put_Response(args, response);

	MTY_Free(header_w);
	MTY_Free(header);

	ICoreWebView2WebResourceResponse_Release(response);
	IStream_Release(stream);

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mty_webview_focus_changed(ICoreWebView2FocusChangedEventHandler *this, ICoreWebView2Controller *sender, IUnknown *args)
{
	MTY_Webview *ctx = ((struct focus_changed *) this)->context;

	ctx->common.has_focus = !ctx->common.has_focus;

	return 0;
}

// Private

MTY_Webview *mty_webview_create(uint32_t identifier, void *handle, void *opaque)
{
	MTY_Webview *ctx = MTY_Alloc(1, sizeof(MTY_Webview));

	mty_webview_create_common(ctx, identifier, opaque);
	
	ctx->hwnd = (HWND) handle;

	ctx->environment_completed.context  = ctx;
	ctx->controller_completed.context   = ctx;
	ctx->web_message_received.context   = ctx;
	ctx->web_resource_requested.context = ctx;
	ctx->focus_changed.context          = ctx;

	struct IUnknownVtbl common_vtbl = {0};
	common_vtbl.QueryInterface = mty_webview_query_interface;
	common_vtbl.AddRef         = mty_webview_add_ref;
	common_vtbl.Release        = mty_webview_release;

	ctx->environment_completed.handler.lpVtbl = MTY_Alloc(1, sizeof(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl));
	*ctx->environment_completed.handler.lpVtbl = * (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl *) &common_vtbl;
	ctx->environment_completed.handler.lpVtbl->Invoke = mty_webview_environment_completed;

	ctx->controller_completed.handler.lpVtbl = MTY_Alloc(1, sizeof(ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl));
	*ctx->controller_completed.handler.lpVtbl = * (ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl *) &common_vtbl;
	ctx->controller_completed.handler.lpVtbl->Invoke = mty_webview_controller_completed;

	ctx->web_message_received.handler.lpVtbl = MTY_Alloc(1, sizeof(ICoreWebView2WebMessageReceivedEventHandlerVtbl));
	*ctx->web_message_received.handler.lpVtbl = * (ICoreWebView2WebMessageReceivedEventHandlerVtbl *) &common_vtbl;
	ctx->web_message_received.handler.lpVtbl->Invoke = mty_webview_message_received;

	ctx->web_resource_requested.handler.lpVtbl = MTY_Alloc(1, sizeof(ICoreWebView2WebResourceRequestedEventHandlerVtbl));
	*ctx->web_resource_requested.handler.lpVtbl = * (ICoreWebView2WebResourceRequestedEventHandlerVtbl *) &common_vtbl;
	ctx->web_resource_requested.handler.lpVtbl->Invoke = mty_webview_resource_requested;

	ctx->focus_changed.handler.lpVtbl = MTY_Alloc(1, sizeof(ICoreWebView2FocusChangedEventHandlerVtbl));
	*ctx->focus_changed.handler.lpVtbl = * (ICoreWebView2FocusChangedEventHandlerVtbl *) &common_vtbl;
	ctx->focus_changed.handler.lpVtbl->Invoke = mty_webview_focus_changed;

	return ctx;
}

void mty_webview_destroy(MTY_Webview *ctx)
{
	if (!ctx)
		return;

	if (ctx->controller)
		ICoreWebView2Controller_Close(ctx->controller);

	if (ctx->thread) {
		PostThreadMessage(ctx->thread_id, WM_QUIT, 0, 0);
		MTY_ThreadDestroy(&ctx->thread);
	}

	if (ctx->focus_changed.handler.lpVtbl)
		MTY_Free(ctx->focus_changed.handler.lpVtbl);

	if (ctx->web_resource_requested.handler.lpVtbl)
		MTY_Free(ctx->web_resource_requested.handler.lpVtbl);

	if (ctx->web_message_received.handler.lpVtbl)
		MTY_Free(ctx->web_message_received.handler.lpVtbl);

	if (ctx->controller_completed.handler.lpVtbl)
		MTY_Free(ctx->controller_completed.handler.lpVtbl);

	if (ctx->environment_completed.handler.lpVtbl)
		MTY_Free(ctx->environment_completed.handler.lpVtbl);

	mty_webview_destroy_common(ctx);

	MTY_Free(ctx);
}

void mty_webview_resize(MTY_Webview *ctx)
{
	if (!ctx || !ctx->controller)
		return;

	PostThreadMessage(ctx->thread_id, WV_SIZE, 0, 0);
}

static void *mty_webview_thread_func(void *opaque)
{
	MTY_Webview *ctx = opaque;
	ctx->thread_id = GetCurrentThreadId();

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	SetEnvironmentVariable(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", L"--disable-web-security");
	CreateCoreWebView2EnvironmentWithOptions(NULL, L"C:\\Windows\\Temp\\webview-data", NULL, &ctx->environment_completed.handler);

	MSG msg = {0};
	while (GetMessage(&msg, NULL, 0, 0)) {
		switch (msg.message) {
			case WV_SIZE: {
				RECT bounds = {
					lrint(ctx->common.bounds.left),
					lrint(ctx->common.bounds.top),
					lrint(ctx->common.bounds.left + ctx->common.bounds.right),
					lrint(ctx->common.bounds.top  + ctx->common.bounds.bottom),
				};

				if (ctx->common.auto_size)
					GetClientRect(ctx->hwnd, &bounds);

				ICoreWebView2Controller3_put_BoundsMode(ctx->controller, ctx->common.auto_size
					? COREWEBVIEW2_BOUNDS_MODE_USE_RAW_PIXELS
					: COREWEBVIEW2_BOUNDS_MODE_USE_RASTERIZATION_SCALE);
				ICoreWebView2Controller_put_Bounds(ctx->controller, bounds);
				break;
			}
			case WV_SCRIPT_INIT:
				ICoreWebView2_AddScriptToExecuteOnDocumentCreated(ctx->webview, (wchar_t *) msg.lParam, NULL);
				MTY_Free((void *) msg.lParam);
				break;
			case WV_SCRIPT_EVAL:
				ICoreWebView2_ExecuteScript(ctx->webview, (wchar_t *) msg.lParam, NULL);
				MTY_Free((void *) msg.lParam);
				break;
			case WV_NAVIGATE:
				ICoreWebView2_Navigate(ctx->webview, (wchar_t *) msg.lParam);
				MTY_Free((void *) msg.lParam);
				break;
			case WV_DEV_TOOLS_ENABLE:
				ICoreWebView2_get_Settings(ctx->webview, &ctx->settings);
				ICoreWebView2Settings_put_AreDevToolsEnabled(ctx->settings, (bool) msg.lParam);
				ICoreWebView2Settings_put_AreDefaultContextMenusEnabled(ctx->settings, (bool) msg.lParam);
				break;
			case WV_DEV_TOOLS_OPEN:
				ICoreWebView2_OpenDevToolsWindow(ctx->webview);
				break;
			default:
				break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return NULL;
}

// Public

void MTY_WebviewShow(MTY_Webview *ctx, MTY_WebviewOnCreated callback)
{
	ctx->on_created = callback;

	ctx->thread = MTY_ThreadCreate(mty_webview_thread_func, ctx);
}

void MTY_WebviewEnableDevTools(MTY_Webview *ctx, bool enable)
{
	PostThreadMessage(ctx->thread_id, WV_DEV_TOOLS_ENABLE, 0, (LPARAM) enable);
}

void MTY_WebviewOpenDevTools(MTY_Webview *ctx)
{
	PostThreadMessage(ctx->thread_id, WV_DEV_TOOLS_OPEN, 0, 0);
}

void MTY_WebviewNavigateURL(MTY_Webview *ctx, const char *url)
{
	PostThreadMessage(ctx->thread_id, WV_NAVIGATE, 0, (LPARAM) MTY_MultiToWideD(url));
}

void MTY_WebviewNavigateHTML(MTY_Webview *ctx, const char *html)
{
	size_t size = strlen(html);
	size_t base64_size = ((4 * size / 3) + 3) & ~3;

	char *base64 = MTY_Alloc(base64_size + 1, 1);
	MTY_BytesToBase64(html, size, base64, base64_size);

	char *encoded = MTY_SprintfD("data:text/html;charset=UTF-8;base64, %s", base64);
	MTY_WebviewNavigateURL(ctx, encoded);

	MTY_Free(encoded);
	MTY_Free(base64);
}

void MTY_WebviewJavascriptInit(MTY_Webview *ctx, const char *js)
{
	PostThreadMessage(ctx->thread_id, WV_SCRIPT_INIT, 0, (LPARAM) MTY_MultiToWideD(js));
}

void MTY_WebviewJavascriptEval(MTY_Webview *ctx, const char *js)
{
	PostThreadMessage(ctx->thread_id, WV_SCRIPT_EVAL, 0, (LPARAM) MTY_MultiToWideD(js));
}
