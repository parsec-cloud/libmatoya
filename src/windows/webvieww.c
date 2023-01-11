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
	struct webview *context;
};

struct controller_completed {
	ICoreWebView2CreateCoreWebView2ControllerCompletedHandler handler;
	struct webview *context;
};

struct dom_load_completed {
	ICoreWebView2DOMContentLoadedEventHandler handler;
	struct webview *context;
};

struct web_message_received {
	ICoreWebView2WebMessageReceivedEventHandler handler;
	struct webview *context;
};

struct web_resource_requested {
	ICoreWebView2WebResourceRequestedEventHandler handler;
	struct webview *context;
};

struct focus_changed {
	ICoreWebView2FocusChangedEventHandler handler;
	struct webview *context;
};

struct webview {
	struct webview_common common;

	bool has_focus;

	HWND hwnd;
	MTY_Thread *thread;
	DWORD thread_id;

	HANDLE event_webview_ready;

	ICoreWebView2 *webview;
	ICoreWebView2_2 *webview2;
	ICoreWebView2Environment *environment;
	ICoreWebView2Controller3 *controller;
	ICoreWebView2Settings *settings;

	struct environment_completed environment_completed;
	struct controller_completed controller_completed;
	struct dom_load_completed dom_load_completed;
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
	struct webview *ctx = ((struct environment_completed *) this)->context;

	ctx->environment = env;

	return ICoreWebView2Environment3_CreateCoreWebView2Controller(env, ctx->hwnd, &ctx->controller_completed.handler);
}

static HRESULT STDMETHODCALLTYPE mty_webview_controller_completed(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *this, HRESULT code, ICoreWebView2Controller *controller)
{
	struct webview *ctx = ((struct controller_completed *) this)->context;

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

	ICoreWebView2_QueryInterface(ctx->webview, &IID_ICoreWebView2_2, &ctx->webview2);
	if (ctx->webview2) {
		ICoreWebView2_2_add_DOMContentLoaded(ctx->webview2, &ctx->dom_load_completed.handler, NULL);
		ICoreWebView2_2_Release(ctx->webview2);
	}

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
	ICoreWebView2Settings_put_IsZoomControlEnabled(ctx->settings, FALSE);

	// TODO The whole request inerception thing should be removed when the 2MB limitation on Navigate and NavigateToString is gone.
	// Tracking issue: https://github.com/MicrosoftEdge/WebView2Feedback/issues/1355
	const char *bootstrap =
		"<html>"
		"    <head>"
		"        <script>"
		"            const currentScript = document.currentScript;"
		"            fetch('https://app.local/index.html')"
		"                .then(res => res.text())"
		"                .then(res => {"
		"                    const html = document.createElement('html');"
		"                    html.innerHTML = res;"
		""
		"                    const head = html.querySelector('head');"
		"                    const body = html.querySelector('body');"
		""
		"                    for (const child of head.children) document.head.append(child);"
		"                    for (const child of body.children) document.body.append(child);"
		""
		"                    for (const child of head.querySelectorAll('script')) {"
		"                        const script = document.createElement('script');"
		"                        script.src = child.src ? child.src : URL.createObjectURL(new Blob([child.innerHTML]));"
		"                        document.head.appendChild(script);"
		"                    }"
		""
		"                    for (const child of body.querySelectorAll('script')) {"
		"                        const script = document.createElement('script');"
		"                        script.src = child.src ? child.src : URL.createObjectURL(new Blob([child.innerHTML]));"
		"                        document.body.appendChild(script);"
		"                    }"
		""
		"                    currentScript.remove();"
		"                });"
		"        </script>"
		"    </head>"
		"</html>";
	PostThreadMessage(ctx->thread_id, WV_NAVIGATE, 0, (LPARAM) MTY_MultiToWideD(bootstrap));

	SetEvent(ctx->event_webview_ready);

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mty_webview_dom_load_completed(ICoreWebView2DOMContentLoadedEventHandler *this, ICoreWebView2 *sender, ICoreWebView2DOMContentLoadedEventArgs *args)
{
	struct webview *ctx = ((struct dom_load_completed *) this)->context;

	// TODO: This is still not enough? GRR!! I don't really like the webview_event("init") approach, so think this through some more....there HAS to be a callback called AFTER we have begun listening for events!
	mty_webview_event(ctx, "parsec_config", "{ data: 123, datum: \"you know me\"}");

	return S_OK;
}

static void mty_webview_handle_event(struct webview *ctx, const char *message)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_WEBVIEW;
	evt.message = message;

	ctx->common.event(&evt, ctx->common.opaque);
}

static HRESULT STDMETHODCALLTYPE mty_webview_message_received(ICoreWebView2WebMessageReceivedEventHandler *this, ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args)
{
	struct webview *ctx = ((struct web_message_received *) this)->context;

	wchar_t *wide_message = NULL;
	ICoreWebView2WebMessageReceivedEventArgs_TryGetWebMessageAsString(args, &wide_message);

	char *message = MTY_WideToMultiD(wide_message);

	mty_webview_handle_event(ctx, message);

	MTY_Free(message);

	return 0;
}

static HRESULT STDMETHODCALLTYPE mty_webview_resource_requested(ICoreWebView2WebResourceRequestedEventHandler *this, ICoreWebView2 *sender, ICoreWebView2WebResourceRequestedEventArgs *args)
{
	struct webview *ctx = ((struct web_resource_requested *) this)->context;

	ICoreWebView2WebResourceRequest *request = NULL;
	ICoreWebView2WebResourceRequestedEventArgs_get_Request(args, &request);

	wchar_t *url_w = NULL;
	ICoreWebView2WebResourceRequest_get_Uri(request, &url_w);

	if (strcmp("https://app.local/index.html", MTY_WideToMultiDL(url_w)))
		return S_OK;

	const wchar_t *headers = 
		L"Content-Type: text/html\n"
		L"Access-Control-Allow-Origin: *\n";

	ICoreWebView2WebResourceResponse *response = NULL;
	IStream *stream = SHCreateMemStream((const BYTE *) ctx->common.html, (UINT) strlen(ctx->common.html));
	ICoreWebView2Environment_CreateWebResourceResponse(ctx->environment, stream, 200, L"OK", headers, &response);
	ICoreWebView2WebResourceRequestedEventArgs_put_Response(args, response);

	ICoreWebView2WebResourceResponse_Release(response);
	IStream_Release(stream);

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mty_webview_focus_changed(ICoreWebView2FocusChangedEventHandler *this, ICoreWebView2Controller *sender, IUnknown *args)
{
	struct webview *ctx = ((struct focus_changed *) this)->context;

	ctx->has_focus = !ctx->has_focus;

	return 0;
}

// Private

static void mty_webview_thread_handle_msg(struct webview *ctx, MSG *msg)
{
	char hr_str[1024] = {0};
	HRESULT hr = S_OK;

	switch (msg->message) {
		case WV_SIZE: {
			RECT bounds = {0};

			if (!ctx->common.hidden)
				GetClientRect(ctx->hwnd, &bounds);

			hr = ICoreWebView2Controller3_put_BoundsMode(ctx->controller, COREWEBVIEW2_BOUNDS_MODE_USE_RAW_PIXELS);
			if (hr == S_OK)
				hr = ICoreWebView2Controller_put_Bounds(ctx->controller, bounds);
			break;
		}
		case WV_SCRIPT_INIT:
			hr = ICoreWebView2_AddScriptToExecuteOnDocumentCreated(ctx->webview, (wchar_t *) msg->lParam, NULL);
			MTY_Free((void *) msg->lParam);
			break;
		case WV_SCRIPT_EVAL:
			hr = ICoreWebView2_ExecuteScript(ctx->webview, (wchar_t *) msg->lParam, NULL);
			MTY_Free((void *) msg->lParam);
			break;
		case WV_NAVIGATE:
			hr = ICoreWebView2_NavigateToString(ctx->webview, (wchar_t *) msg->lParam);
			MTY_Free((void *) msg->lParam);
			break;
		default:
			break;
	}

	if (FAILED(hr)) {
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, hr, 0, hr_str, sizeof(hr_str), NULL);
		MTY_Log("[MTY_Webview] Failed to invoke ICoreWebView2 function. Reason: %s", hr_str);
	}
}

static void *mty_webview_thread_func(void *opaque)
{
	struct webview *ctx = opaque;
	ctx->thread_id = GetCurrentThreadId();

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	// Ensure the thread message loop is initialized before we create the webview
	MSG msg = {0};
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE); // see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postthreadmessagea#remarks
	
	CreateCoreWebView2EnvironmentWithOptions(NULL, L"C:\\Windows\\Temp\\webview-data", NULL, &ctx->environment_completed.handler);

	MTY_List *early_msg_q = MTY_ListCreate();

	bool running = true;
	while (running) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (LOWORD(msg.message) == WM_QUIT) {
				running = false;
				break;
			}

			if (WaitForSingleObject(ctx->event_webview_ready, 0) != WAIT_OBJECT_0) {
				MSG *p = MTY_Alloc(1, sizeof(MSG));
				*p = msg;
				MTY_ListAppend(early_msg_q, p);

			} else {
				mty_webview_thread_handle_msg(ctx, &msg);
			}

			TranslateMessage(&msg); // XXX: We MAY not need this, depending on whether webview requires 
								// WM_CHAR messages or can handle WK_KEY* messages alone.
								// Re-visit this later and remove this call if we can.
			DispatchMessage(&msg);
		}

		for (MTY_ListNode *n = MTY_ListGetFirst(early_msg_q); n; /* nothing */) {
			MTY_ListNode *next = n->next;

			mty_webview_thread_handle_msg(ctx, (MSG *) n->value);

			MTY_Free(n->value);
			MTY_ListRemove(early_msg_q, n);

			n = next;
		}

		if (running && MTY_ListGetFirst(early_msg_q) == NULL)
			WaitMessage(); // blocks until a message is enqueued, effectively replicating the behaviour of GetMessage
	}

	MTY_ListDestroy(&early_msg_q, MTY_Free);

	return NULL;
}

struct webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	ctx->common.html = MTY_Strdup(html);
	ctx->common.debug = debug;
	ctx->common.event = event;
	ctx->common.opaque = opaque;

	ctx->hwnd = (HWND) handle;

	ctx->environment_completed.context  = ctx;
	ctx->controller_completed.context   = ctx;
	ctx->dom_load_completed.context     = ctx;
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

	ctx->dom_load_completed.handler.lpVtbl = MTY_Alloc(1, sizeof(ICoreWebView2DOMContentLoadedEventHandlerVtbl));
	*ctx->dom_load_completed.handler.lpVtbl = * (ICoreWebView2DOMContentLoadedEventHandlerVtbl *) &common_vtbl;
	ctx->dom_load_completed.handler.lpVtbl->Invoke = mty_webview_dom_load_completed;

	ctx->web_message_received.handler.lpVtbl = MTY_Alloc(1, sizeof(ICoreWebView2WebMessageReceivedEventHandlerVtbl));
	*ctx->web_message_received.handler.lpVtbl = * (ICoreWebView2WebMessageReceivedEventHandlerVtbl *) &common_vtbl;
	ctx->web_message_received.handler.lpVtbl->Invoke = mty_webview_message_received;

	ctx->web_resource_requested.handler.lpVtbl = MTY_Alloc(1, sizeof(ICoreWebView2WebResourceRequestedEventHandlerVtbl));
	*ctx->web_resource_requested.handler.lpVtbl = * (ICoreWebView2WebResourceRequestedEventHandlerVtbl *) &common_vtbl;
	ctx->web_resource_requested.handler.lpVtbl->Invoke = mty_webview_resource_requested;

	ctx->focus_changed.handler.lpVtbl = MTY_Alloc(1, sizeof(ICoreWebView2FocusChangedEventHandlerVtbl));
	*ctx->focus_changed.handler.lpVtbl = * (ICoreWebView2FocusChangedEventHandlerVtbl *) &common_vtbl;
	ctx->focus_changed.handler.lpVtbl->Invoke = mty_webview_focus_changed;

	ctx->thread = MTY_ThreadCreate(mty_webview_thread_func, ctx);

	ctx->event_webview_ready = CreateEvent(NULL, TRUE, FALSE, NULL);

	return ctx;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;

	CloseHandle(ctx->event_webview_ready);

	if (ctx->controller)
		ICoreWebView2Controller_Close(ctx->controller);

	if (ctx->thread) {
		PostThreadMessage(ctx->thread_id, WM_QUIT, 0, 0);
		MTY_ThreadDestroy(&ctx->thread);
	}

	MTY_Free(ctx->focus_changed.handler.lpVtbl);
	MTY_Free(ctx->web_resource_requested.handler.lpVtbl);
	MTY_Free(ctx->web_message_received.handler.lpVtbl);
	MTY_Free(ctx->dom_load_completed.handler.lpVtbl);
	MTY_Free(ctx->controller_completed.handler.lpVtbl);
	MTY_Free(ctx->environment_completed.handler.lpVtbl);

	MTY_Free(ctx->common.html);

	MTY_Free(ctx);
	*webview = NULL;
}

bool mty_webview_has_focus(struct webview *ctx)
{
	return ctx && ctx->has_focus;
}

void mty_webview_resize(struct webview *ctx)
{
	if (!ctx || !ctx->controller)
		return;

	PostThreadMessage(ctx->thread_id, WV_SIZE, 0, 0);
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

	WaitForSingleObject(ctx->event_webview_ready, INFINITE);

	PostThreadMessage(ctx->thread_id, WV_SCRIPT_EVAL, 0, (LPARAM) MTY_MultiToWideD(javascript));

	MTY_Free(javascript);
}
