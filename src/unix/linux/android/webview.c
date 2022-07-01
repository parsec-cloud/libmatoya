#include <string.h>
#include <math.h>

#include "matoya.h"
#include "webview.h"
#include "jnih.h"

typedef void (*get_screen_size)(uint32_t *width, uint32_t *height);

struct MTY_Webview {
	struct webview common;

	jobject obj;
	get_screen_size get_screen_size;
};

MTY_Webview *mty_webview_create(uint32_t identifier, void *handle, void *opaque)
{
	MTY_Webview *ctx = MTY_Alloc(1, sizeof(MTY_Webview));

	mty_webview_create_common(ctx, identifier, opaque);

	ctx->obj = (jobject) ((void **) handle)[0];
	ctx->get_screen_size = (get_screen_size) ((void **) handle)[1];

	return ctx;
}

void mty_webview_destroy(MTY_Webview *ctx)
{
	if (!ctx)
		return;

	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewDestroy", "(J)V", (long) ctx);

	MTY_Free(ctx);
}

void mty_webview_resize(MTY_Webview *ctx)
{
	float scale = mty_jni_float(MTY_GetJNIEnv(), ctx->obj, "getDisplayDensity", "()F");

	int32_t x = lrint(ctx->common.bounds.left * scale);
	int32_t y = lrint(ctx->common.bounds.top * scale);
	uint32_t width = lrint(ctx->common.bounds.right * scale);
	uint32_t height = lrint(ctx->common.bounds.bottom * scale);

	if (ctx->common.auto_size) {
		x = 0;
		y = 0;
		ctx->get_screen_size(&width, &height);
	}

	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewResize", "(JIIII)V", (long) ctx, x, y, (int32_t) width, (int32_t) height);
}

void MTY_WebviewShow(MTY_Webview *ctx, MTY_WebviewOnCreated callback)
{
	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewShow", "(J)V", (long) ctx);

	callback(ctx, ctx->common.opaque);
}

void MTY_WebviewEnableDevTools(MTY_Webview *ctx, bool enable)
{
	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewEnableDevTools", "(Z)V", enable);
}

void MTY_WebviewOpenDevTools(MTY_Webview *ctx)
{
	// XXX Not supported by the platform
}

static void mty_webview_call(MTY_Webview *ctx, const char *name, const char *text)
{
	JNIEnv *env = MTY_GetJNIEnv();
	jstring jtext = mty_jni_strdup(env, text);

	mty_jni_void(env, ctx->obj, name, "(JLjava/lang/String;)V", (long) ctx, jtext);

	mty_jni_free(env, jtext);
}

void MTY_WebviewNavigateURL(MTY_Webview *ctx, const char *url)
{
	mty_webview_call(ctx, "webviewNavigateURL", url);
}

void MTY_WebviewNavigateHTML(MTY_Webview *ctx, const char *html)
{
	mty_webview_call(ctx, "webviewNavigateHTML", html);
}

void MTY_WebviewJavascriptInit(MTY_Webview *ctx, const char *js)
{
	mty_webview_call(ctx, "webviewJavascriptInit", js);
}

void MTY_WebviewJavascriptEval(MTY_Webview *ctx, const char *js)
{
	mty_webview_call(ctx, "webviewJavascriptEval", js);
}

JNIEXPORT void JNICALL Java_group_matoya_lib_Matoya_webview_1handle_1event(JNIEnv *env, jobject obj,
	jlong jctx, jstring jjson)
{
	char *json = mty_jni_cstrdup(env, jjson);
	mty_webview_handle_event((MTY_Webview *) jctx, json);
	MTY_Free(json);
}

static jbyteArray mty_webview_get_java_resource(JNIEnv *env, jlong jctx, jstring jurl, jstring *jmime)
{
	char *url = mty_jni_cstrdup(env, jurl);

	size_t size = 0;
	const char *mime = NULL;
	const void *res = MTY_WebviewGetResource((MTY_Webview *) jctx, url, &size, &mime);

	MTY_Free(url);

	if (jmime)
		*jmime = mty_jni_strdup(env, mime);
	
	return mty_jni_dup(env, res, size);
}

JNIEXPORT jbyteArray JNICALL Java_group_matoya_lib_Matoya_webview_1get_1resource(JNIEnv *env, jobject obj,
	jlong jctx, jstring jurl)
{
	return mty_webview_get_java_resource(env, jctx, jurl, NULL);
}

JNIEXPORT jstring JNICALL Java_group_matoya_lib_Matoya_webview_1get_1mime(JNIEnv *env, jobject obj,
	jlong jctx, jstring jurl)
{
	jstring mime = NULL;
	mty_webview_get_java_resource(env, jctx, jurl, &mime);
	return mime;
}
