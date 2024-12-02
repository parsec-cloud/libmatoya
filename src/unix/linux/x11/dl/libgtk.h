#pragma once

#include "dl/sym.h"
#include "matoya.h"

#include "dl/libx11.h"


// glib-2.0

#define G_SOURCE_FUNC(f) ((GSourceFunc)(void (*)(void))(f))

typedef char gchar;
typedef int gint;
typedef unsigned int guint;
typedef unsigned long gulong;
typedef double gdouble;
typedef signed long gssize;
typedef void *gpointer;
typedef bool gboolean;
typedef void GObject;
typedef void GClosure;
typedef void GCancellable;
typedef void GAsyncResult;

typedef void (*GCallback)(void);
typedef gboolean (*GSourceFunc)(gpointer user_data);
typedef void (*GClosureNotify)(gpointer data, GClosure *closure);
typedef void (*GAsyncReadyCallback)(GObject *source_object, GAsyncResult *res, gpointer user_data);

typedef enum {
	G_CONNECT_AFTER = 1 << 0,
	G_CONNECT_SWAPPED = 1 << 1
} GConnectFlags;

static guint (*g_idle_add)(GSourceFunc function, gpointer data);
static gulong (*g_signal_connect_data)(gpointer instance, const gchar *detailed_signal, GCallback c_handler,
	gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);
static gboolean (*g_setenv)(const gchar *variable, const gchar *value, gboolean overwrite);


// gdk-3

typedef void GdkDisplay;
typedef void GdkWindow;

typedef struct {
	gdouble red;
	gdouble green;
	gdouble blue;
	gdouble alpha;
} GdkRGBA;

static Display *(*gdk_x11_display_get_xdisplay)(GdkDisplay *display);
static GdkDisplay *(*gdk_window_get_display)(GdkWindow *window);
static Window (*gdk_x11_window_get_xid)(GdkWindow *window);
static void (*gdk_window_show)(GdkWindow *window);
static void (*gdk_window_hide)(GdkWindow *window);
static gboolean (*gdk_window_is_visible)(GdkWindow *window);


// gtk-3

typedef void GtkContainer;
typedef void GtkWidget;
typedef void GtkWindow;

typedef enum { GTK_WINDOW_TOPLEVEL, GTK_WINDOW_POPUP } GtkWindowType;

static GtkWidget *(*gtk_window_new)(GtkWindowType type);
static void (*gtk_widget_realize)(GtkWidget *widget);
static GdkWindow *(*gtk_widget_get_window)(GtkWidget *widget);
static void (*gtk_window_get_size)(GtkWindow *window, gint *width, gint *height);
static void (*gtk_window_resize)(GtkWindow *window, gint width, gint height);
static void (*gtk_container_add)(GtkContainer *container, GtkWidget *widget);
static void (*gtk_widget_set_app_paintable)(GtkWidget *widget, gboolean app_paintable);
static void (*gtk_widget_show_all)(GtkWidget *widget);
static gboolean (*gtk_init_check)(int *argc, char ***argv);
static void (*gtk_main)(void);
static void (*gtk_window_close)(GtkWindow *window);
static void (*gtk_widget_destroy)(GtkWidget *widget);
static void (*gtk_main_quit)(void);


// libjavascriptcoregtk-4.0

typedef void JSCValue;

static char *(*jsc_value_to_string)(JSCValue *value);


// libwebkit2gtk-4.0

typedef void WebKitSettings;
typedef void WebKitWebView;
typedef void WebKitUserContentManager;
typedef void WebKitJavascriptResult;
typedef void WebKitUserScript;

typedef enum {
	WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
	WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
} WebKitUserContentInjectedFrames;

typedef enum {
	WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
	WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
} WebKitUserScriptInjectionTime;

static JSCValue *(*webkit_javascript_result_get_js_value)(WebKitJavascriptResult *js_result);
static GtkWidget *(*webkit_web_view_new)(void);
static void (*webkit_web_view_set_background_color)(WebKitWebView *web_view, const GdkRGBA *rgba);
static WebKitUserContentManager *(*webkit_web_view_get_user_content_manager)(WebKitWebView *web_view);
static gboolean (*webkit_user_content_manager_register_script_message_handler)(WebKitUserContentManager *manager, const gchar *name);
static WebKitUserScript *(*webkit_user_script_new)(const gchar *source, WebKitUserContentInjectedFrames injected_frames,
		WebKitUserScriptInjectionTime injection_time, const gchar *const *allow_list, const gchar *const *block_list);
static void (*webkit_user_content_manager_add_script)(WebKitUserContentManager *manager, WebKitUserScript *script);
static WebKitSettings *(*webkit_web_view_get_settings)(WebKitWebView *web_view);
static void (*webkit_settings_set_enable_developer_extras)(WebKitSettings *settings, gboolean enabled);
static void (*webkit_web_view_load_uri)(WebKitWebView *web_view, const gchar *uri);
static void (*webkit_web_view_load_html)(WebKitWebView *web_view, const gchar *content, const gchar *base_uri);
static void (*webkit_web_view_evaluate_javascript)(WebKitWebView *web_view, const char *script, gssize length, const char *world_name,
	const char *source_uri, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
static void (*webkit_web_view_reload)(WebKitWebView *web_view);


// Runtime open

static MTY_Atomic32 LIBGTK_LOCK;
static MTY_SO *LIBGOBJECT_SO;
static MTY_SO *LIBGLIB_SO;
static MTY_SO *LIBDGK_SO;
static MTY_SO *LIBGTK_SO;
static MTY_SO *LIBJAVASCRIPT_SO;
static MTY_SO *LIBWEBKIT2_SO;
static bool LIBGTK_INIT;

static void libgtk_global_destroy_lockfree(void) {
	MTY_SOUnload(&LIBWEBKIT2_SO);
	MTY_SOUnload(&LIBJAVASCRIPT_SO);
	MTY_SOUnload(&LIBGTK_SO);
	MTY_SOUnload(&LIBDGK_SO);
	MTY_SOUnload(&LIBGLIB_SO);
	MTY_SOUnload(&LIBGOBJECT_SO);
	LIBGTK_INIT = false;
}

static void __attribute__((destructor)) libgtk_global_destroy(void) {
	MTY_GlobalLock(&LIBGTK_LOCK);

	libgtk_global_destroy_lockfree();

	MTY_GlobalUnlock(&LIBGTK_LOCK);
}

static bool libgtk_global_init(void) {
	MTY_GlobalLock(&LIBGTK_LOCK);

	if (!LIBGTK_INIT) {
		bool r = true;

		LIBGOBJECT_SO = MTY_SOLoad("libgobject-2.0.so");
		LIBGLIB_SO = MTY_SOLoad("libglib-2.0.so");
		LIBDGK_SO = MTY_SOLoad("libgdk-3.so");
		LIBGTK_SO = MTY_SOLoad("libgtk-3.so");
		LIBJAVASCRIPT_SO = MTY_SOLoad("libjavascriptcoregtk-4.0.so");
		LIBWEBKIT2_SO = MTY_SOLoad("libwebkit2gtk-4.0.so");
		if (!LIBGOBJECT_SO || !LIBGLIB_SO || !LIBDGK_SO || !LIBGTK_SO || !LIBJAVASCRIPT_SO || !LIBWEBKIT2_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBGOBJECT_SO, g_signal_connect_data);

		LOAD_SYM(LIBGLIB_SO, g_idle_add);
		LOAD_SYM(LIBGLIB_SO, g_setenv);

		LOAD_SYM(LIBDGK_SO, gdk_x11_display_get_xdisplay);
		LOAD_SYM(LIBDGK_SO, gdk_window_get_display);
		LOAD_SYM(LIBDGK_SO, gdk_x11_window_get_xid);
		LOAD_SYM(LIBDGK_SO, gdk_window_show);
		LOAD_SYM(LIBDGK_SO, gdk_window_hide);
		LOAD_SYM(LIBDGK_SO, gdk_window_is_visible);

		LOAD_SYM(LIBGTK_SO, gtk_window_new);
		LOAD_SYM(LIBGTK_SO, gtk_widget_realize);
		LOAD_SYM(LIBGTK_SO, gtk_widget_get_window);
		LOAD_SYM(LIBGTK_SO, gtk_window_get_size);
		LOAD_SYM(LIBGTK_SO, gtk_window_resize);
		LOAD_SYM(LIBGTK_SO, gtk_container_add);
		LOAD_SYM(LIBGTK_SO, gtk_widget_set_app_paintable);
		LOAD_SYM(LIBGTK_SO, gtk_widget_show_all);
		LOAD_SYM(LIBGTK_SO, gtk_init_check);
		LOAD_SYM(LIBGTK_SO, gtk_main);
		LOAD_SYM(LIBGTK_SO, gtk_window_close);
		LOAD_SYM(LIBGTK_SO, gtk_widget_destroy);
		LOAD_SYM(LIBGTK_SO, gtk_main_quit);

		LOAD_SYM(LIBJAVASCRIPT_SO, jsc_value_to_string);

		LOAD_SYM(LIBWEBKIT2_SO, webkit_javascript_result_get_js_value);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_web_view_new);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_web_view_set_background_color);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_web_view_get_user_content_manager);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_user_content_manager_register_script_message_handler);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_user_script_new);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_user_content_manager_add_script);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_web_view_get_settings);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_settings_set_enable_developer_extras);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_web_view_load_uri);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_web_view_load_html);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_web_view_evaluate_javascript);
		LOAD_SYM(LIBWEBKIT2_SO, webkit_web_view_reload);

	except:

		if (!r)
			libgtk_global_destroy_lockfree();

		LIBGTK_INIT = r;
	}

	MTY_GlobalUnlock(&LIBGTK_LOCK);

	return LIBGTK_INIT;
}
