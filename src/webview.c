#include <string.h>

#include "matoya.h"
#include "webview.h"

struct webview_resource {
	void *data;
	size_t size;
};

static void mty_webview_destroy_resource(void *ptr)
{
	struct webview_resource *resource = ptr;

	MTY_Free(resource->data);
	MTY_Free(resource);
}

MTY_Webview *mty_window_create_webview(MTY_Hash *webviews, void *handle, void *opaque)
{
	uint32_t identifier = MTY_GetRandomUInt(1, UINT32_MAX);
	MTY_Webview *webview = mty_webview_create(identifier, handle, opaque);

	MTY_HashSetInt(webviews, identifier, webview);

	return webview;
}

void mty_window_destroy_webview(MTY_Hash *webviews, MTY_Webview **webview)
{
	if (!webview || !*webview)
		return;

	MTY_Webview *ctx = *webview;

	uint32_t identifier = mty_webview_get_identifier(ctx);
	MTY_HashSetInt(webviews, identifier, NULL);
	mty_webview_destroy(ctx);

	*webview = NULL;
}

void mty_webview_create_common(MTY_Webview *ctx, uint32_t identifier, void *opaque)
{
	struct webview *common = (struct webview *) ctx;

	common->bindings    = MTY_HashCreate(0);
	common->resources   = MTY_HashCreate(0);
	common->identifier  = identifier;
	common->opaque      = opaque;
}

void mty_webview_destroy_common(MTY_Webview *ctx)
{
	struct webview *common = (struct webview *) ctx;

	if (common->resources)
		MTY_HashDestroy(&common->resources, mty_webview_destroy_resource);

	if (common->bindings)
		MTY_HashDestroy(&common->bindings, NULL);
}


uint32_t mty_webview_get_identifier(MTY_Webview *ctx)
{
	struct webview *common = (struct webview *) ctx;

	return ctx ? common->identifier : 0;
}

bool mty_webview_has_focus(MTY_Webview *ctx)
{
	struct webview *common = (struct webview *) ctx;

	return ctx && common->has_focus;
}

void mty_webview_handle_event(MTY_Webview *ctx, const char *message)
{
	struct webview *common = (struct webview *) ctx;

	MTY_JSON *json = MTY_JSONParse(message);

	uint32_t serial = 0;
	MTY_JSONObjGetUInt(json, "serial", &serial);

	char method[512] = {0};
	MTY_JSONObjGetString(json, "method", method, 512);

	const MTY_JSON *params = MTY_JSONObjGetItem(json, "params");

	MTY_WebviewBinding binding = (MTY_WebviewBinding) MTY_HashGet(common->bindings, method);

	if (binding)
		binding(ctx, serial, params, common->opaque);

	MTY_JSONDestroy(&json);
}

bool mty_webviews_has_focus(MTY_Hash *webviews)
{
	uint64_t iter = 0;
	int64_t key = 0;

	while (MTY_HashGetNextKeyInt(webviews, &iter, &key)) {
		MTY_Webview *ctx = (MTY_Webview *) MTY_HashGetInt(webviews, key);

		if (mty_webview_has_focus(ctx))
			return true;
	}

	return false;
}

void mty_webviews_resize(MTY_Hash *webviews)
{
	uint64_t iter = 0;
	int64_t key = 0;

	while (MTY_HashGetNextKeyInt(webviews, &iter, &key)) {
		MTY_Webview *ctx = (MTY_Webview *) MTY_HashGetInt(webviews, key);

		mty_webview_resize(ctx);
	}
}

void MTY_WebviewSetOrigin(MTY_Webview *ctx, int32_t x, int32_t y)
{
	struct webview *common = (struct webview *) ctx;

	if (!ctx) return;

	common->bounds.left = (float) x;
	common->bounds.top  = (float) y;

	mty_webview_resize(ctx);
}

void MTY_WebviewSetSize(MTY_Webview *ctx, uint32_t width, uint32_t height)
{
	struct webview *common = (struct webview *) ctx;

	if (!ctx)
		return;

	common->bounds.right  = (float) width;
	common->bounds.bottom = (float) height;

	mty_webview_resize(ctx);
}

void MTY_WebviewAutomaticSize(MTY_Webview *ctx, bool enable)
{
	struct webview *common = (struct webview *) ctx;

	common->auto_size = enable;
}

void MTY_WebviewMapVirtualHost(MTY_Webview *ctx, const char *scheme, const char *path)
{
	struct webview *common = (struct webview *) ctx;

	common->scheme = scheme;
	common->vpath = path;
}

void MTY_WebviewAddResource(MTY_Webview *ctx, const char *path, const void *data, size_t size)
{
	struct webview *common = (struct webview *) ctx;

	if (!size)
		size = strlen((const char *) data);

	struct webview_resource *resource = MTY_Alloc(1, sizeof(struct webview_resource));
	resource->data = MTY_Alloc(size + 1, 1);
	memcpy(resource->data, data, size);
	resource->size = size;

	MTY_HashSet(common->resources, path, resource);
}

const void *MTY_WebviewGetResource(MTY_Webview *ctx, const char *url, size_t *size, const char **mime)
{
	struct webview *common = (struct webview *) ctx;

	size_t lsize = 0;
	if (!size)
		size = &lsize;

	if (!common->scheme)
		return NULL;

	char *full_scheme = MTY_SprintfD("%s://", common->scheme);
	size_t full_scheme_len = strlen(full_scheme);
	if (strncmp(full_scheme, url, full_scheme_len))
		return NULL;
	MTY_Free(full_scheme);

	const char *path = &url[full_scheme_len];

	struct webview_resource *resource = MTY_HashGet(common->resources, path);
	if (resource) {
		*size = resource->size;

		if (mime)
			*mime = MTY_MIMEGetType(path);

		return resource->data;
	}

	if (!common->vpath)
		return NULL;

	char *vpath = MTY_SprintfD("%s%s", common->vpath, path);
	void *data = MTY_ReadFile(vpath, size);
	MTY_Free(vpath);

	if (data)
		MTY_WebviewAddResource(ctx, path, data, *size);

	if (mime)
		*mime = MTY_MIMEGetType(path);

	return data;
}

void MTY_WebviewJavascriptEvent(MTY_Webview *ctx, const char *name, MTY_JSON *json)
{
	char *serialized = MTY_JSONSerialize(json);
	char *javascript = MTY_SprintfD("window.dispatchEvent(new CustomEvent('%s', { detail: %s } ));", name, serialized);

	MTY_WebviewJavascriptEval(ctx, javascript);

	MTY_Free(javascript);
	MTY_Free(serialized);
}

void MTY_WebviewInteropReturn(MTY_Webview *ctx, uint32_t serial, bool success, const MTY_JSON *result)
{
	char *json = result ? MTY_JSONSerialize(result) : NULL;
	char *method = success ? "resolve" : "reject";

	char *js_resolve = MTY_SprintfD("window._rpc[%d].%s(%s);", serial, method, json ? json : "null");
	char *js_destroy = MTY_SprintfD("delete window._rpc[%d];", serial);

	MTY_WebviewJavascriptEval(ctx, js_resolve);
	MTY_WebviewJavascriptEval(ctx, js_destroy);

	MTY_Free(js_destroy);
	MTY_Free(js_resolve);
	MTY_Free(json);
}

void MTY_WebviewInteropBind(MTY_Webview *ctx, const char *name, MTY_WebviewBinding func, void *opaque)
{
	struct webview *common = (struct webview *) ctx;

	MTY_HashSet(common->bindings, name, func);

	char *binding = MTY_SprintfD("(function () { \
		const name = '%s'; \
		const RPC = window._rpc = (window._rpc || { nextSeq: 1 }); \
		window[name] = function (param) { \
			const seq = RPC.nextSeq++; \
			const promise = new Promise(function (resolve, reject) { \
				RPC[seq] = { \
					resolve: resolve, \
					reject: reject, \
				}; \
			}); \
			const payload = JSON.stringify({ \
				serial: seq, \
				method: name, \
				params: param, \
			}); \
			window.external.invoke(payload); \
			return promise; \
		}; \
	})()", name);

	MTY_WebviewJavascriptInit(ctx, binding);
	MTY_Free(binding);
}
