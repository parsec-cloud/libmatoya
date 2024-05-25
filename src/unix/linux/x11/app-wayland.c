// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define _DEFAULT_SOURCE // pid_t

#include "../../../app.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>

#include <unistd.h>
#include <wayland-client.h>
#include "dl/xdg-shell.h"
#include "dl/xdg-shell.c"

#include "../../hid/utils.h"
#include "evdev.h"
#include "keymap-wayland.h"

struct window {
	struct window_common cmn;
	struct wl_surface *surface;
	MTY_Window index;
	//XIC ic;
	MTY_Frame frame;
	int32_t last_width;
	int32_t last_height;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	struct wl_shm *wl_shm;
	//struct xinfo info;
};

struct MTY_App {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *wl_shm;
	struct xdg_wm_base *xdg_wm_base;
	//Cursor empty_cursor;
	//Cursor custom_cursor;
	//Cursor cursor;
	bool hide_cursor;
	char *class_name;
	MTY_Cursor scursor;
	MTY_DetachState detach;
	//XVisualInfo *vis;
	//Atom wm_close;
	//Atom wm_ping;
	//Window sel_owner;
	//XIM im;

	MTY_EventFunc event_func;
	MTY_AppFunc app_func;
	MTY_Hash *hotkey;
	MTY_Hash *deduper;
	MTY_Mutex *mutex;
	struct evdev *evdev;
	struct window *windows[MTY_WINDOW_MAX];
	uint32_t timeout;
	MTY_Time suspend_ts;
	bool relative;
	bool suspend_ss;
	bool mgrab;
	bool kbgrab;
	void *opaque;
	char *clip;
	float scale;

	//bool xfixes;
	//int32_t xfixes_base;

	uint64_t state;
	uint64_t prev_state;
};


// Helpers

static struct window *app_get_window(MTY_App *ctx, MTY_Window window)
{
	return window < 0 ? NULL : ctx->windows[window];
}

static MTY_Window app_find_open_window(MTY_App *ctx, MTY_Window req)
{
	if (req >= 0 && req < MTY_WINDOW_MAX && !ctx->windows[req])
		return req;

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		if (!ctx->windows[x])
			return x;

	return -1;
}

static MTY_Window app_find_window(MTY_App *ctx, struct wl_surface * wlwindow)
{
	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++) {
		struct window *win = app_get_window(ctx, x);

		if (win->surface == wlwindow)
			return x;
	}

	return -1;
}

static struct window *app_get_active_window(MTY_App *ctx)
{
	return NULL;
}


// Window manager helpers

static uint8_t window_wm_state(struct wl_display *display, struct wl_surface * w)
{
	uint8_t state = 0;

	return state;
}

static void window_wm_event(struct wl_display *display, struct wl_surface * w, int action, const char *state1, const char *state2)
{

}


// Clipboard (selections)




// Cursor/grab state

static void app_apply_keyboard_grab(MTY_App *app, struct window *win)
{

}

static void app_apply_mouse_grab(MTY_App *app, struct window *win)
{

}

static void app_apply_cursor(MTY_App *app, bool focus)
{

}


// Event handling


static float app_get_scale(struct wl_display *display)
{
	return 1.0f;
}

static void app_refresh_scale(MTY_App *ctx)
{

}



// App

static void app_evdev_connect(struct evdev_dev *device, void *opaque)
{
	MTY_App *ctx = opaque;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_CONNECT;
	evt.controller = mty_evdev_state(device);
	mty_hid_map_axes(&evt.controller);

	ctx->event_func(&evt, ctx->opaque);
}

static void app_evdev_disconnect(struct evdev_dev *device, void *opaque)
{
	MTY_App *ctx = opaque;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_DISCONNECT;
	evt.controller = mty_evdev_state(device);
	mty_hid_map_axes(&evt.controller);

	ctx->event_func(&evt, ctx->opaque);
}

static void app_evdev_report(struct evdev_dev *device, void *opaque)
{
	MTY_App *ctx = opaque;

	if (MTY_AppIsActive(ctx)) {
		MTY_Event evt = {0};
		evt.type = MTY_EVENT_CONTROLLER;
		evt.controller = mty_evdev_state(device);
		mty_hid_map_axes(&evt.controller);

		if (mty_hid_dedupe(ctx->deduper, &evt.controller))
			ctx->event_func(&evt, ctx->opaque);
	}
}

static void handleShellPing(void* data, struct xdg_wm_base* shell, uint32_t serial)
{
	xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = handleShellPing,
};

static void handleRegistry(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
{
	MTY_App *ctx = (MTY_App *) data;
	if (strcmp(interface, wl_shm_interface.name) == 0) {
		ctx->wl_shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	
	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		ctx->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);

	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		ctx->xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(ctx->xdg_wm_base, &xdg_wm_base_listener, data);
	} else {
		//printf("Other: %s\n", interface);
	}
}

static const struct wl_registry_listener registryListener = {
	.global = handleRegistry
};

MTY_App *MTY_AppCreate(MTY_AppFlag flags, MTY_AppFunc appFunc, MTY_EventFunc eventFunc, void *opaque)
{
	bool r = true;
	MTY_App *ctx = MTY_Alloc(1, sizeof(MTY_App));
	ctx->hotkey = MTY_HashCreate(0);
	ctx->deduper = MTY_HashCreate(0);
	ctx->mutex = MTY_MutexCreate();
	ctx->app_func = appFunc;
	ctx->event_func = eventFunc;
	ctx->opaque = opaque;
	ctx->class_name = MTY_Strdup(MTY_GetFileName(MTY_GetProcessPath(), false));

	// This may return NULL
	ctx->evdev = mty_evdev_create(app_evdev_connect, app_evdev_disconnect, ctx);

	ctx->display = wl_display_connect(NULL);
	if (!ctx->display) {
		printf("wl_display_connect failed: %d\n", errno);
		r = false;
		goto except;
	}

	ctx->registry = wl_display_get_registry(ctx->display);
	if (!ctx->registry) {
		printf("wl_display_get_registry failed: %d\n", errno);
		r = false;
		goto except;
	}

	wl_registry_add_listener(ctx->registry, &registryListener, ctx);
	int32_t ret = wl_display_roundtrip(ctx->display);
	if (ret == -1) {
		printf("wl_display_roundtrip failed: %d\n", errno);
	} else {
		printf("Handled %d requests\n", ret);
	}

	app_refresh_scale(ctx);

	except:

	if (!r)
		MTY_AppDestroy(&ctx);

	return ctx;
}

void MTY_AppDestroy(MTY_App **app)
{
	if (!app || !*app)
		return;

	printf("Destroy\n");

	MTY_App *ctx = *app;

	if (ctx->display)
		wl_display_disconnect(ctx->display);

	mty_evdev_destroy(&ctx->evdev);

	MTY_HashDestroy(&ctx->deduper, MTY_Free);
	MTY_HashDestroy(&ctx->hotkey, NULL);
	MTY_MutexDestroy(&ctx->mutex);
	MTY_Free(ctx->clip);
	MTY_Free(ctx->class_name);

	MTY_Free(*app);
	*app = NULL;
}

static void app_suspend_ss(MTY_App *ctx)
{
}

void MTY_AppRun(MTY_App *ctx)
{
	for (bool cont = true; cont;) {
		struct window *win0 = app_get_window(ctx, 0);

		// Grab / mouse state evaluation
		if (ctx->state != ctx->prev_state) {
			struct window *win = app_get_active_window(ctx);

			app_apply_keyboard_grab(ctx, win);
			app_apply_mouse_grab(ctx, win);
			app_apply_cursor(ctx, win != NULL);

			ctx->prev_state = ctx->state;
		}

		wl_display_dispatch(ctx->display);

		// evdev events
		if (ctx->evdev)
			mty_evdev_poll(ctx->evdev, app_evdev_report);

		// WebView main thread upkeep (Steam callbacks)
		if (win0->cmn.webview)
			mty_webview_run(win0->cmn.webview);

		// Fire app func after all events have been processed
		cont = ctx->app_func(ctx->opaque);

		// Keep screensaver from turning on
		if (ctx->suspend_ss)
			app_suspend_ss(ctx);

		if (ctx->timeout > 0)
			MTY_Sleep(ctx->timeout);
	}
}

void MTY_AppSetTimeout(MTY_App *ctx, uint32_t timeout)
{
	ctx->timeout = timeout;
}

bool MTY_AppIsActive(MTY_App *ctx)
{
	return app_get_active_window(ctx) != NULL;
}

void MTY_AppActivate(MTY_App *ctx, bool active)
{
	MTY_WindowActivate(ctx, 0, active);
}

void MTY_AppSetTray(MTY_App *ctx, const char *tooltip, const MTY_MenuItem *items, uint32_t len)
{
}

void MTY_AppRemoveTray(MTY_App *ctx)
{
}

void MTY_AppSendNotification(MTY_App *ctx, const char *title, const char *msg)
{
}

char *MTY_AppGetClipboard(MTY_App *ctx)
{
	MTY_MutexLock(ctx->mutex);

	char *str = ctx->clip ? MTY_Strdup(ctx->clip) : NULL;

	MTY_MutexUnlock(ctx->mutex);

	return str;
}

void MTY_AppSetClipboard(MTY_App *ctx, const char *text)
{
}

void MTY_AppStayAwake(MTY_App *ctx, bool enable)
{
	ctx->suspend_ss = enable;
}

MTY_DetachState MTY_AppGetDetachState(MTY_App *ctx)
{
	return ctx->detach;
}

void MTY_AppSetDetachState(MTY_App *ctx, MTY_DetachState state)
{
	if (ctx->detach != state) {
		ctx->detach = state;
		ctx->state++;
	}
}

bool MTY_AppIsMouseGrabbed(MTY_App *ctx)
{
	return ctx->mgrab;
}

void MTY_AppGrabMouse(MTY_App *ctx, bool grab)
{
	if (ctx->mgrab != grab) {
		ctx->mgrab = grab;
		ctx->state++;
	}
}

bool MTY_AppGetRelativeMouse(MTY_App *ctx)
{
	return ctx->relative;
}

void MTY_AppSetRelativeMouse(MTY_App *ctx, bool relative)
{
	if (ctx->relative != relative) {
		// FIXME This should keep track of the position where the cursor went into relative,
		// but for now since we usually call WarpCursor whenever we exit relative it
		// doesn't matter

		ctx->relative = relative;

		struct window *win = app_get_active_window(ctx);

		app_apply_mouse_grab(ctx, win);
		app_apply_cursor(ctx, win != NULL);
	}
}

void MTY_AppSetRGBACursor(MTY_App *ctx, const void *image, uint32_t width, uint32_t height,
	uint32_t hotX, uint32_t hotY)
{

}

void MTY_AppSetPNGCursor(MTY_App *ctx, const void *image, size_t size, uint32_t hotX, uint32_t hotY)
{
	uint32_t width = 0;
	uint32_t height = 0;
	void *rgba = image ? MTY_DecompressImage(image, size, &width, &height) : NULL;

	MTY_AppSetRGBACursor(ctx, rgba, width, height, hotX, hotY);

	MTY_Free(rgba);
}

void MTY_AppSetCursor(MTY_App *ctx, MTY_Cursor cursor)
{
	if (ctx->scursor != cursor) {
		ctx->scursor = cursor;
		ctx->state++;
	}
}

void MTY_AppSetCursorSize(MTY_App* ctx, uint32_t width, uint32_t height)
{
}

void MTY_AppShowCursor(MTY_App *ctx, bool show)
{
	if (ctx->hide_cursor == show) {
		ctx->hide_cursor = !show;
		ctx->state++;
	}
}

bool MTY_AppCanWarpCursor(MTY_App *ctx)
{
	return true;
}

bool MTY_AppIsKeyboardGrabbed(MTY_App *ctx)
{
	return ctx->kbgrab;
}

bool MTY_AppGrabKeyboard(MTY_App *ctx, bool grab)
{
	if (ctx->kbgrab != grab) {
		ctx->kbgrab = grab;
		ctx->state++;
	}

	return ctx->kbgrab;
}

uint32_t MTY_AppGetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key)
{
	mod &= 0xFF;

	return (uint32_t) (uintptr_t) MTY_HashGetInt(ctx->hotkey, (mod << 16) | key);
}

void MTY_AppSetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key, uint32_t id)
{
	mod &= 0xFF;
	MTY_HashSetInt(ctx->hotkey, (mod << 16) | key, (void *) (uintptr_t) id);
}

void MTY_AppRemoveHotkeys(MTY_App *ctx, MTY_Scope scope)
{
	MTY_HashDestroy(&ctx->hotkey, NULL);
	ctx->hotkey = MTY_HashCreate(0);
}

void MTY_AppEnableGlobalHotkeys(MTY_App *ctx, bool enable)
{
}

bool MTY_AppIsSoftKeyboardShowing(MTY_App *ctx)
{
	return false;
}

void MTY_AppShowSoftKeyboard(MTY_App *ctx, bool show)
{
}

MTY_Orientation MTY_AppGetOrientation(MTY_App *ctx)
{
	return MTY_ORIENTATION_USER;
}

void MTY_AppSetOrientation(MTY_App *ctx, MTY_Orientation orientation)
{
}

void MTY_AppRumbleController(MTY_App *ctx, uint32_t id, uint16_t low, uint16_t high)
{
	if (ctx->evdev)
		mty_evdev_rumble(ctx->evdev, id, low, high);
}

const char *MTY_AppGetControllerDeviceName(MTY_App *ctx, uint32_t id)
{
	return NULL;
}

MTY_CType MTY_AppGetControllerType(MTY_App *ctx, uint32_t id)
{
	return MTY_CTYPE_DEFAULT;
}

void MTY_AppSubmitHIDReport(MTY_App *ctx, uint32_t id, const void *report, size_t size)
{
}

bool MTY_AppIsPenEnabled(MTY_App *ctx)
{
	return false;
}

void MTY_AppEnablePen(MTY_App *ctx, bool enable)
{
}

MTY_InputMode MTY_AppGetInputMode(MTY_App *ctx)
{
	return MTY_INPUT_MODE_UNSPECIFIED;
}

void MTY_AppSetInputMode(MTY_App *ctx, MTY_InputMode mode)
{
}

void MTY_AppSetWMsgFunc(MTY_App *ctx, MTY_WMsgFunc func)
{
}


// Window


static MTY_Frame window_denormalize_frame(const MTY_Frame *frame, float scale)
{
	int32_t px_h = lrint(frame->size.h * (scale - 1));
	int32_t px_w = lrint(frame->size.w * (scale - 1));

	MTY_Frame dframe = *frame;
	dframe.x -= px_w / 2;
	dframe.y -= px_h / 2;
	dframe.size.w += px_w;
	dframe.size.h += px_h;

	// Ensure the title bar is visible
	if (dframe.y < 0)
		dframe.y = 0;

	return dframe;
}

/* Shared memory support code */
static void randname(char *buf)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int create_shm_file(void)
{
	int retries = 100;
	do {
		char name[] = "/wl_shm-XXXXXX";
		randname(name + sizeof(name) - 7);
		--retries;
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);
	return -1;
}

static int allocate_shm_file(size_t size)
{
	int fd = create_shm_file();
	if (fd < 0)
		return -1;
	int ret;
	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
	/* Sent by the compositor when it's no longer using this buffer */
	wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release,
};

static struct wl_buffer *draw_frame(struct window *ctx)
{
	const int width = 640, height = 480;
	int stride = width * 4;
	int size = stride * height;

	int fd = allocate_shm_file(size);
	if (fd == -1) {
		return NULL;
	}

	uint32_t *data = mmap(NULL, size,
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(ctx->wl_shm, fd, size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
			width, height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	/* Draw checkerboxed background */
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if ((x + y / 8 * 8) % 16 < 8)
				data[y * width + x] = 0xFF666666;
			else
				data[y * width + x] = 0xFFEEEEEE;
		}
	}

	munmap(data, size);
	wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
	return buffer;
}

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
	struct window *ctx = data;
	xdg_surface_ack_configure(xdg_surface, serial);

	struct wl_buffer *buffer = draw_frame(ctx);
	wl_surface_attach(ctx->surface, buffer, 0, 0);
	wl_surface_commit(ctx->surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

MTY_Window MTY_WindowCreate(MTY_App *app, const char *title, const MTY_Frame *frame, MTY_Window index)
{
	bool r = true;
	struct window *ctx = MTY_Alloc(1, sizeof(struct window));

	MTY_Window window = app_find_open_window(app, index);
	if (window == -1) {
		r = false;
		MTY_Log("Maximum windows (MTY_WINDOW_MAX) of %u reached", MTY_WINDOW_MAX);
		goto except;
	}

	ctx->index = window;
	ctx->surface = wl_compositor_create_surface(app->compositor);
	if (!ctx->surface) {
		r = false;
		goto except;
	}

	ctx->xdg_surface = xdg_wm_base_get_xdg_surface(app->xdg_wm_base, ctx->surface);
	xdg_surface_add_listener(ctx->xdg_surface, &xdg_surface_listener, ctx);
	ctx->xdg_toplevel = xdg_surface_get_toplevel(ctx->xdg_surface);
	ctx->wl_shm = app->wl_shm;

	app->windows[window] = ctx;

	MTY_Frame dframe = {0};

	if (!frame) {
		dframe = APP_DEFAULT_FRAME();
		frame = &dframe;
	}

	dframe = window_denormalize_frame(frame, app->scale);
	frame = &dframe;

	MTY_WindowSetTitle(app, window, title ? title : "MTY_Window");
	wl_surface_commit(ctx->surface);

	except:

	if (!r) {
		MTY_WindowDestroy(app, window);
		window = -1;
	}

	return window;
}

void MTY_WindowDestroy(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	mty_webview_destroy(&ctx->cmn.webview);

	MTY_Free(ctx);
	app->windows[window] = NULL;
}

MTY_Size MTY_WindowGetSize(MTY_App *app, MTY_Window window)
{
	return (MTY_Size) {0};
}

static void window_adjust_frame(struct wl_display *display, struct wl_surface * window, MTY_Frame *frame)
{

}

MTY_Frame MTY_WindowGetFrame(MTY_App *app, MTY_Window window)
{
	return (MTY_Frame) {0};
}

void MTY_WindowSetFrame(MTY_App *app, MTY_Window window, const MTY_Frame *frame)
{

}

void MTY_WindowSetMinSize(MTY_App *app, MTY_Window window, uint32_t minWidth, uint32_t minHeight)
{

}

MTY_Size MTY_WindowGetScreenSize(MTY_App *app, MTY_Window window)
{
	return (MTY_Size) {0};
}

float MTY_WindowGetScreenScale(MTY_App *app, MTY_Window window)
{
	return app->scale;
}

void MTY_WindowSetTitle(MTY_App *app, MTY_Window window, const char *title)
{
	struct window *ctx = app_get_window(app, window);
	xdg_toplevel_set_title(ctx->xdg_toplevel, title);
}

bool MTY_WindowIsVisible(MTY_App *app, MTY_Window window)
{
	return false;;
}

bool MTY_WindowIsActive(MTY_App *app, MTY_Window window)
{
	return false;
}

void MTY_WindowActivate(MTY_App *app, MTY_Window window, bool active)
{

}

bool MTY_WindowExists(MTY_App *app, MTY_Window window)
{
	return app_get_window(app, window) != NULL;
}

bool MTY_WindowIsFullscreen(MTY_App *app, MTY_Window window)
{
	return false;
}

void MTY_WindowSetFullscreen(MTY_App *app, MTY_Window window, bool fullscreen)
{

}

void MTY_WindowWarpCursor(MTY_App *app, MTY_Window window, uint32_t x, uint32_t y)
{

}

MTY_ContextState MTY_WindowGetContextState(MTY_App *app, MTY_Window window)
{
	return MTY_CONTEXT_STATE_NORMAL;
}

void *MTY_WindowGetNative(MTY_App *app, MTY_Window window)
{
	return NULL;
}


// App, Window Private

MTY_EventFunc mty_app_get_event_func(MTY_App *app, void **opaque)
{
	*opaque = app->opaque;

	return app->event_func;
}

MTY_Hash *mty_app_get_hotkey_hash(MTY_App *app)
{
	return app->hotkey;
}

struct window_common *mty_window_get_common(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return NULL;

	return &ctx->cmn;
}


// Misc

static char APP_KEYS[MTY_KEY_MAX][16];

MTY_Frame MTY_MakeDefaultFrame(int32_t x, int32_t y, uint32_t w, uint32_t h, float maxHeight)
{
	return (MTY_Frame) {0};
}

static void app_hotkey_init(void)
{

}

void MTY_HotkeyToString(MTY_Mod mod, MTY_Key key, char *str, size_t len)
{
	memset(str, 0, len);

	app_hotkey_init();

	MTY_Strcat(str, len, (mod & MTY_MOD_WIN) ? "Super+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_CTRL) ? "Ctrl+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_ALT) ? "Alt+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_SHIFT) ? "Shift+" : "");

	MTY_Strcat(str, len, APP_KEYS[key]);
}

void MTY_SetAppID(const char *id)
{
}

void *MTY_GLGetProcAddress(const char *name)
{
	return NULL;
}

void MTY_RunAndYield(MTY_IterFunc iter, void *opaque)
{
	while (iter(opaque));
}
