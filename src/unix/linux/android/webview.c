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

static void mty_webview_call(MTY_Webview *ctx, const char *name, const char *text)
{
	JNIEnv *env = MTY_GetJNIEnv();
	jstring jtext = mty_jni_strdup(env, text);

	mty_jni_void(env, ctx->obj, name, "(Ljava/lang/String;)V", jtext);

	mty_jni_free(env, jtext);
}

MTY_Webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque)
{
	MTY_Webview *ctx = MTY_Alloc(1, sizeof(MTY_Webview));

	mty_webview_create_common(ctx, html, debug, event, opaque);

	ctx->obj = (jobject) ((void **) handle)[0];
	ctx->get_screen_size = (get_screen_size) ((void **) handle)[1];

	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewShow", "(J)V", (long) ctx);
	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewEnableDevTools", "(Z)V", ctx->common.debug);
	mty_webview_call(ctx, "webviewNavigate", ctx->common.html);

	return ctx;
}

void mty_webview_destroy(MTY_Webview **webview)
{
	if (!webview || !*webview)
		return;

	MTY_Webview *ctx = *webview;

	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewDestroy", "()V");

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_resize(MTY_Webview *ctx)
{
	uint32_t width = 0, height = 0;

	if (!ctx->common.hidden)
		ctx->get_screen_size(&width, &height);

	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewResize", "(IIII)V", 0, 0, (int32_t) width, (int32_t) height);
}

void mty_webview_javascript_eval(MTY_Webview *ctx, const char *js)
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
