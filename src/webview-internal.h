#pragma once

#include "matoya.h"

#include <string.h>

static MTY_Key mty_webview_str_to_key(const char *key_str)
{
	MTY_Key key = MTY_KEY_NONE;

	if (!strcmp(key_str, "Escape"))
		key = MTY_KEY_ESCAPE;

	else if (!strcmp(key_str, "F1"))
		key = MTY_KEY_F1;

	else if (!strcmp(key_str, "F2"))
		key = MTY_KEY_F2;

	else if (!strcmp(key_str, "F3"))
		key = MTY_KEY_F3;

	else if (!strcmp(key_str, "F4"))
		key = MTY_KEY_F4;

	else if (!strcmp(key_str, "F5"))
		key = MTY_KEY_F5;

	else if (!strcmp(key_str, "F6"))
		key = MTY_KEY_F6;

	else if (!strcmp(key_str, "F7"))
		key = MTY_KEY_F7;

	else if (!strcmp(key_str, "F8"))
		key = MTY_KEY_F8;

	else if (!strcmp(key_str, "F9"))
		key = MTY_KEY_F9;

	else if (!strcmp(key_str, "F10"))
		key = MTY_KEY_F10;

	else if (!strcmp(key_str, "F11"))
		key = MTY_KEY_F11;

	else if (!strcmp(key_str, "F12"))
		key = MTY_KEY_F12;

	else if (!strcmp(key_str, "Backquote"))
		key = MTY_KEY_GRAVE;

	else if (!strcmp(key_str, "Digit1"))
		key = MTY_KEY_1;

	else if (!strcmp(key_str, "Digit2"))
		key = MTY_KEY_2;

	else if (!strcmp(key_str, "Digit3"))
		key = MTY_KEY_3;

	else if (!strcmp(key_str, "Digit4"))
		key = MTY_KEY_4;

	else if (!strcmp(key_str, "Digit5"))
		key = MTY_KEY_5;

	else if (!strcmp(key_str, "Digit6"))
		key = MTY_KEY_6;

	else if (!strcmp(key_str, "Digit7"))
		key = MTY_KEY_7;

	else if (!strcmp(key_str, "Digit8"))
		key = MTY_KEY_8;

	else if (!strcmp(key_str, "Digit9"))
		key = MTY_KEY_9;

	else if (!strcmp(key_str, "Digit0"))
		key = MTY_KEY_0;

	else if (!strcmp(key_str, "Minus"))
		key = MTY_KEY_MINUS;

	else if (!strcmp(key_str, "Equal"))
		key = MTY_KEY_EQUALS;

	else if (!strcmp(key_str, "Backspace"))
		key = MTY_KEY_BACKSPACE;

	else if (!strcmp(key_str, "Tab"))
		key = MTY_KEY_TAB;

	else if (!strcmp(key_str, "KeyQ"))
		key = MTY_KEY_Q;

	else if (!strcmp(key_str, "KeyW"))
		key = MTY_KEY_W;

	else if (!strcmp(key_str, "KeyE"))
		key = MTY_KEY_E;

	else if (!strcmp(key_str, "KeyR"))
		key = MTY_KEY_R;

	else if (!strcmp(key_str, "KeyT"))
		key = MTY_KEY_T;

	else if (!strcmp(key_str, "KeyY"))
		key = MTY_KEY_Y;

	else if (!strcmp(key_str, "KeyU"))
		key = MTY_KEY_U;

	else if (!strcmp(key_str, "KeyI"))
		key = MTY_KEY_I;

	else if (!strcmp(key_str, "KeyO"))
		key = MTY_KEY_O;

	else if (!strcmp(key_str, "KeyP"))
		key = MTY_KEY_P;

	else if (!strcmp(key_str, "BracketLeft"))
		key = MTY_KEY_LBRACKET;

	else if (!strcmp(key_str, "BracketRight"))
		key = MTY_KEY_RBRACKET;

	else if (!strcmp(key_str, "Backslash"))
		key = MTY_KEY_BACKSLASH;

	else if (!strcmp(key_str, "CapsLock"))
		key = MTY_KEY_CAPS;

	else if (!strcmp(key_str, "KeyA"))
		key = MTY_KEY_A;

	else if (!strcmp(key_str, "KeyS"))
		key = MTY_KEY_S;

	else if (!strcmp(key_str, "KeyD"))
		key = MTY_KEY_D;

	else if (!strcmp(key_str, "KeyF"))
		key = MTY_KEY_F;

	else if (!strcmp(key_str, "KeyG"))
		key = MTY_KEY_G;

	else if (!strcmp(key_str, "KeyH"))
		key = MTY_KEY_H;

	else if (!strcmp(key_str, "KeyJ"))
		key = MTY_KEY_J;

	else if (!strcmp(key_str, "KeyK"))
		key = MTY_KEY_K;

	else if (!strcmp(key_str, "KeyL"))
		key = MTY_KEY_L;

	else if (!strcmp(key_str, "Semicolon"))
		key = MTY_KEY_SEMICOLON;

	else if (!strcmp(key_str, "Quote"))
		key = MTY_KEY_QUOTE;

	else if (!strcmp(key_str, "Enter"))
		key = MTY_KEY_ENTER;

	else if (!strcmp(key_str, "ShiftLeft"))
		key = MTY_KEY_LSHIFT;

	else if (!strcmp(key_str, "KeyZ"))
		key = MTY_KEY_Z;

	else if (!strcmp(key_str, "KeyX"))
		key = MTY_KEY_X;

	else if (!strcmp(key_str, "KeyC"))
		key = MTY_KEY_C;

	else if (!strcmp(key_str, "KeyV"))
		key = MTY_KEY_V;

	else if (!strcmp(key_str, "KeyB"))
		key = MTY_KEY_B;

	else if (!strcmp(key_str, "KeyN"))
		key = MTY_KEY_N;

	else if (!strcmp(key_str, "KeyM"))
		key = MTY_KEY_M;

	else if (!strcmp(key_str, "Comma"))
		key = MTY_KEY_COMMA;

	else if (!strcmp(key_str, "Period"))
		key = MTY_KEY_PERIOD;

	else if (!strcmp(key_str, "Slash"))
		key = MTY_KEY_SLASH;

	else if (!strcmp(key_str, "ShiftRight"))
		key = MTY_KEY_RSHIFT;

	else if (!strcmp(key_str, "ControlLeft"))
		key = MTY_KEY_LCTRL;

	else if (!strcmp(key_str, "MetaLeft"))
		key = MTY_KEY_LWIN;

	else if (!strcmp(key_str, "AltLeft"))
		key = MTY_KEY_LALT;

	else if (!strcmp(key_str, "Space"))
		key = MTY_KEY_SPACE;

	else if (!strcmp(key_str, "AltRight"))
		key = MTY_KEY_RALT;

	else if (!strcmp(key_str, "MetaRight"))
		key = MTY_KEY_RWIN;

	else if (!strcmp(key_str, "ContextMenu"))
		key = MTY_KEY_APP;

	else if (!strcmp(key_str, "ControlRight"))
		key = MTY_KEY_RCTRL;

	else if (!strcmp(key_str, "PrintScreen"))
		key = MTY_KEY_PRINT_SCREEN;

	else if (!strcmp(key_str, "ScrollLock"))
		key = MTY_KEY_SCROLL_LOCK;

	else if (!strcmp(key_str, "Pause"))
		key = MTY_KEY_PAUSE;

	else if (!strcmp(key_str, "Insert"))
		key = MTY_KEY_INSERT;

	else if (!strcmp(key_str, "Home"))
		key = MTY_KEY_HOME;

	else if (!strcmp(key_str, "PageUp"))
		key = MTY_KEY_PAGE_UP;

	else if (!strcmp(key_str, "Delete"))
		key = MTY_KEY_DELETE;

	else if (!strcmp(key_str, "End"))
		key = MTY_KEY_END;

	else if (!strcmp(key_str, "PageDown"))
		key = MTY_KEY_PAGE_DOWN;

	else if (!strcmp(key_str, "ArrowUp"))
		key = MTY_KEY_UP;

	else if (!strcmp(key_str, "ArrowLeft"))
		key = MTY_KEY_LEFT;

	else if (!strcmp(key_str, "ArrowDown"))
		key = MTY_KEY_DOWN;

	else if (!strcmp(key_str, "ArrowRight"))
		key = MTY_KEY_RIGHT;

	else if (!strcmp(key_str, "NumLock"))
		key = MTY_KEY_NUM_LOCK;

	else if (!strcmp(key_str, "NumpadDivide"))
		key = MTY_KEY_NP_DIVIDE;

	else if (!strcmp(key_str, "NumpadMultiply"))
		key = MTY_KEY_NP_MULTIPLY;

	else if (!strcmp(key_str, "NumpadSubtract"))
		key = MTY_KEY_NP_MINUS;

	else if (!strcmp(key_str, "Numpad7"))
		key = MTY_KEY_NP_7;

	else if (!strcmp(key_str, "Numpad8"))
		key = MTY_KEY_NP_8;

	else if (!strcmp(key_str, "Numpad9"))
		key = MTY_KEY_NP_9;

	else if (!strcmp(key_str, "NumpadAdd"))
		key = MTY_KEY_NP_PLUS;

	else if (!strcmp(key_str, "Numpad4"))
		key = MTY_KEY_NP_4;

	else if (!strcmp(key_str, "Numpad5"))
		key = MTY_KEY_NP_5;

	else if (!strcmp(key_str, "Numpad6"))
		key = MTY_KEY_NP_6;

	else if (!strcmp(key_str, "Numpad1"))
		key = MTY_KEY_NP_1;

	else if (!strcmp(key_str, "Numpad2"))
		key = MTY_KEY_NP_2;

	else if (!strcmp(key_str, "Numpad3"))
		key = MTY_KEY_NP_3;

	else if (!strcmp(key_str, "NumpadEnter"))
		key = MTY_KEY_NP_ENTER;

	else if (!strcmp(key_str, "Numpad0"))
		key = MTY_KEY_NP_0;

	else if (!strcmp(key_str, "NumpadDecimal"))
		key = MTY_KEY_NP_PERIOD;

	// Non-US

	else if (!strcmp(key_str, "IntlBackslash"))
		key = MTY_KEY_INTL_BACKSLASH;

	else if (!strcmp(key_str, "IntlYen"))
		key = MTY_KEY_YEN;

	else if (!strcmp(key_str, "IntlRo"))
		key = MTY_KEY_RO;

	else if (!strcmp(key_str, "NonConvert"))
		key = MTY_KEY_MUHENKAN;

	else if (!strcmp(key_str, "Convert"))
		key = MTY_KEY_HENKAN;

	else if (!strcmp(key_str, "Hiragana"))
		key = MTY_KEY_JP;

	else if (!strcmp(key_str, "AltGraph"))
		key = MTY_KEY_RALT;

	else if (!strcmp(key_str, "Lang2"))
		key = MTY_KEY_MUHENKAN;

	else if (!strcmp(key_str, "Lang1"))
		key = MTY_KEY_HENKAN;

	else if (!strcmp(key_str, "KanaMode"))
		key = MTY_KEY_JP;

	else if (!strcmp(key_str, "Zenkaku"))
		key = MTY_KEY_GRAVE;

	return key;
}

static MTY_Mod mty_webview_modifier_flags_to_keymod(const char *mod_str)
{
	uint32_t modi = 0;

	if (!strcmp(mod_str, "ShiftLeft"))
		modi |= MTY_MOD_LSHIFT;

	else if (!strcmp(mod_str, "ShiftRight"))
		modi |= MTY_MOD_RSHIFT;

	else if (!strcmp(mod_str, "Shift"))
		modi |= MTY_MOD_SHIFT;

	else if (!strcmp(mod_str, "ControlLeft"))
		modi |= MTY_MOD_LCTRL;

	else if (!strcmp(mod_str, "ControlRight"))
		modi |= MTY_MOD_RCTRL;

	else if (!strcmp(mod_str, "Control"))
		modi |= MTY_MOD_CTRL;

	else if (!strcmp(mod_str, "AltLeft"))
		modi |= MTY_MOD_LALT;

	else if (!strcmp(mod_str, "AltRight"))
		modi |= MTY_MOD_RALT;

	else if (!strcmp(mod_str, "Alt"))
		modi |= MTY_MOD_ALT;

	else if (!strcmp(mod_str, "MetaLeft"))
		modi |= MTY_MOD_LWIN;

	else if (!strcmp(mod_str, "MetaRight"))
		modi |= MTY_MOD_RWIN;

	else if (!strcmp(mod_str, "Meta"))
		modi |= MTY_MOD_WIN;

	else if (!strcmp(mod_str, "CapsLock"))
		modi |= MTY_MOD_CAPS;

	else if (!strcmp(mod_str, "NumLock"))
		modi |= MTY_MOD_NUM;

	return (MTY_Mod) modi;
}

// Input interception feature
static void mty_webview_intercept_key_event(const char *message, MTY_Event *out)
{
	MTY_JSON *intercept = MTY_JSONParse(message);
	if (!intercept) return;

	const MTY_JSON *intercepted_key_event = MTY_JSONObjGetItem(intercept, "intercept");
	if (!intercepted_key_event) goto except;

	MTY_Key key = MTY_KEY_NONE;
	const char *key_str = MTY_JSONObjGetStringPtr(intercepted_key_event, "key");
	if (key_str && key_str[0])
		key = mty_webview_str_to_key(key_str);

	MTY_Mod mod = MTY_MOD_NONE;
	const MTY_JSON *mods = MTY_JSONObjGetItem(intercepted_key_event, "mods");
	if (mods) {
		uint32_t n = MTY_JSONArrayGetLength(mods);
		for (uint32_t i = 0; i < n; i++) {
			const MTY_JSON *item = MTY_JSONArrayGetItem(mods, i);
			const char *mod_str = MTY_JSONStringPtr(item);

			if (mod_str && mod_str[0])
				mod |= mty_webview_modifier_flags_to_keymod(mod_str);
		}
	}

	bool pressed = false;
	MTY_JSONObjGetBool(intercepted_key_event, "pressed", &pressed);

	// Success!
	out->type = MTY_EVENT_KEY;
	out->key.key = key;
	out->key.mod = mod;
	out->key.pressed = pressed;

except:

	MTY_JSONDestroy(&intercept);
}
