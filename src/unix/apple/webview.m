// #include <objc/objc-runtime.h>
#include <WebKit/WebKit.h>

#include "matoya.h"
#include "webview.h"

#ifndef OS_DESKTOP
typedef UIView NSView;
#endif

struct MTY_Webview {
	struct webview common;

	const NSView *view;
	WKWebView *webview;
};

API_AVAILABLE(macos(10.13)) 
@interface MySchemeHandler : NSObject <WKURLSchemeHandler>
	@property MTY_Webview *webview;
@end

@implementation MySchemeHandler
	- (MySchemeHandler *)init:(MTY_Webview *)webview
	{
		self.webview = webview;
		return self;
	}

	- (void)webView:(WKWebView *)webView 
		startURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask 
	{
		NSURL *url = urlSchemeTask.request.URL;
		size_t size = 0;
		const char *curl = [[url absoluteString] UTF8String];
		const void *res = MTY_WebviewGetResource(self.webview, curl, &size, NULL);

		if (res) {
			const char *mime = MTY_MIMEGetType(curl);
			NSDictionary* headers = @{ @"Content-Type": [NSString stringWithUTF8String:mime], @"Access-Control-Allow-Origin": @"*" };

			NSURLResponse *response = [[NSHTTPURLResponse alloc] initWithURL:url statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:headers];
			[urlSchemeTask didReceiveResponse:response];
			[urlSchemeTask didReceiveData:[NSData dataWithBytes:res length:size]];

		} else {
			NSURLResponse *response = [[NSHTTPURLResponse alloc] initWithURL:url statusCode:404 HTTPVersion:@"HTTP/1.1" headerFields:nil];
			[urlSchemeTask didReceiveResponse:response];
		}

		[urlSchemeTask didFinish];
	}

	- (void)webView:(WKWebView *)webView 
		stopURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
	}
@end

@interface ScriptMessageHandler : NSObject <WKScriptMessageHandler>
	@property MTY_Webview *webview;
@end

@implementation ScriptMessageHandler
	- (ScriptMessageHandler *)init:(MTY_Webview *)webview
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

MTY_Webview *mty_webview_create(uint32_t identifier, void *handle, void *opaque)
{
	MTY_Webview *ctx = MTY_Alloc(1, sizeof(MTY_Webview));

	mty_webview_create_common(ctx, identifier, opaque);

	ctx->view = (__bridge const NSView *) handle;

	return ctx;
}

void mty_webview_destroy(MTY_Webview *ctx)
{
	if (!ctx)
		return;
	
	WKUserContentController *controller = ctx->webview.configuration.userContentController;
	[controller removeScriptMessageHandlerForName:@"external"];

	[ctx->webview removeFromSuperview];

	MTY_Free(ctx);
}

void mty_webview_resize(MTY_Webview *ctx)
{
	float x = ctx->common.bounds.left;
	float y = ctx->common.bounds.top;
	float width = ctx->common.bounds.right;
	float height = ctx->common.bounds.bottom;

	if (ctx->common.auto_size) {
		x = 0;
		y = 0;
		width = ctx->view.frame.size.width;
		height = ctx->view.frame.size.height;
	}

	y = ctx->view.frame.size.height - height - y;

	CGRect rect = {0};
	rect.origin.x = x;
	rect.origin.y = y;
	rect.size.width = width;
	rect.size.height = height;

	ctx->webview.frame = rect;
}

void MTY_WebviewShow(MTY_Webview *ctx, MTY_WebviewOnCreated callback)
{
	WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
	if (@available(macOS 10.13, *)) {
		if (ctx->common.scheme) {
			NSString *ns_scheme = [NSString stringWithUTF8String:ctx->common.scheme];
			[configuration setURLSchemeHandler:[[MySchemeHandler alloc] init:ctx] forURLScheme:ns_scheme];
		}
	}
	ctx->webview = [[WKWebView alloc] initWithFrame:ctx->view.frame configuration:configuration];
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

	MTY_WebviewJavascriptInit(ctx, "window.external = { \
		invoke: function (s) { window.webkit.messageHandlers.external.postMessage(s); } \
	}");

	callback(ctx, ctx->common.opaque);
}

void MTY_WebviewEnableDevTools(MTY_Webview *ctx, bool enable)
{
	[ctx->webview.configuration.preferences setValue:(enable? @YES : @NO) forKey:@"developerExtrasEnabled"];
}

void MTY_WebviewOpenDevTools(MTY_Webview *ctx)
{
	// XXX Looks like WebKit does not support that
}

void MTY_WebviewNavigateURL(MTY_Webview *ctx, const char *url)
{
	NSString *str = [NSString stringWithUTF8String:url];
	NSURL *nsurl = [NSURL URLWithString:str];
	NSURLRequest *nsrequest = [NSURLRequest requestWithURL:nsurl];
	[ctx->webview loadRequest:nsrequest];
}

void MTY_WebviewNavigateHTML(MTY_Webview *ctx, const char *html)
{
	NSString *str = [NSString stringWithUTF8String:html];
	[ctx->webview loadHTMLString:str baseURL:nil];
}

void MTY_WebviewJavascriptInit(MTY_Webview *ctx, const char *js)
{
	WKUserContentController *controller = ctx->webview.configuration.userContentController;
	NSString *str = [NSString stringWithUTF8String:js];
	WKUserScriptInjectionTime injection = WKUserScriptInjectionTimeAtDocumentStart;
	WKUserScript *script = [[WKUserScript alloc] initWithSource:str injectionTime:injection forMainFrameOnly:true];
	[controller addUserScript:script];
}

void MTY_WebviewJavascriptEval(MTY_Webview *ctx, const char *js)
{
	NSString *str = [NSString stringWithUTF8String:js];
	[ctx->webview evaluateJavaScript:str completionHandler:nil];
}
