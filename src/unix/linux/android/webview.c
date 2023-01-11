#include <string.h>
#include <math.h>

#include "matoya.h"
#include "webview.h"
#include "jnih.h"

typedef void (*get_screen_size)(uint32_t *width, uint32_t *height);

struct webview {
	struct webview_common common;

	jobject obj;
	get_screen_size get_screen_size;
};

static void mty_webview_call(struct webview *ctx, const char *name, const char *text)
{
	JNIEnv *env = MTY_GetJNIEnv();
	jstring jtext = mty_jni_strdup(env, text);

	mty_jni_void(env, ctx->obj, name, "(Ljava/lang/String;)V", jtext);

	mty_jni_free(env, jtext);
}

struct webview *mty_webview_create(void *handle, const char *html, bool debug, MTY_EventFunc event, void *opaque)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	ctx->common.html = MTY_Strdup(html);
	ctx->common.debug = debug;
	ctx->common.event = event;
	ctx->common.opaque = opaque;

	ctx->obj = (jobject) ((void **) handle)[0];
	ctx->get_screen_size = (get_screen_size) ((void **) handle)[1];

	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewShow", "(J)V", (long) ctx);
	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewEnableDevTools", "(Z)V", ctx->common.debug);
	mty_webview_call(ctx, "webviewNavigate", ctx->common.html);

	return ctx;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;

	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewDestroy", "()V");

	MTY_Free(ctx->common.html);

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_resize(struct webview *ctx)
{
	if (!ctx)
		return;

	uint32_t width = 0, height = 0;

	if (!ctx->common.hidden)
		ctx->get_screen_size(&width, &height);

	mty_jni_void(MTY_GetJNIEnv(), ctx->obj, "webviewResize", "(IIII)V", 0, 0, (int32_t) width, (int32_t) height);
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

	mty_webview_call(ctx, "webviewJavascriptEval", javascript);

	MTY_Free(javascript);
}

static void mty_webview_handle_event(struct webview *ctx, const char *message)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_WEBVIEW;
	evt.message = message;

	ctx->common.event(&evt, ctx->common.opaque);
}

JNIEXPORT void JNICALL Java_group_matoya_lib_Matoya_webview_1handle_1event(JNIEnv *env, jobject obj,
	jlong jctx, jstring jjson)
{
	char *json = mty_jni_cstrdup(env, jjson);
	mty_webview_handle_event((struct webview *) jctx, json);
	MTY_Free(json);
}
