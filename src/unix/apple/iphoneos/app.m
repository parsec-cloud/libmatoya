// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "app.h"

#include <math.h>

#include <UIKit/UIKit.h>
#include <GameController/GameController.h>
#include <objc/message.h>

#include "objc.h"
#include "hid/utils.h"
#include "gcmap.h"

// UIKit owns the process entry point: UIApplicationMain must run on the main thread
// before any window can exist, and it never returns. libmatoya therefore provides
// main() itself, and calls the application's entry point on the main thread once a
// window scene has connected. The application's main() must be renamed to mty_main,
// i.e. compile it with -Dmain=mty_main. MTY_AppRun then pumps the run loop itself,
// so MTY_AppCreate -> MTY_WindowCreate -> MTY_AppRun -> cleanup behaves as it does
// on the desktop.

int mty_main(int argc, char **argv);

#define DELEGATE_CLASS_NAME "__mty_app_delegate_" CLASS_VER
#define SCENE_CLASS_NAME    "__mty_app_scene_"    CLASS_VER
#define VC_CLASS_NAME       "__mty_app_vc_"       CLASS_VER

#define APP_MAX_CONTROLLERS 16

// Siri Remote buttons, reported as key events
enum app_remote_key {
	APP_REMOTE_UP,
	APP_REMOTE_DOWN,
	APP_REMOTE_LEFT,
	APP_REMOTE_RIGHT,
	APP_REMOTE_SELECT,
	APP_REMOTE_BACK,
	APP_REMOTE_PLAY,
	APP_REMOTE_MAX,
};

struct window {
	UIWindow *uiw;
	UIViewController *vc;
	MTY_App *app;
	struct window_common cmn;
	MTY_Window window;
	MTY_Size size;
	float scale;
};

struct MTY_App {
	MTY_AppFunc app_func;
	MTY_EventFunc event_func;
	MTY_Hash *hotkey;
	MTY_Hash *deduper;
	MTY_AppFlag flags;
	void *opaque;
	bool cont;
	bool active;
	float timeout;
	struct window *windows[MTY_WINDOW_MAX];
	GCController *controllers[APP_MAX_CONTROLLERS];
	GCController *remote;
	bool remote_keys[APP_REMOTE_MAX];
	UITextField *text_field;
	bool kb_showing;
	id connect_observer;
	id disconnect_observer;
};

// UIKit instantiates the delegates itself, so their state can't live in indexed ivars
static struct {
	int argc;
	char **argv;
	UIWindowScene *scene;
	MTY_App *app;
	bool started;
} APP_GLOBAL;

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

static void app_set_active(MTY_App *ctx, bool active)
{
	if (ctx->active == active)
		return;

	ctx->active = active;

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++) {
		if (ctx->windows[x]) {
			MTY_Event evt = {
				.type = MTY_EVENT_FOCUS,
				.window = x,
				.focus = active,
			};

			ctx->event_func(&evt, ctx->opaque);
		}
	}
}


// Siri Remote / keyboard presses

static MTY_Key app_press_to_key(UIPress *press)
{
	switch (press.type) {
		case UIPressTypeUpArrow:    return MTY_KEY_UP;
		case UIPressTypeDownArrow:  return MTY_KEY_DOWN;
		case UIPressTypeLeftArrow:  return MTY_KEY_LEFT;
		case UIPressTypeRightArrow: return MTY_KEY_RIGHT;
		case UIPressTypeSelect:     return MTY_KEY_ENTER;
		case UIPressTypeMenu:       return MTY_KEY_ESCAPE;
		case UIPressTypePlayPause:  return MTY_KEY_SPACE;
		default:
			break;
	}

	return MTY_KEY_NONE;
}

static void app_key_event(MTY_App *ctx, MTY_Key key, bool pressed)
{
	MTY_Window window = -1;
	for (MTY_Window x = 0; x < MTY_WINDOW_MAX && window < 0; x++)
		if (ctx->windows[x])
			window = x;

	if (window < 0)
		return;

	MTY_Event evt = {
		.type = MTY_EVENT_KEY,
		.window = window,
		.key.key = key,
		.key.pressed = pressed,
	};

	mty_app_kb_to_hotkey(ctx, &evt, MTY_EVENT_HOTKEY);

	if (evt.type != MTY_EVENT_NONE)
		ctx->event_func(&evt, ctx->opaque);
}

// Hardware keyboards. Game controllers and the Siri Remote don't arrive here: the root
// view controller is a GCEventViewController, so their input goes through GameController.
static bool vc_press_event(UIViewController *self, NSSet<UIPress *> *presses, bool pressed)
{
	struct window *ctx = OBJC_CTX();
	if (!ctx || ctx->app->kb_showing)
		return false;

	bool handled = false;

	for (UIPress *press in presses) {
		MTY_Key key = app_press_to_key(press);
		if (key == MTY_KEY_NONE)
			continue;

		app_key_event(ctx->app, key, pressed);
		handled = true;
	}

	return handled;
}

static void vc_super_presses(UIViewController *self, SEL cmd, NSSet<UIPress *> *presses, UIPressesEvent *event)
{
	struct objc_super s = {self, [GCEventViewController class]};
	((void (*)(struct objc_super *, SEL, NSSet *, UIPressesEvent *)) objc_msgSendSuper)(&s, cmd, presses, event);
}

static void vc_pressesBegan_withEvent(UIViewController *self, SEL _cmd, NSSet<UIPress *> *presses, UIPressesEvent *event)
{
	if (!vc_press_event(self, presses, true))
		vc_super_presses(self, _cmd, presses, event);
}

static void vc_pressesEnded_withEvent(UIViewController *self, SEL _cmd, NSSet<UIPress *> *presses, UIPressesEvent *event)
{
	if (!vc_press_event(self, presses, false))
		vc_super_presses(self, _cmd, presses, event);
}

static void vc_pressesCancelled_withEvent(UIViewController *self, SEL _cmd, NSSet<UIPress *> *presses, UIPressesEvent *event)
{
	if (!vc_press_event(self, presses, false))
		vc_super_presses(self, _cmd, presses, event);
}

static Class vc_class(void)
{
	Class cls = objc_getClass(VC_CLASS_NAME);
	if (cls)
		return cls;

	// GCEventViewController: controller and Siri Remote input is delivered only through
	// GameController (controllerUserInteractionEnabled = NO), not also as UIKit presses
	cls = OBJC_ALLOCATE("GCEventViewController", VC_CLASS_NAME);

	OBJC_OVERRIDE(cls, @selector(pressesBegan:withEvent:), vc_pressesBegan_withEvent);
	OBJC_OVERRIDE(cls, @selector(pressesEnded:withEvent:), vc_pressesEnded_withEvent);
	OBJC_OVERRIDE(cls, @selector(pressesCancelled:withEvent:), vc_pressesCancelled_withEvent);

	objc_registerClassPair(cls);

	return cls;
}


// Soft keyboard
//
// The system keyboard is full screen and edits its own text field, so the result is
// delivered when it closes: the text as MTY_EVENT_TEXT followed by an Enter press to
// commit it, or an Escape press if it was dismissed with nothing entered. While it is
// up, UIKit owns the remote and controllers and the app sees them as idle.

static void app_controller_state(MTY_App *ctx, GCController *gc);
static void app_remote_state(MTY_App *ctx, GCController *gc);

static MTY_Window app_first_window(MTY_App *ctx)
{
	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		if (ctx->windows[x])
			return x;

	return -1;
}

static void app_kb_set_owner(MTY_App *ctx, bool kb)
{
	ctx->kb_showing = kb;

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		if (ctx->windows[x])
			((GCEventViewController *) ctx->windows[x]->vc).controllerUserInteractionEnabled = kb;

	// Release anything held when the keyboard takes over, resync when it gives input back
	for (uint32_t x = 0; x < APP_MAX_CONTROLLERS; x++)
		if (ctx->controllers[x])
			app_controller_state(ctx, ctx->controllers[x]);

	if (ctx->remote)
		app_remote_state(ctx, ctx->remote);
}

static void app_kb_tap(MTY_App *ctx, MTY_Key key)
{
	app_key_event(ctx, key, true);

	// The UI samples key state once per frame, so the release can't come right away
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
		if (APP_GLOBAL.app == ctx)
			app_key_event(ctx, key, false);
	});
}

static void app_kb_send_text(MTY_App *ctx, NSString *text)
{
	MTY_Window window = app_first_window(ctx);
	if (window < 0)
		return;

	NSRange range = NSMakeRange(0, text.length);
	[text enumerateSubstringsInRange:range options:NSStringEnumerationByComposedCharacterSequences
		usingBlock:^(NSString *sub, NSRange r, NSRange er, BOOL *stop) {
			const char *utf8 = sub.UTF8String;

			MTY_Event evt = {0};
			evt.type = MTY_EVENT_TEXT;
			evt.window = window;

			if (utf8 && strlen(utf8) < sizeof(evt.text)) {
				snprintf(evt.text, sizeof(evt.text), "%s", utf8);
				ctx->event_func(&evt, ctx->opaque);
			}
		}];
}

static void app_kb_did_end(MTY_App *ctx)
{
	// Closed by the app (MTY_AppShowSoftKeyboard), nothing to deliver
	if (!ctx->kb_showing)
		return;

	NSString *text = ctx->text_field.text;
	ctx->text_field.text = @"";

	app_kb_set_owner(ctx, false);

	if (text.length > 0) {
		app_kb_send_text(ctx, text);
		app_kb_tap(ctx, MTY_KEY_ENTER);

	} else {
		app_kb_tap(ctx, MTY_KEY_ESCAPE);
	}
}

static void app_kb_show(MTY_App *ctx, bool show)
{
	if (show == ctx->kb_showing)
		return;

	if (!show) {
		app_kb_set_owner(ctx, false);
		[ctx->text_field resignFirstResponder];
		return;
	}

	MTY_Window window = app_first_window(ctx);
	if (window < 0)
		return;

	if (!ctx->text_field) {
		if (@available(tvOS 14.0, iOS 14.0, *)) {
			// Never visible itself, the system presents the keyboard with its own copy
			UITextField *field = [[UITextField alloc] initWithFrame:CGRectMake(0, 0, 1, 1)];
			field.alpha = 0;
			field.autocorrectionType = UITextAutocorrectionTypeNo;
			field.autocapitalizationType = UITextAutocapitalizationTypeNone;
			field.spellCheckingType = UITextSpellCheckingTypeNo;
			field.returnKeyType = UIReturnKeyDone;

			[field addAction:[UIAction actionWithHandler:^(UIAction *action) {
				app_kb_did_end(ctx);
			}] forControlEvents:UIControlEventEditingDidEnd];

			ctx->text_field = field;

		} else {
			return;
		}
	}

	UIView *view = ctx->windows[window]->vc.view;
	if (ctx->text_field.superview != view)
		[view addSubview:ctx->text_field];

	ctx->text_field.text = @"";
	app_kb_set_owner(ctx, true);

	if (![ctx->text_field becomeFirstResponder]) {
		MTY_Log("Soft keyboard text field could not become first responder");
		app_kb_set_owner(ctx, false);
	}
}


// Game controllers (GameController.framework)

static uint32_t app_controller_id(MTY_App *ctx, GCController *gc)
{
	for (uint32_t x = 0; x < APP_MAX_CONTROLLERS; x++)
		if (ctx->controllers[x] == gc)
			return x;

	return UINT32_MAX;
}

static void app_controller_state(MTY_App *ctx, GCController *gc)
{
	uint32_t id = app_controller_id(ctx, gc);
	if (id == UINT32_MAX || !gc.extendedGamepad)
		return;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_CONTROLLER;
	evt.controller.id = id;

	gcmap_extended_gamepad(gc.extendedGamepad, &evt.controller);

	// The soft keyboard owns controller input while it's up, report the controller as idle
	if (ctx->kb_showing) {
		memset(evt.controller.buttons, 0, sizeof(evt.controller.buttons));

		for (uint8_t x = 0; x < evt.controller.numAxes; x++)
			evt.controller.axes[x].value = 0;
	}

	// Prevent gamepad input while in the background, dedupe
	if (ctx->active && mty_hid_dedupe(ctx->deduper, &evt.controller))
		ctx->event_func(&evt, ctx->opaque);
}

// Buttons bound to a tvOS gesture (on Xbox controllers, the View button) are held back
// until the system rules the gesture out, which arrives 100-200ms late and breaks button
// combinations. Take them for the app. The Home/Xbox button is not one of these: tvOS
// always keeps it.
static void app_own_gesture_buttons(GCController *gc)
{
	if (@available(tvOS 14.0, iOS 14.0, *)) {
		for (GCControllerElement *e in gc.physicalInputProfile.allElements) {
			if (e.boundToSystemGesture)
				e.preferredSystemGestureState = GCSystemGestureStateDisabled;
		}
	}
}

// Siri Remote (GameController micro / directional gamepad)

static const MTY_Key APP_REMOTE_KEYS[APP_REMOTE_MAX] = {
	[APP_REMOTE_UP]     = MTY_KEY_UP,
	[APP_REMOTE_DOWN]   = MTY_KEY_DOWN,
	[APP_REMOTE_LEFT]   = MTY_KEY_LEFT,
	[APP_REMOTE_RIGHT]  = MTY_KEY_RIGHT,
	[APP_REMOTE_SELECT] = MTY_KEY_ENTER,
	[APP_REMOTE_BACK]   = MTY_KEY_ESCAPE,
	[APP_REMOTE_PLAY]   = MTY_KEY_SPACE,
};

// XXX The Back button never returns to the tvOS home screen while GameController owns
// the remote. A shipping app needs a policy for letting it through at its top level.
static void app_remote_state(MTY_App *ctx, GCController *gc)
{
	GCMicroGamepad *mg = gc.microGamepad;
	if (gc != ctx->remote || !mg)
		return;

	GCControllerDirectionPad *cardinal = nil;
	GCControllerButtonInput *center = nil;

	if (@available(tvOS 14.5, iOS 14.5, *))
		cardinal = (GCControllerDirectionPad *) gc.physicalInputProfile.dpads[GCInputDirectionalCardinalDpad];

	if (@available(tvOS 15.0, iOS 15.0, *))
		center = (GCControllerButtonInput *) gc.physicalInputProfile.buttons[GCInputDirectionalCenterButton];

	bool state[APP_REMOTE_MAX] = {0};

	if (cardinal) {
		// 2nd generation remote: the outer ring is a 4-way dpad, the inner ring selects
		state[APP_REMOTE_UP] = cardinal.up.pressed;
		state[APP_REMOTE_DOWN] = cardinal.down.pressed;
		state[APP_REMOTE_LEFT] = cardinal.left.pressed;
		state[APP_REMOTE_RIGHT] = cardinal.right.pressed;

		bool ring = state[APP_REMOTE_UP] || state[APP_REMOTE_DOWN] || state[APP_REMOTE_LEFT] || state[APP_REMOTE_RIGHT];
		state[APP_REMOTE_SELECT] = center ? center.pressed : mg.buttonA.pressed && !ring;

	} else {
		// 1st generation remote: a touch surface only, clicking it selects
		state[APP_REMOTE_SELECT] = mg.buttonA.pressed;
	}

	state[APP_REMOTE_BACK] = mg.buttonMenu.pressed;
	state[APP_REMOTE_PLAY] = mg.buttonX.pressed;

	for (uint32_t x = 0; x < APP_REMOTE_MAX; x++) {
		// Releases always go through so nothing stays held while in the background, or
		// while the soft keyboard owns the remote
		bool pressed = state[x] && ctx->active && !ctx->kb_showing;

		if (pressed != ctx->remote_keys[x]) {
			ctx->remote_keys[x] = pressed;
			app_key_event(ctx, APP_REMOTE_KEYS[x], pressed);
		}
	}
}

static void app_remote_connect(MTY_App *ctx, GCController *gc)
{
	if (ctx->remote || !gc.microGamepad)
		return;

	ctx->remote = gc;

	// The controller retains these blocks, so reach it through the profile instead of capturing it.
	// Before tvOS 16 the micro gamepad handler still fires for ring presses, which also press
	// the touch surface, and app_remote_state reads the ring from the physical input profile.
	if (@available(tvOS 16.0, iOS 16.0, *)) {
		gc.physicalInputProfile.valueDidChangeHandler = ^(GCPhysicalInputProfile *profile, GCControllerElement *element) {
			app_remote_state(ctx, (GCController *) profile.device);
		};

	} else {
		gc.microGamepad.valueChangedHandler = ^(GCMicroGamepad *gamepad, GCControllerElement *element) {
			app_remote_state(ctx, gamepad.controller);
		};
	}
}

static void app_remote_disconnect(MTY_App *ctx, GCController *gc)
{
	if (gc != ctx->remote)
		return;

	if (@available(tvOS 16.0, iOS 16.0, *))
		gc.physicalInputProfile.valueDidChangeHandler = nil;

	gc.microGamepad.valueChangedHandler = nil;
	ctx->remote = nil;

	for (uint32_t x = 0; x < APP_REMOTE_MAX; x++) {
		if (ctx->remote_keys[x]) {
			ctx->remote_keys[x] = false;
			app_key_event(ctx, APP_REMOTE_KEYS[x], false);
		}
	}
}

static void app_controller_connect(MTY_App *ctx, GCController *gc)
{
	// The Siri Remote only has a micro gamepad profile, its buttons become key events
	if (!gc.extendedGamepad) {
		app_remote_connect(ctx, gc);
		return;
	}

	if (app_controller_id(ctx, gc) != UINT32_MAX)
		return;

	app_own_gesture_buttons(gc);

	for (uint32_t x = 0; x < APP_MAX_CONTROLLERS; x++) {
		if (!ctx->controllers[x]) {
			ctx->controllers[x] = gc;

			// The controller retains this block, so reach it through the gamepad instead of capturing it
			gc.extendedGamepad.valueChangedHandler = ^(GCExtendedGamepad *gamepad, GCControllerElement *element) {
				app_controller_state(ctx, gamepad.controller);
			};

			MTY_Event evt = {0};
			evt.type = MTY_EVENT_CONNECT;
			evt.controller.id = x;

			ctx->event_func(&evt, ctx->opaque);
			break;
		}
	}
}

static void app_controller_disconnect(MTY_App *ctx, GCController *gc)
{
	app_remote_disconnect(ctx, gc);

	uint32_t id = app_controller_id(ctx, gc);
	if (id == UINT32_MAX)
		return;

	gc.extendedGamepad.valueChangedHandler = nil;
	ctx->controllers[id] = nil;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_DISCONNECT;
	evt.controller.id = id;

	ctx->event_func(&evt, ctx->opaque);
}


// Application / scene delegates

static void scene_appRunMain(id self, SEL _cmd)
{
	exit(mty_main(APP_GLOBAL.argc, APP_GLOBAL.argv));
}

static UISceneConfiguration *app_application_configurationForConnectingSceneSession_options(id self, SEL _cmd,
	UIApplication *application, UISceneSession *session, UISceneConnectionOptions *options)
{
	UISceneConfiguration *config = [[UISceneConfiguration alloc] initWithName:nil sessionRole:session.role];
	config.delegateClass = objc_getClass(SCENE_CLASS_NAME);

	return config;
}

static void scene_scene_willConnectToSession_options(id self, SEL _cmd, UIScene *scene,
	UISceneSession *session, UISceneConnectionOptions *options)
{
	APP_GLOBAL.scene = (UIWindowScene *) scene;

	// Defer so this callback returns before the application's main takes over the main thread
	if (!APP_GLOBAL.started) {
		APP_GLOBAL.started = true;
		[self performSelector:@selector(appRunMain) withObject:nil afterDelay:0];
	}
}

static void scene_sceneDidBecomeActive(id self, SEL _cmd, UIScene *scene)
{
	if (APP_GLOBAL.app)
		app_set_active(APP_GLOBAL.app, true);
}

static void scene_sceneWillResignActive(id self, SEL _cmd, UIScene *scene)
{
	if (APP_GLOBAL.app)
		app_set_active(APP_GLOBAL.app, false);
}

static void app_register_classes(void)
{
	Class cls = OBJC_ALLOCATE("UIResponder", DELEGATE_CLASS_NAME);
	Protocol *proto = OBJC_PROTOCOL(cls, @protocol(UIApplicationDelegate));
	OBJC_POVERRIDE(cls, proto, NO, @selector(application:configurationForConnectingSceneSession:options:),
		app_application_configurationForConnectingSceneSession_options);
	objc_registerClassPair(cls);

	cls = OBJC_ALLOCATE("UIResponder", SCENE_CLASS_NAME);
	proto = OBJC_PROTOCOL(cls, @protocol(UIWindowSceneDelegate));
	OBJC_POVERRIDE(cls, proto, NO, @selector(scene:willConnectToSession:options:), scene_scene_willConnectToSession_options);
	OBJC_POVERRIDE(cls, proto, NO, @selector(sceneDidBecomeActive:), scene_sceneDidBecomeActive);
	OBJC_POVERRIDE(cls, proto, NO, @selector(sceneWillResignActive:), scene_sceneWillResignActive);
	class_addMethod(cls, @selector(appRunMain), (IMP) scene_appRunMain, "v@:");
	objc_registerClassPair(cls);
}

int main(int argc, char **argv)
{
	APP_GLOBAL.argc = argc;
	APP_GLOBAL.argv = argv;

	@autoreleasepool {
		app_register_classes();

		return UIApplicationMain(argc, argv, nil, @DELEGATE_CLASS_NAME);
	}
}


// App

MTY_App *MTY_AppCreate(MTY_AppFlag flags, MTY_AppFunc appFunc, MTY_EventFunc eventFunc, void *opaque)
{
	if (!APP_GLOBAL.scene) {
		MTY_Log("No window scene, the application entry point must be built with -Dmain=mty_main");
		return NULL;
	}

	MTY_App *ctx = MTY_Alloc(1, sizeof(MTY_App));
	ctx->flags = flags;
	ctx->app_func = appFunc;
	ctx->event_func = eventFunc;
	ctx->opaque = opaque;
	ctx->cont = true;
	ctx->active = APP_GLOBAL.scene.activationState == UISceneActivationStateForegroundActive;

	ctx->hotkey = MTY_HashCreate(0);
	ctx->deduper = MTY_HashCreate(0);

	APP_GLOBAL.app = ctx;

	NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];

	ctx->connect_observer = [nc addObserverForName:GCControllerDidConnectNotification object:nil
		queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *n) {
			app_controller_connect(ctx, n.object);
		}];

	ctx->disconnect_observer = [nc addObserverForName:GCControllerDidDisconnectNotification object:nil
		queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *n) {
			app_controller_disconnect(ctx, n.object);
		}];

	for (GCController *gc in [GCController controllers])
		app_controller_connect(ctx, gc);

	return ctx;
}

void MTY_AppDestroy(MTY_App **app)
{
	if (!app || !*app)
		return;

	MTY_App *ctx = *app;

	// Before the windows go, so ending the edit doesn't deliver anything
	ctx->kb_showing = false;
	[ctx->text_field removeFromSuperview];
	ctx->text_field = nil;

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		MTY_WindowDestroy(ctx, x);

	[[NSNotificationCenter defaultCenter] removeObserver:ctx->connect_observer];
	[[NSNotificationCenter defaultCenter] removeObserver:ctx->disconnect_observer];
	ctx->connect_observer = nil;
	ctx->disconnect_observer = nil;

	for (uint32_t x = 0; x < APP_MAX_CONTROLLERS; x++) {
		ctx->controllers[x].extendedGamepad.valueChangedHandler = nil;
		ctx->controllers[x] = nil;
	}

	if (ctx->remote)
		app_remote_disconnect(ctx, ctx->remote);

	MTY_HashDestroy(&ctx->hotkey, NULL);
	MTY_HashDestroy(&ctx->deduper, MTY_Free);

	APP_GLOBAL.app = NULL;

	MTY_Free(ctx);
	*app = NULL;
}

void MTY_AppRun(MTY_App *ctx)
{
	while (ctx->cont) {
		@autoreleasepool {
			// Drain pending UIKit work, then wait out the timeout while still servicing events
			while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource);

			ctx->cont = ctx->app_func(ctx->opaque);

			if (ctx->cont && ctx->timeout > 0.0f)
				CFRunLoopRunInMode(kCFRunLoopDefaultMode, ctx->timeout, true);
		}
	}
}

void MTY_AppSetTimeout(MTY_App *ctx, uint32_t timeout)
{
	ctx->timeout = (float) timeout / 1000.0f;
}

bool MTY_AppIsActive(MTY_App *ctx)
{
	return ctx->active;
}

void MTY_AppActivate(MTY_App *ctx, bool active)
{
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
	return NULL;
}

void MTY_AppSetClipboard(MTY_App *ctx, const char *text)
{
}

void MTY_AppStayAwake(MTY_App *ctx, bool enable)
{
	dispatch_async(dispatch_get_main_queue(), ^{
		[UIApplication sharedApplication].idleTimerDisabled = enable;
	});
}

MTY_DetachState MTY_AppGetDetachState(MTY_App *ctx)
{
	return MTY_DETACH_STATE_NONE;
}

void MTY_AppSetDetachState(MTY_App *ctx, MTY_DetachState state)
{
}

bool MTY_AppIsMouseGrabbed(MTY_App *ctx)
{
	return false;
}

void MTY_AppGrabMouse(MTY_App *ctx, bool grab)
{
}

bool MTY_AppGetRelativeMouse(MTY_App *ctx)
{
	return false;
}

void MTY_AppSetRelativeMouse(MTY_App *ctx, bool relative)
{
}

void MTY_AppSetRGBACursor(MTY_App *ctx, const void *image, uint32_t width, uint32_t height,
	uint32_t hotX, uint32_t hotY)
{
}

void MTY_AppSetPNGCursor(MTY_App *ctx, const void *image, size_t size, uint32_t hotX, uint32_t hotY)
{
}

void MTY_AppSetCursor(MTY_App *ctx, MTY_Cursor cursor)
{
}

void MTY_AppSetCursorSize(MTY_App *ctx, uint32_t width, uint32_t height)
{
}

void MTY_AppShowCursor(MTY_App *ctx, bool show)
{
}

bool MTY_AppCanWarpCursor(MTY_App *ctx)
{
	return false;
}

bool MTY_AppIsKeyboardGrabbed(MTY_App *ctx)
{
	return false;
}

bool MTY_AppGrabKeyboard(MTY_App *ctx, bool grab)
{
	return false;
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
	return ctx->kb_showing;
}

void MTY_AppShowSoftKeyboard(MTY_App *ctx, bool show)
{
	if ([NSThread isMainThread]) {
		app_kb_show(ctx, show);

	} else {
		dispatch_async(dispatch_get_main_queue(), ^{
			if (APP_GLOBAL.app == ctx)
				app_kb_show(ctx, show);
		});
	}
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
}

const char *MTY_AppGetControllerDeviceName(MTY_App *ctx, uint32_t id)
{
	if (id >= APP_MAX_CONTROLLERS || !ctx->controllers[id])
		return NULL;

	return ctx->controllers[id].vendorName.UTF8String;
}

MTY_CType MTY_AppGetControllerType(MTY_App *ctx, uint32_t id)
{
	if (id >= APP_MAX_CONTROLLERS || !ctx->controllers[id])
		return MTY_CTYPE_DEFAULT;

	return MTY_CTYPE_XINPUT;
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

MTY_Window MTY_WindowCreate(MTY_App *app, const char *title, const MTY_Frame *frame, MTY_Window index)
{
	MTY_Window window = app_find_open_window(app, index);
	if (window == -1) {
		MTY_Log("Maximum windows (MTY_WINDOW_MAX) of %u reached", MTY_WINDOW_MAX);
		return -1;
	}

	struct window *ctx = MTY_Alloc(1, sizeof(struct window));
	ctx->window = window;
	ctx->app = app;

	// A UIWindow always fills its scene, the requested frame does not apply
	ctx->uiw = [[UIWindow alloc] initWithWindowScene:APP_GLOBAL.scene];
	ctx->vc = [OBJC_NEW(vc_class(), ctx) init];
	((GCEventViewController *) ctx->vc).controllerUserInteractionEnabled = NO;
	ctx->vc.view.backgroundColor = [UIColor blackColor];
	ctx->uiw.rootViewController = ctx->vc;

	// Cache the size here so it can be read from a render thread without touching UIKit
	CGSize size = ctx->uiw.bounds.size;
	ctx->scale = ctx->uiw.screen.nativeScale;
	ctx->size.w = lrint(size.width * ctx->scale);
	ctx->size.h = lrint(size.height * ctx->scale);

	app->windows[window] = ctx;

	if (!frame || !(frame->type & MTY_WINDOW_HIDDEN))
		[ctx->uiw makeKeyAndVisible];

	return window;
}

void MTY_WindowDestroy(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	app->windows[window] = NULL;

	OBJC_CTX_CLEAR(ctx->vc);
	ctx->uiw.hidden = YES;
	ctx->uiw.rootViewController = nil;
	ctx->uiw = nil;
	ctx->vc = nil;

	MTY_Free(ctx);
}

MTY_Size MTY_WindowGetSize(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return (MTY_Size) {0};

	return ctx->size;
}

MTY_Frame MTY_WindowGetFrame(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return (MTY_Frame) {0};

	return (MTY_Frame) {
		.type = MTY_WINDOW_FULLSCREEN,
		.size = ctx->size,
	};
}

void MTY_WindowSetFrame(MTY_App *app, MTY_Window window, const MTY_Frame *frame)
{
}

void MTY_WindowSetMinSize(MTY_App *app, MTY_Window window, uint32_t minWidth, uint32_t minHeight)
{
}

MTY_Size MTY_WindowGetScreenSize(MTY_App *app, MTY_Window window)
{
	return MTY_WindowGetSize(app, window);
}

float MTY_WindowGetScreenScale(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return 1.0f;

	return ctx->scale;
}

uint32_t MTY_WindowGetRefreshRate(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return 60;

	NSInteger fps = ctx->uiw.screen.maximumFramesPerSecond;

	return fps > 0 ? (uint32_t) fps : 60;
}

void MTY_WindowSetTitle(MTY_App *app, MTY_Window window, const char *title)
{
}

bool MTY_WindowIsVisible(MTY_App *app, MTY_Window window)
{
	return MTY_WindowExists(app, window) && app->active;
}

bool MTY_WindowIsActive(MTY_App *app, MTY_Window window)
{
	return MTY_WindowIsVisible(app, window);
}

void MTY_WindowActivate(MTY_App *app, MTY_Window window, bool active)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	if (active)
		[ctx->uiw makeKeyAndVisible];
}

bool MTY_WindowExists(MTY_App *app, MTY_Window window)
{
	return app_get_window(app, window) != NULL;
}

bool MTY_WindowIsFullscreen(MTY_App *app, MTY_Window window)
{
	return MTY_WindowExists(app, window);
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
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return NULL;

	return (__bridge void *) ctx->uiw;
}


// App, Window Private

MTY_EventFunc mty_app_get_event_func(MTY_App *ctx, void **opaque)
{
	*opaque = ctx->opaque;

	return ctx->event_func;
}

MTY_Hash *mty_app_get_hotkey_hash(MTY_App *ctx)
{
	return ctx->hotkey;
}

struct window_common *mty_window_get_common(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return NULL;

	return &ctx->cmn;
}


// Misc

MTY_Frame MTY_MakeDefaultFrame(int32_t x, int32_t y, uint32_t w, uint32_t h, float maxHeight)
{
	return (MTY_Frame) {
		.type = MTY_WINDOW_FULLSCREEN,
		.x = x,
		.y = y,
		.size.w = w,
		.size.h = h,
	};
}

void MTY_HotkeyToString(MTY_Mod mod, MTY_Key key, char *str, size_t len)
{
	if (len > 0)
		str[0] = '\0';
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
