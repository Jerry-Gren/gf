const int UI_KEYCODE_A = ES_SCANCODE_A;
const int UI_KEYCODE_0 = ES_SCANCODE_0;
const int UI_KEYCODE_BACKSPACE = ES_SCANCODE_BACKSPACE;
const int UI_KEYCODE_DELETE = ES_SCANCODE_DELETE;
const int UI_KEYCODE_DOWN = ES_SCANCODE_DOWN_ARROW;
const int UI_KEYCODE_END = ES_SCANCODE_END;
const int UI_KEYCODE_ENTER = ES_SCANCODE_ENTER;
const int UI_KEYCODE_ESCAPE = ES_SCANCODE_ESCAPE;
const int UI_KEYCODE_F1 = ES_SCANCODE_F1;
const int UI_KEYCODE_HOME = ES_SCANCODE_HOME;
const int UI_KEYCODE_LEFT = ES_SCANCODE_LEFT_ARROW;
const int UI_KEYCODE_RIGHT = ES_SCANCODE_RIGHT_ARROW;
const int UI_KEYCODE_SPACE = ES_SCANCODE_SPACE;
const int UI_KEYCODE_TAB = ES_SCANCODE_TAB;
const int UI_KEYCODE_UP = ES_SCANCODE_UP_ARROW;
const int UI_KEYCODE_INSERT = ES_SCANCODE_INSERT;
const int UI_KEYCODE_PAGE_UP = ES_SCANCODE_PAGE_UP;
const int UI_KEYCODE_PAGE_DOWN = ES_SCANCODE_PAGE_DOWN;

int _UIWindowMessage(UIElement *element, UIMessage message, int di, void *dp) {
	if (message == UI_MSG_DEALLOCATE) {
		// TODO Non-main windows.
		element->window = NULL;
		EsInstanceCloseReference(ui.instance);
	}

	return _UIWindowMessageCommon(element, message, di, dp);
}

void UIInitialise() {
	_UIInitialiseCommon();

	while (true) {
		EsMessage *message = EsMessageReceive();

		if (message->type == ES_MSG_INSTANCE_CREATE) {
			ui.instance = EsInstanceCreate(message, NULL, 0);
			break;
		}
	}
}

bool _UIMessageLoopSingle(int *result) {
	if (ui.animating) {
		// TODO.
	} else {
		_UIMessageProcess(EsMessageReceive());
	}

	return true;
}

UIMenu *UIMenuCreate(UIElement *parent, uint32_t flags) {
	ui.menuIndex = 0;
	return EsMenuCreate(parent->window->window, ES_MENU_AT_CURSOR);
}

void _UIMenuItemCallback(EsMenu *menu, EsGeneric context) {
	((void (*)(void *)) ui.menuData[context.u * 2 + 0])(ui.menuData[context.u * 2 + 1]);
}

void UIMenuAddItem(UIMenu *menu, uint32_t flags, const char *label, ptrdiff_t labelBytes, void (*invoke)(void *cp), void *cp) {
	EsAssert(ui.menuIndex < 128);
	ui.menuData[ui.menuIndex * 2 + 0] = (void *) invoke;
	ui.menuData[ui.menuIndex * 2 + 1] = cp;
	EsMenuAddItem(menu, (flags & UI_BUTTON_CHECKED) ? ES_MENU_ITEM_CHECKED : ES_FLAGS_DEFAULT,
			label, labelBytes, _UIMenuItemCallback, ui.menuIndex);
	ui.menuIndex++;
}

void UIMenuShow(UIMenu *menu) {
	EsMenuShow(menu);
}

int _UIWindowCanvasMessage(EsElement *element, EsMessage *message) {
	UIWindow *window = (UIWindow *) element->window->userData.p;

	if (!window) {
		return 0;
	} else if (message->type == ES_MSG_PAINT) {
		EsRectangle bounds = ES_RECT_4PD(message->painter->offsetX, message->painter->offsetY, window->width, window->height);
		EsDrawBitmap(message->painter, bounds, window->bits, window->width * 4, 0xFFFF);
	} else if (message->type == ES_MSG_LAYOUT) {
		EsElementGetSize(element, &window->width, &window->height);
		window->bits = (uint32_t *) UI_REALLOC(window->bits, window->width * window->height * 4);
		window->e.bounds = UI_RECT_2S(window->width, window->height);
		window->e.clip = UI_RECT_2S(window->width, window->height);
		UIElementRelayout(&window->e);
		_UIUpdate();
	} else if (message->type == ES_MSG_SCROLL_WHEEL) {
		_UIWindowInputEvent(window, UI_MSG_MOUSE_WHEEL, -message->scrollWheel.dy, 0);
	} else if (message->type == ES_MSG_MOUSE_MOVED || message->type == ES_MSG_HOVERED_END
			|| message->type == ES_MSG_MOUSE_LEFT_DRAG || message->type == ES_MSG_MOUSE_RIGHT_DRAG || message->type == ES_MSG_MOUSE_MIDDLE_DRAG) {
		EsPoint point = EsMouseGetPosition(element);
		window->cursorX = point.x, window->cursorY = point.y;
		_UIWindowInputEvent(window, UI_MSG_MOUSE_MOVE, 0, 0);
	} else if (message->type == ES_MSG_KEY_UP) {
		window->ctrl = EsKeyboardIsCtrlHeld();
		window->shift = EsKeyboardIsShiftHeld();
		window->alt = EsKeyboardIsAltHeld();
	} else if (message->type == ES_MSG_KEY_DOWN) {
		window->ctrl = EsKeyboardIsCtrlHeld();
		window->shift = EsKeyboardIsShiftHeld();
		window->alt = EsKeyboardIsAltHeld();
		UIKeyTyped m = { 0 };
		char c[64];
		m.text = c;
		m.textBytes = EsMessageGetInputText(message, c);
		m.code = message->keyboard.scancode;
		return _UIWindowInputEvent(window, UI_MSG_KEY_TYPED, 0, &m) ? ES_HANDLED : 0;
	} else if (message->type == ES_MSG_MOUSE_LEFT_CLICK) {
		_UIInspectorSetFocusedWindow(window);
	} else if (message->type == ES_MSG_USER_START) {
		UIElementMessage(&window->e, (UIMessage) message->user.context1.u, 0, (void *) message->user.context2.p);
		_UIUpdate();
	} else if (message->type == ES_MSG_GET_CURSOR) {
		message->cursorStyle = ES_CURSOR_NORMAL;
		if (window->cursor == UI_CURSOR_TEXT)              message->cursorStyle = ES_CURSOR_TEXT;
		if (window->cursor == UI_CURSOR_SPLIT_V)           message->cursorStyle = ES_CURSOR_SPLIT_VERTICAL;
		if (window->cursor == UI_CURSOR_SPLIT_H)           message->cursorStyle = ES_CURSOR_SPLIT_HORIZONTAL;
		if (window->cursor == UI_CURSOR_FLIPPED_ARROW)     message->cursorStyle = ES_CURSOR_SELECT_LINES;
		if (window->cursor == UI_CURSOR_CROSS_HAIR)        message->cursorStyle = ES_CURSOR_CROSS_HAIR_PICK;
		if (window->cursor == UI_CURSOR_HAND)              message->cursorStyle = ES_CURSOR_HAND_HOVER;
		if (window->cursor == UI_CURSOR_RESIZE_UP)         message->cursorStyle = ES_CURSOR_RESIZE_VERTICAL;
		if (window->cursor == UI_CURSOR_RESIZE_LEFT)       message->cursorStyle = ES_CURSOR_RESIZE_HORIZONTAL;
		if (window->cursor == UI_CURSOR_RESIZE_UP_RIGHT)   message->cursorStyle = ES_CURSOR_RESIZE_DIAGONAL_1;
		if (window->cursor == UI_CURSOR_RESIZE_UP_LEFT)    message->cursorStyle = ES_CURSOR_RESIZE_DIAGONAL_2;
		if (window->cursor == UI_CURSOR_RESIZE_DOWN)       message->cursorStyle = ES_CURSOR_RESIZE_VERTICAL;
		if (window->cursor == UI_CURSOR_RESIZE_RIGHT)      message->cursorStyle = ES_CURSOR_RESIZE_HORIZONTAL;
		if (window->cursor == UI_CURSOR_RESIZE_DOWN_RIGHT) message->cursorStyle = ES_CURSOR_RESIZE_DIAGONAL_1;
		if (window->cursor == UI_CURSOR_RESIZE_DOWN_LEFT)  message->cursorStyle = ES_CURSOR_RESIZE_DIAGONAL_2;
	}

	else if (message->type == ES_MSG_MOUSE_LEFT_DOWN)   _UIWindowInputEvent(window, UI_MSG_LEFT_DOWN, 0, 0);
	else if (message->type == ES_MSG_MOUSE_LEFT_UP)     _UIWindowInputEvent(window, UI_MSG_LEFT_UP, 0, 0);
	else if (message->type == ES_MSG_MOUSE_MIDDLE_DOWN) _UIWindowInputEvent(window, UI_MSG_MIDDLE_DOWN, 0, 0);
	else if (message->type == ES_MSG_MOUSE_MIDDLE_UP)   _UIWindowInputEvent(window, UI_MSG_MIDDLE_UP, 0, 0);
	else if (message->type == ES_MSG_MOUSE_RIGHT_DOWN)  _UIWindowInputEvent(window, UI_MSG_RIGHT_DOWN, 0, 0);
	else if (message->type == ES_MSG_MOUSE_RIGHT_UP)    _UIWindowInputEvent(window, UI_MSG_RIGHT_UP, 0, 0);

	else return 0;

	return ES_HANDLED;
}

UIWindow *UIWindowCreate(UIWindow *owner, uint32_t flags, const char *cTitle, int width, int height) {
	_UIMenusClose();

	UIWindow *window = (UIWindow *) UIElementCreate(sizeof(UIWindow), NULL, flags | UI_ELEMENT_WINDOW, _UIWindowMessage, "Window");
	_UIWindowAdd(window);
	if (owner) window->scale = owner->scale;

	if (flags & UI_WINDOW_MENU) {
		// TODO.
	} else {
		// TODO Non-main windows.
		window->window = ui.instance->window;
		window->window->userData = window;
		window->canvas = EsCustomElementCreate(window->window, ES_CELL_FILL | ES_ELEMENT_FOCUSABLE);
		window->canvas->messageUser = _UIWindowCanvasMessage;
		EsWindowSetTitle(window->window, cTitle, -1);
		EsElementFocus(window->canvas);
	}

	return window;
}

void _UIWindowEndPaint(UIWindow *window, UIPainter *painter) {
	EsElementRepaint(window->canvas, &window->updateRegion);
}

void _UIWindowSetCursor(UIWindow *window, int cursor) {
	window->cursor = cursor;
}

void _UIWindowGetScreenPosition(UIWindow *window, int *_x, int *_y) {
	EsRectangle r = EsElementGetScreenBounds(window->window);
	*_x = r.l, *_y = r.t;
}

void UIWindowPostMessage(UIWindow *window, UIMessage message, void *_dp) {
	EsMessage m = {};
	m.type = ES_MSG_USER_START;
	m.user.context1.u = message;
	m.user.context2.p = _dp;
	EsMessagePost(window->canvas, &m);
}

void _UIClipboardWriteText(UIWindow *window, char *text) {
	EsClipboardAddText(ES_CLIPBOARD_PRIMARY, text, -1);
	UI_FREE(text);
}

char *_UIClipboardReadTextStart(UIWindow *window, size_t *bytes) {
	return EsClipboardReadText(ES_CLIPBOARD_PRIMARY, bytes, NULL);
}

void _UIClipboardReadTextEnd(UIWindow *window, char *text) {
	EsHeapFree(text);
}