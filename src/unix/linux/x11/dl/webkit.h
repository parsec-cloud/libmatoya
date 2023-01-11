#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "dl/libx11.h"
#include "sym.h"

// X11

static int (*XReparentWindow)(Display *display, Window w, Window parent, int x, int y);
static Status (*XGetWindowAttributes)(Display *display, Window w, XWindowAttributes *window_attributes_return);


// Glib

typedef bool gboolean;
typedef char gchar;
typedef int32_t gint;
typedef uint32_t guint;
typedef uint64_t gulong;
typedef double gdouble;
typedef void *gpointer;

typedef void *GClosureNotify;
typedef uint8_t GConnectFlags;
typedef struct GCancellable GCancellable;
typedef void (*GCallback)(void);
typedef gboolean (*GSourceFunc)(gpointer user_data);
typedef void (*GAsyncReadyCallback)(void);

static gboolean (*g_setenv)(const gchar *variable, const gchar *value, gboolean overwrite);
static guint (*g_idle_add)(GSourceFunc function, gpointer data);
static gulong (*g_signal_connect_data)(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);


// GDK

typedef struct GdkDisplay GdkDisplay;
typedef struct GdkWindow GdkWindow;

typedef struct {
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble alpha;
} GdkRGBA;

static Display *(*gdk_x11_display_get_xdisplay)(GdkDisplay *display);
static Window (*gdk_x11_window_get_xid)(GdkWindow *window);
static GdkDisplay *(*gdk_window_get_display)(GdkWindow *window);
static void (*gdk_window_show)(GdkWindow *window);
static void (*gdk_window_hide)(GdkWindow *window);


// GTK

typedef struct GtkWindow GtkWindow;
typedef struct GtkContainer GtkContainer;
typedef struct GtkWidget GtkWidget;

typedef enum {
  GTK_WINDOW_TOPLEVEL,
  GTK_WINDOW_POPUP
} GtkWindowType;

static gboolean (*gtk_init_check)(int *argc, char ***argv);
static void (*gtk_main)(void);
static void (*gtk_main_quit)(void);
static GtkWidget *(*gtk_window_new)(GtkWindowType type);
static void (*gtk_window_move)(GtkWindow *window, gint x, gint y);
static void (*gtk_window_resize)(GtkWindow *window, gint width, gint height);
static void (*gtk_window_close)(GtkWindow *window);
static void (*gtk_container_add)(GtkContainer *container, GtkWidget *widget);
static void (*gtk_widget_realize)(GtkWidget *widget);
static void (*gtk_widget_show_all)(GtkWidget *widget);
static GdkWindow *(*gtk_widget_get_window)(GtkWidget *widget);
static void (*gtk_widget_set_app_paintable)(GtkWidget *widget, gboolean app_paintable);
static void (*gtk_widget_destroy)(GtkWidget *widget);


// Javascript

typedef struct JSCValue JSCValue;

static char *(*jsc_value_to_string)(JSCValue *value);


// WebKit

typedef struct WebKitWebContext WebKitWebContext;
typedef struct WebKitSettings WebKitSettings;
typedef struct WebKitUserContentManager WebKitUserContentManager;
typedef struct WebKitWebView WebKitWebView;
typedef struct WebKitWebContext WebKitWebContext;
typedef struct WebKitUserScript WebKitUserScript;
typedef struct WebKitJavascriptResult WebKitJavascriptResult;

typedef enum {
    WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
    WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
} WebKitUserContentInjectedFrames;

typedef enum {
    WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
    WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
} WebKitUserScriptInjectionTime;

static WebKitWebContext *(*webkit_web_context_new)(void);
static GtkWidget *(*webkit_web_view_new_with_context)(WebKitWebContext *context);
static WebKitSettings *(*webkit_web_view_get_settings)(WebKitWebView *web_view);
static WebKitUserContentManager *(*webkit_web_view_get_user_content_manager)(WebKitWebView *web_view);
static void (*webkit_web_view_load_html)(WebKitWebView *web_view, const gchar *content, const gchar *base_uri);
static void (*webkit_web_view_run_javascript)(WebKitWebView *web_view, const gchar *script, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
static void (*webkit_web_view_set_background_color)(WebKitWebView *web_view, const GdkRGBA *rgba);
static WebKitUserScript *(*webkit_user_script_new)(const gchar *source, WebKitUserContentInjectedFrames injected_frames, WebKitUserScriptInjectionTime injection_time, const gchar* const *allow_list, const gchar* const *block_list);
static void (*webkit_user_content_manager_add_script)(WebKitUserContentManager *manager, WebKitUserScript *script);
static gboolean (*webkit_user_content_manager_register_script_message_handler)(WebKitUserContentManager *manager, const gchar *name);
static JSCValue *(*webkit_javascript_result_get_js_value)(WebKitJavascriptResult *js_result);
static void (*webkit_settings_set_enable_developer_extras)(WebKitSettings *settings, gboolean enabled);


// Runtime open

static MTY_Atomic32 WEKBIT_LOCK;
static MTY_SO *LIBX11_SO;
static MTY_SO *LIBGLIB_SO;
static MTY_SO *LIBGOBJECT_SO;
static MTY_SO *LIBGDK_SO;
static MTY_SO *LIBGTK_SO;
static MTY_SO *LIBJAVASCRIPT_SO;
static MTY_SO *LIBWEKBIT_SO;
static bool WEKBIT_INIT;

static void __attribute__((destructor)) webkit_global_destroy(void)
{
	MTY_GlobalLock(&WEKBIT_LOCK);

	MTY_SOUnload(&LIBX11_SO);
	MTY_SOUnload(&LIBGLIB_SO);
	MTY_SOUnload(&LIBGOBJECT_SO);
	MTY_SOUnload(&LIBGDK_SO);
	MTY_SOUnload(&LIBGTK_SO);
	MTY_SOUnload(&LIBJAVASCRIPT_SO);
	MTY_SOUnload(&LIBWEKBIT_SO);

	WEKBIT_INIT = false;

	MTY_GlobalUnlock(&WEKBIT_LOCK);
}

static bool webkit_global_init(void)
{
	MTY_GlobalLock(&WEKBIT_LOCK);

	if (!WEKBIT_INIT) {
		bool r = true;

		LIBX11_SO = MTY_SOLoad("libX11.so.6");
		LIBGLIB_SO = MTY_SOLoad("libglib-2.0.so.0");
		LIBGOBJECT_SO = MTY_SOLoad("libgobject-2.0.so.0");
		LIBGDK_SO = MTY_SOLoad("libgdk-3.so.0");
		LIBGTK_SO = MTY_SOLoad("libgtk-3.so.0");
		LIBJAVASCRIPT_SO = MTY_SOLoad("libjavascriptcoregtk-4.0.so.18");
		LIBWEKBIT_SO = MTY_SOLoad("libwebkit2gtk-4.0.so.37");

		if (!LIBX11_SO || !LIBGLIB_SO || !LIBGOBJECT_SO || !LIBGDK_SO || !LIBGTK_SO || !LIBJAVASCRIPT_SO || !LIBWEKBIT_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBX11_SO, XReparentWindow);
		LOAD_SYM(LIBX11_SO, XGetWindowAttributes);

		LOAD_SYM(LIBGLIB_SO, g_setenv);
		LOAD_SYM(LIBGLIB_SO, g_idle_add);
		LOAD_SYM(LIBGOBJECT_SO, g_signal_connect_data);

		LOAD_SYM(LIBGDK_SO, gdk_x11_display_get_xdisplay);
		LOAD_SYM(LIBGDK_SO, gdk_x11_window_get_xid);
		LOAD_SYM(LIBGDK_SO, gdk_window_get_display);
		LOAD_SYM(LIBGDK_SO, gdk_window_show);
		LOAD_SYM(LIBGDK_SO, gdk_window_hide);

		LOAD_SYM(LIBGTK_SO, gtk_init_check);
		LOAD_SYM(LIBGTK_SO, gtk_main);
		LOAD_SYM(LIBGTK_SO, gtk_main_quit);
		LOAD_SYM(LIBGTK_SO, gtk_window_new);
		LOAD_SYM(LIBGTK_SO, gtk_window_move);
		LOAD_SYM(LIBGTK_SO, gtk_window_resize);
		LOAD_SYM(LIBGTK_SO, gtk_window_close);
		LOAD_SYM(LIBGTK_SO, gtk_container_add);
		LOAD_SYM(LIBGTK_SO, gtk_widget_realize);
		LOAD_SYM(LIBGTK_SO, gtk_widget_show_all);
		LOAD_SYM(LIBGTK_SO, gtk_widget_get_window);
		LOAD_SYM(LIBGTK_SO, gtk_widget_set_app_paintable);
		LOAD_SYM(LIBGTK_SO, gtk_widget_destroy);

		LOAD_SYM(LIBJAVASCRIPT_SO, jsc_value_to_string);

		LOAD_SYM(LIBWEKBIT_SO, webkit_web_context_new);
		LOAD_SYM(LIBWEKBIT_SO, webkit_web_view_new_with_context);
		LOAD_SYM(LIBWEKBIT_SO, webkit_web_view_get_settings);
		LOAD_SYM(LIBWEKBIT_SO, webkit_web_view_get_user_content_manager);
		LOAD_SYM(LIBWEKBIT_SO, webkit_web_view_load_html);
		LOAD_SYM(LIBWEKBIT_SO, webkit_web_view_run_javascript);
		LOAD_SYM(LIBWEKBIT_SO, webkit_web_view_set_background_color);
		LOAD_SYM(LIBWEKBIT_SO, webkit_user_script_new);
		LOAD_SYM(LIBWEKBIT_SO, webkit_user_content_manager_add_script);
		LOAD_SYM(LIBWEKBIT_SO, webkit_user_content_manager_register_script_message_handler);
		LOAD_SYM(LIBWEKBIT_SO, webkit_javascript_result_get_js_value);
		LOAD_SYM(LIBWEKBIT_SO, webkit_settings_set_enable_developer_extras);

		except:

		if (!r)
			webkit_global_destroy();

		WEKBIT_INIT = r;
	}

	MTY_GlobalUnlock(&WEKBIT_LOCK);

	return WEKBIT_INIT;
}
