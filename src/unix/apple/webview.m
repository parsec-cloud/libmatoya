// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "webview.h"

#include <WebKit/WebKit.h>

#include "objc.h"

struct webview {
	struct webview_base base;

	WKWebView *webview;
	MTY_Time ts;
};


// Class: Webview

static void webview_noResponderFor(id self, SEL _cmd, SEL eventSelector)
{
}

static Class webview_class(void)
{
	Class cls = objc_getClass(WEBVIEW_CLASS_NAME);
	if (cls)
		return cls;

	cls = OBJC_ALLOCATE("WKWebView", WEBVIEW_CLASS_NAME);

	// Overrides
	OBJC_OVERRIDE(cls, @selector(noResponderFor:), webview_noResponderFor);

	objc_registerClassPair(cls);

	return cls;
}


// Class: MsgHandler

static void msg_handler_userContentController_didReceiveScriptMessage(id self, SEL _cmd,
	WKUserContentController *userContentController, WKScriptMessage *message)
{
	struct webview *ctx = OBJC_CTX();

	const char *str = [message.body UTF8String];
	mty_webview_base_handle_event(&ctx->base, str);
}

static Class msg_handler_class(void)
{
	Class cls = objc_getClass(MSG_HANDLER_CLASS_NAME);
	if (cls)
		return cls;

	cls = OBJC_ALLOCATE("NSObject", MSG_HANDLER_CLASS_NAME);

	// WKScriptMessageHandler
	Protocol *proto = OBJC_PROTOCOL(cls, @protocol(WKScriptMessageHandler));
	if (proto)
		OBJC_POVERRIDE(cls, proto, YES, @selector(userContentController:didReceiveScriptMessage:),
			msg_handler_userContentController_didReceiveScriptMessage);

	objc_registerClassPair(cls);

	return cls;
}

static bool mty_webview_supported(void)
{
	if (@available(macos 11.0, *))
		return true;

	return false;
}


// Public

struct webview *mty_webview_create(MTY_App *app, MTY_Window window, const char *dir,
	bool debug, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func, WEBVIEW_KEY key_func)
{
	if (!mty_webview_supported())
		return NULL;

	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	mty_webview_base_create(&ctx->base, app, window, dir, debug, ready_func, text_func, key_func);

	// WKWebView creation, start hidden
	#if TARGET_OS_OSX
		NSWindow *nsw = (__bridge NSWindow *) MTY_WindowGetNative(app, window);
		NSView *view = nsw.contentView;

	#else
		UIWindow *view = (__bridge UIWindow *) MTY_WindowGetNative(app, window);
	#endif

	ctx->webview = [OBJC_NEW(webview_class(), NULL) initWithFrame:view.frame];
	[view addSubview:ctx->webview];

	ctx->webview.hidden = YES;

	// Settings
	NSNumber *ndebug = debug ? @YES : @NO;

	[ctx->webview.configuration.preferences setValue:ndebug forKey:@"developerExtrasEnabled"];
	ctx->webview.configuration.preferences.tabFocusesLinks = YES;

	if (@available(macOS 13.3, *))
		ctx->webview.configuration.preferences.shouldPrintBackgrounds = NO;

	if (@available(macOS 11.3, *))
		ctx->webview.configuration.preferences.textInteractionEnabled = YES;

	// Message handler
	NSObject <WKScriptMessageHandler> *handler = OBJC_NEW(msg_handler_class(), ctx);

	[ctx->webview.configuration.userContentController addScriptMessageHandler:handler name:@"native"];

	// MTY javascript shim
	WKUserScript *script = [[WKUserScript alloc] initWithSource:@"window.parent = window.webkit.messageHandlers.native;"
		injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES
	];

	[ctx->webview.configuration.userContentController addUserScript:script];

	return ctx;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;

	if (ctx->webview) {
		[ctx->webview removeFromSuperview];
		ctx->webview = nil;
	}

	mty_webview_base_destroy(&ctx->base);

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_navigate(struct webview *ctx, const char *source, bool url)
{
	NSString *osource = [NSString stringWithUTF8String:source];

	if (url) {
		NSURL *ourl = [[NSURL alloc] initWithString:osource];

		NSURLRequest *req = [[NSURLRequest alloc] initWithURL:ourl];
		[ctx->webview loadRequest:req];

	} else {
		[ctx->webview loadHTMLString:osource baseURL:nil];
	}
}

void mty_webview_show(struct webview *ctx, bool show)
{
	// The way macOS bubbles key events can cause them to fire multiple times
	// between the WebView and window, potentially firing mty_webview_show
	// more than intended

	MTY_Time ts = MTY_GetTime();

	if (MTY_TimeDiff(ctx->ts, ts) < 10)
		return;

	ctx->ts = ts;
	ctx->webview.hidden = !show;

	#if TARGET_OS_OSX
	if (show) {
		[ctx->webview.window makeFirstResponder:ctx->webview];
		ctx->webview.nextResponder = nil;
	}
	#endif
}

bool mty_webview_is_visible(struct webview *ctx)
{
	return !ctx->webview.hidden;
}

void mty_webview_send_text(struct webview *ctx, const char *msg)
{
	if (!ctx->base.ready) {
		MTY_QueuePushPtr(ctx->base.pushq, MTY_Strdup(msg), 0);

	} else {
		MTY_JSON *json = MTY_JSONStringCreate(msg);
		char *text = MTY_JSONSerialize(json);
		NSString *wrapped = [NSString stringWithFormat:@"window.postMessage(%@, '*');", [NSString stringWithUTF8String:text]];
		MTY_Free(text);
		MTY_JSONDestroy(&json);

		[ctx->webview evaluateJavaScript:wrapped completionHandler:^(id object, NSError *error) {
			if (error)
				MTY_Log("'WKWebView:evaluateJavaScript' failed to evaluate string '%s'", text);
		}];
	}
}

void mty_webview_reload(struct webview *ctx)
{
	[ctx->webview reload];
}

void mty_webview_set_input_passthrough(struct webview *ctx, bool passthrough)
{
	ctx->base.passthrough = passthrough;
}

bool mty_webview_event(struct webview *ctx, MTY_Event *evt)
{
	if (evt->type == MTY_EVENT_SIZE) {
		#if TARGET_OS_OSX
			ctx->webview.frame = ctx->webview.window.contentView.frame;

		#else
			ctx->webview.frame = ctx->webview.window.frame;
		#endif
	}

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
	return false;
}

bool mty_webview_is_steam(void)
{
	return false;
}


bool mty_webview_is_available(void)
{
	return mty_webview_supported();
}
