// #include <objc/objc-runtime.h>
#include <WebKit/WebKit.h>

#include "matoya.h"
#include "webview.h"

#ifndef OS_DESKTOP
typedef UIView NSView;
#endif

struct webview {
	struct webview_common common;

	const NSView *view;
	WKWebView *webview;
};

static void mty_webview_handle_event(struct webview *ctx, const char *message)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_WEBVIEW;
	evt.message = message;

	ctx->common.event(&evt, ctx->common.opaque);
}

@interface ScriptMessageHandler : NSObject <WKScriptMessageHandler>
	@property struct webview *webview;
@end

@implementation ScriptMessageHandler
	- (ScriptMessageHandler *)init:(struct webview *)webview
	{
		self.webview = webview;
		return self;
	}

	- (void)userContentController:(WKUserContentController *)userContentController
		didReceiveScriptMessage:(WKScriptMessage *)message
	{
		NSString *msg = (NSString *) message.body;
		const char *raw = [msg UTF8String];
		mty_webview_handle_event(self.webview, raw);
	}
@end

struct webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	ctx->common.html = MTY_Strdup(html);
	ctx->common.debug = debug;
	ctx->common.event = event;
	ctx->common.opaque = opaque;

	ctx->view = (__bridge const NSView *) handle;

	ctx->webview = [[WKWebView alloc] initWithFrame:ctx->view.frame];
	[ctx->view addSubview:ctx->webview];

#ifndef OS_DESKTOP
	ctx->webview.opaque = false;
	ctx->webview.backgroundColor = [UIColor clearColor];
#else
	[ctx->webview setValue: @NO forKey: @"drawsBackground"];
#endif

	WKUserContentController *controller = ctx->webview.configuration.userContentController;
	ScriptMessageHandler *handler = [[ScriptMessageHandler alloc] init:ctx];
	[controller addScriptMessageHandler:handler name:@"external"];

	WKUserScript *script = [[WKUserScript alloc]
		initWithSource:@"window.external = { \
			invoke: function (s) { window.webkit.messageHandlers.external.postMessage(s); } \
		}"
		injectionTime:WKUserScriptInjectionTimeAtDocumentStart 
		forMainFrameOnly:true
	];
	[ctx->webview.configuration.userContentController addUserScript:script];

	[ctx->webview.configuration.preferences setValue:(ctx->common.debug ? @YES : @NO) forKey:@"developerExtrasEnabled"];

	NSString *str = [NSString stringWithUTF8String:ctx->common.html];
	[ctx->webview loadHTMLString:str baseURL:nil];

	return ctx;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;

	WKUserContentController *controller = ctx->webview.configuration.userContentController;
	[controller removeScriptMessageHandlerForName:@"external"];

	[ctx->webview removeFromSuperview];

	MTY_Free(ctx->common.html);

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_resize(struct webview *ctx)
{
	if (!ctx)
		return;

	CGRect rect = {0};
	rect.origin.x = 0;
	rect.origin.y = 0;
	rect.size.width = ctx->common.hidden ? 0 : ctx->view.frame.size.width;
	rect.size.height = ctx->common.hidden ? 0 : ctx->view.frame.size.height;

	ctx->webview.frame = rect;
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

	NSString *str = [NSString stringWithUTF8String:javascript];
	[ctx->webview evaluateJavaScript:str completionHandler:nil];

	MTY_Free(javascript);
}
