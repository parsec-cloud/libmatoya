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

struct focus_changed {
	ICoreWebView2FocusChangedEventHandler handler;
	MTY_Webview *context;
};

struct MTY_Webview {
	struct webview common;

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

void mty_webview_navigate(MTY_Webview *ctx)
{
	size_t size = strlen(ctx->common.html);
	size_t base64_size = ((4 * size / 3) + 3) & ~3;

	char *base64 = MTY_Alloc(base64_size + 1, 1);
	MTY_BytesToBase64(ctx->common.html, size, base64, base64_size);

	char *encoded = MTY_SprintfD("data:text/html;charset=UTF-8;base64, %s", base64);
	PostThreadMessage(ctx->thread_id, WV_NAVIGATE, 0, (LPARAM) MTY_MultiToWideD(encoded));
	MTY_Free(encoded);

	MTY_Free(base64);
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
	ICoreWebView2_AddWebResourceRequestedFilter(ctx->webview, L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

	// https://github.com/MicrosoftEdge/WebView2Feedback/issues/547
	// https://github.com/MicrosoftEdge/WebView2Feedback/issues/1907
	// https://github.com/MicrosoftEdge/WebView2Feedback/issues/468#issuecomment-1016993455

	mty_webview_resize(ctx);

	PostThreadMessage(ctx->thread_id, WV_SCRIPT_INIT, 0, (LPARAM) MTY_MultiToWideD("window.external = { \
		invoke: function (s) { window.chrome.webview.postMessage(s); } \
	}"));

	ICoreWebView2_get_Settings(ctx->webview, &ctx->settings);
	ICoreWebView2Settings_put_AreDevToolsEnabled(ctx->settings, ctx->common.debug);
	ICoreWebView2Settings_put_AreDefaultContextMenusEnabled(ctx->settings, ctx->common.debug);

	mty_webview_navigate(ctx);

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

static HRESULT STDMETHODCALLTYPE mty_webview_focus_changed(ICoreWebView2FocusChangedEventHandler *this, ICoreWebView2Controller *sender, IUnknown *args)
{
	MTY_Webview *ctx = ((struct focus_changed *) this)->context;

	ctx->common.has_focus = !ctx->common.has_focus;

	return 0;
}

// Private

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
				RECT bounds = {0};

				if (!ctx->common.hidden)
					GetClientRect(ctx->hwnd, &bounds);

				ICoreWebView2Controller3_put_BoundsMode(ctx->controller, COREWEBVIEW2_BOUNDS_MODE_USE_RAW_PIXELS);
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
			default:
				break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return NULL;
}

MTY_Webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque)
{
	MTY_Webview *ctx = MTY_Alloc(1, sizeof(MTY_Webview));

	mty_webview_create_common(ctx, html, debug, event, opaque);

	ctx->hwnd = (HWND) handle;

	ctx->environment_completed.context  = ctx;
	ctx->controller_completed.context   = ctx;
	ctx->web_message_received.context   = ctx;
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

	ctx->focus_changed.handler.lpVtbl = MTY_Alloc(1, sizeof(ICoreWebView2FocusChangedEventHandlerVtbl));
	*ctx->focus_changed.handler.lpVtbl = * (ICoreWebView2FocusChangedEventHandlerVtbl *) &common_vtbl;
	ctx->focus_changed.handler.lpVtbl->Invoke = mty_webview_focus_changed;

	ctx->thread = MTY_ThreadCreate(mty_webview_thread_func, ctx);

	return ctx;
}

void mty_webview_destroy(MTY_Webview **webview)
{
	if (!webview || !*webview)
		return;

	MTY_Webview *ctx = *webview;

	if (ctx->controller)
		ICoreWebView2Controller_Close(ctx->controller);

	if (ctx->thread) {
		PostThreadMessage(ctx->thread_id, WM_QUIT, 0, 0);
		MTY_ThreadDestroy(&ctx->thread);
	}

	MTY_Free(ctx->focus_changed.handler.lpVtbl);
	MTY_Free(ctx->web_message_received.handler.lpVtbl);
	MTY_Free(ctx->controller_completed.handler.lpVtbl);
	MTY_Free(ctx->environment_completed.handler.lpVtbl);

	mty_webview_destroy_common(ctx);

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_resize(MTY_Webview *ctx)
{
	if (!ctx || !ctx->controller)
		return;

	PostThreadMessage(ctx->thread_id, WV_SIZE, 0, 0);
}

void mty_webview_javascript_eval(MTY_Webview *ctx, const char *js)
{
	PostThreadMessage(ctx->thread_id, WV_SCRIPT_EVAL, 0, (LPARAM) MTY_MultiToWideD(js));
}
