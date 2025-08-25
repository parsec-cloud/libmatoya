// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "webview.h"

#include <string.h>
#include <math.h>

#include "matoya.h"
#include "app-os.h"
#include "jnih.h"

struct webview {
	struct webview_base base;
};

struct webview *mty_webview_create(MTY_App *app, MTY_Window window, const char *dir,
	bool debug, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func, WEBVIEW_KEY key_func)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	mty_webview_base_create(&ctx->base, app, window, dir, debug, ready_func, text_func, key_func);
	mty_jni_void(MTY_GetJNIEnv(), mty_app_get_obj(), "webviewCreate", "(JZ)V", (long) ctx, debug);

	return ctx;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;
	*webview = NULL;

	mty_jni_void(MTY_GetJNIEnv(), mty_app_get_obj(), "webviewDestroy", "()V");
	mty_webview_base_destroy(&ctx->base);

	MTY_Free(ctx);
}

void mty_webview_navigate(struct webview *ctx, const char *source, bool url)
{
	jstring jsource = mty_jni_strdup(MTY_GetJNIEnv(), source);
	mty_jni_void(MTY_GetJNIEnv(), mty_app_get_obj(), "webviewNavigate", "(Ljava/lang/String;Z)V", jsource, url);
	mty_jni_free(MTY_GetJNIEnv(), jsource);
}

void mty_webview_show(struct webview *ctx, bool show)
{
	mty_jni_void(MTY_GetJNIEnv(), mty_app_get_obj(), "webviewShow", "(Z)V", show);
}

bool mty_webview_is_visible(struct webview *ctx)
{
	return mty_jni_bool(MTY_GetJNIEnv(), mty_app_get_obj(), "webviewIsVisible", "()Z");
}

void mty_webview_send_text(struct webview *ctx, const char *msg)
{
	if (!ctx->base.ready) {
		MTY_QueuePushPtr(ctx->base.pushq, MTY_Strdup(msg), 0);

	} else {
		jstring jmsg = mty_jni_strdup(MTY_GetJNIEnv(), msg);
		mty_jni_void(MTY_GetJNIEnv(), mty_app_get_obj(), "webviewSendText", "(Ljava/lang/String;)V", jmsg);
		mty_jni_free(MTY_GetJNIEnv(), jmsg);
	}
}

void mty_webview_reload(struct webview *ctx)
{
	mty_jni_void(MTY_GetJNIEnv(), mty_app_get_obj(), "webviewReload", "()V");
}

void mty_webview_set_input_passthrough(struct webview *ctx, bool passthrough)
{
	ctx->base.passthrough = passthrough;
}

bool mty_webview_event(struct webview *ctx, MTY_Event *evt)
{
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
	return true;
}

bool mty_webview_is_steam(void)
{
	return false;
}

bool mty_webview_is_available(void)
{
	return true;
}

JNIEXPORT void JNICALL Java_group_matoya_lib_Matoya_webview_1handle_1event(JNIEnv *env, jobject obj, jlong jctx, jstring jstr)
{
	struct webview *ctx = (struct webview *) jctx;
	char *str = mty_jni_cstrmov(MTY_GetJNIEnv(), jstr);
	mty_webview_base_handle_event(&ctx->base, str);
	MTY_Free(str);
}
