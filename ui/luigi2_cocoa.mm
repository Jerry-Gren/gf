// TODO Standard keyboard shortcuts (Command+Q, Command+W).

const int UI_KEYCODE_A = -100; // TODO Keyboard layout support.
const int UI_KEYCODE_F1 = -70;
const int UI_KEYCODE_0 = -50;
const int UI_KEYCODE_INSERT = -30;

const int UI_KEYCODE_BACKSPACE = kVK_Delete;
const int UI_KEYCODE_DELETE = kVK_ForwardDelete;
const int UI_KEYCODE_DOWN = kVK_DownArrow;
const int UI_KEYCODE_END = kVK_End;
const int UI_KEYCODE_ENTER = kVK_Return;
const int UI_KEYCODE_ESCAPE = kVK_Escape;
const int UI_KEYCODE_HOME = kVK_Home;
const int UI_KEYCODE_LEFT = kVK_LeftArrow;
const int UI_KEYCODE_RIGHT = kVK_RightArrow;
const int UI_KEYCODE_SPACE = kVK_Space;
const int UI_KEYCODE_TAB = kVK_Tab;
const int UI_KEYCODE_UP = kVK_UpArrow;
const int UI_KEYCODE_BACKTICK = kVK_ANSI_Grave; // TODO Keyboard layout support.
const int UI_KEYCODE_PAGE_UP = kVK_PageUp;
const int UI_KEYCODE_PAGE_DOWN = kVK_PageDown;

int (*_cocoaAppMain)(int, char **);
int _cocoaArgc;
char **_cocoaArgv;

struct _UIPostedMessage {
	UIMessage message;
	void *dp;
};

char *_UIUTF8StringFromNSString(NSString *string) {
	NSUInteger size = [string lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	char *buffer = (char *) UI_MALLOC(size + 1);
	buffer[size] = 0;
	[string getBytes:buffer maxLength:size usedLength:NULL encoding:NSUTF8StringEncoding options:0 range:NSMakeRange(0, [string length]) remainingRange:NULL];
	return buffer;
}

int _UICocoaRemapKey(int code) {
	if (code == kVK_ANSI_A) { return UI_KEYCODE_LETTER('A'); }
	if (code == kVK_ANSI_B) { return UI_KEYCODE_LETTER('B'); }
	if (code == kVK_ANSI_C) { return UI_KEYCODE_LETTER('C'); }
	if (code == kVK_ANSI_D) { return UI_KEYCODE_LETTER('D'); }
	if (code == kVK_ANSI_E) { return UI_KEYCODE_LETTER('E'); }
	if (code == kVK_ANSI_F) { return UI_KEYCODE_LETTER('F'); }
	if (code == kVK_ANSI_G) { return UI_KEYCODE_LETTER('G'); }
	if (code == kVK_ANSI_H) { return UI_KEYCODE_LETTER('H'); }
	if (code == kVK_ANSI_I) { return UI_KEYCODE_LETTER('I'); }
	if (code == kVK_ANSI_J) { return UI_KEYCODE_LETTER('J'); }
	if (code == kVK_ANSI_K) { return UI_KEYCODE_LETTER('K'); }
	if (code == kVK_ANSI_L) { return UI_KEYCODE_LETTER('L'); }
	if (code == kVK_ANSI_M) { return UI_KEYCODE_LETTER('M'); }
	if (code == kVK_ANSI_N) { return UI_KEYCODE_LETTER('N'); }
	if (code == kVK_ANSI_O) { return UI_KEYCODE_LETTER('O'); }
	if (code == kVK_ANSI_P) { return UI_KEYCODE_LETTER('P'); }
	if (code == kVK_ANSI_Q) { return UI_KEYCODE_LETTER('Q'); }
	if (code == kVK_ANSI_R) { return UI_KEYCODE_LETTER('R'); }
	if (code == kVK_ANSI_S) { return UI_KEYCODE_LETTER('S'); }
	if (code == kVK_ANSI_T) { return UI_KEYCODE_LETTER('T'); }
	if (code == kVK_ANSI_U) { return UI_KEYCODE_LETTER('U'); }
	if (code == kVK_ANSI_V) { return UI_KEYCODE_LETTER('V'); }
	if (code == kVK_ANSI_W) { return UI_KEYCODE_LETTER('W'); }
	if (code == kVK_ANSI_X) { return UI_KEYCODE_LETTER('X'); }
	if (code == kVK_ANSI_Y) { return UI_KEYCODE_LETTER('Y'); }
	if (code == kVK_ANSI_Z) { return UI_KEYCODE_LETTER('Z'); }

	if (code == kVK_ANSI_0) { return UI_KEYCODE_DIGIT('0'); }
	if (code == kVK_ANSI_1) { return UI_KEYCODE_DIGIT('1'); }
	if (code == kVK_ANSI_2) { return UI_KEYCODE_DIGIT('2'); }
	if (code == kVK_ANSI_3) { return UI_KEYCODE_DIGIT('3'); }
	if (code == kVK_ANSI_4) { return UI_KEYCODE_DIGIT('4'); }
	if (code == kVK_ANSI_5) { return UI_KEYCODE_DIGIT('5'); }
	if (code == kVK_ANSI_6) { return UI_KEYCODE_DIGIT('6'); }
	if (code == kVK_ANSI_7) { return UI_KEYCODE_DIGIT('7'); }
	if (code == kVK_ANSI_8) { return UI_KEYCODE_DIGIT('8'); }
	if (code == kVK_ANSI_9) { return UI_KEYCODE_DIGIT('9'); }

	if (code == kVK_F1)  { return UI_KEYCODE_FKEY( 1); }
	if (code == kVK_F2)  { return UI_KEYCODE_FKEY( 2); }
	if (code == kVK_F3)  { return UI_KEYCODE_FKEY( 3); }
	if (code == kVK_F4)  { return UI_KEYCODE_FKEY( 4); }
	if (code == kVK_F5)  { return UI_KEYCODE_FKEY( 5); }
	if (code == kVK_F6)  { return UI_KEYCODE_FKEY( 6); }
	if (code == kVK_F7)  { return UI_KEYCODE_FKEY( 7); }
	if (code == kVK_F8)  { return UI_KEYCODE_FKEY( 8); }
	if (code == kVK_F9)  { return UI_KEYCODE_FKEY( 9); }
	if (code == kVK_F10) { return UI_KEYCODE_FKEY(10); }
	if (code == kVK_F11) { return UI_KEYCODE_FKEY(11); }
	if (code == kVK_F12) { return UI_KEYCODE_FKEY(12); }

	return code;
}

@interface UICocoaApplicationDelegate : NSObject<NSApplicationDelegate>
@end

@interface UICocoaWindowDelegate : NSObject<NSWindowDelegate>
@property (nonatomic) UIWindow *uiWindow;
@end

@interface UICocoaMainView : NSView
- (void)handlePostedMessage:(id)message;
- (void)eventCommon:(NSEvent *)event;
@property (nonatomic) UIWindow *uiWindow;
@end

@implementation UICocoaApplicationDelegate
- (void)applicationWillFinishLaunching:(NSNotification *)notification {
	int code = _cocoaAppMain(_cocoaArgc, _cocoaArgv);
	if (code) exit(code);
}
@end

@implementation UICocoaWindowDelegate
- (void)windowDidBecomeKey:(NSNotification *)notification {
	UIElementMessage(&_uiWindow->e, UI_MSG_WINDOW_ACTIVATE, 0, 0);
	_UIUpdate();
}

- (void)windowDidResize:(NSNotification *)notification {
	_uiWindow->width = ((UICocoaMainView *) _uiWindow->view).frame.size.width;
	_uiWindow->height = ((UICocoaMainView *) _uiWindow->view).frame.size.height;
	_uiWindow->bits = (uint32_t *) UI_REALLOC(_uiWindow->bits, _uiWindow->width * _uiWindow->height * 4);
	_uiWindow->e.bounds = UI_RECT_2S(_uiWindow->width, _uiWindow->height);
	_uiWindow->e.clip = UI_RECT_2S(_uiWindow->width, _uiWindow->height);
	UIElementRelayout(&_uiWindow->e);
	_UIUpdate();
}
@end

@implementation UICocoaMainView
- (void)handlePostedMessage:(id)_message {
	_UIPostedMessage *message = (_UIPostedMessage *) _message;
	_UIWindowInputEvent(_uiWindow, message->message, 0, message->dp);
	UI_FREE(message);
}

- (BOOL)acceptsFirstResponder {
	return YES;
}

- (void)onMenuItemSelected:(NSMenuItem *)menuItem {
	((void (*)(void *)) ui.menuData[menuItem.tag * 2 + 0])(ui.menuData[menuItem.tag * 2 + 1]);
}

- (void)drawRect:(NSRect)dirtyRect {
	const unsigned char *data = (const unsigned char *) _uiWindow->bits;
	NSDrawBitmap(NSMakeRect(0, 0, _uiWindow->width, _uiWindow->height), _uiWindow->width, _uiWindow->height,
			8 /* bits per channel */, 4 /* channels per pixel */,
			32 /* bits per pixel */, 4 * _uiWindow->width /* bytes per row */, NO /* planar */, YES /* has alpha */,
			NSDeviceRGBColorSpace /* color space */, &data /* data */);
}

- (void)eventCommon:(NSEvent *)event {
	NSPoint cursor = [self convertPoint:[event locationInWindow] fromView:nil];
	_uiWindow->cursorX = cursor.x, _uiWindow->cursorY = _uiWindow->height - cursor.y - 1;
	_uiWindow->ctrl = event.modifierFlags & NSEventModifierFlagCommand;
	_uiWindow->shift = event.modifierFlags & NSEventModifierFlagShift;
	_uiWindow->alt = event.modifierFlags & NSEventModifierFlagOption;
}

- (void)keyDown:(NSEvent *)event {
	[self eventCommon:event];
	char *text = _UIUTF8StringFromNSString(event.characters);
	UIKeyTyped m = { .code = _UICocoaRemapKey(event.keyCode), .text = text, .textBytes = (int) strlen(text) };
	_UIWindowInputEvent(_uiWindow, UI_MSG_KEY_TYPED, 0, &m);
	UI_FREE(text);
}

- (void)keyUp:(NSEvent *)event {
	[self eventCommon:event];
	UIKeyTyped m = { .code = _UICocoaRemapKey(event.keyCode) };
	_UIWindowInputEvent(_uiWindow, UI_MSG_KEY_RELEASED, 0, &m);
}

- (void)mouseMoved:(NSEvent *)event {
	[self eventCommon:event];
	_UIWindowInputEvent(_uiWindow, UI_MSG_MOUSE_MOVE, 0, 0);
}

- (void)mouseExited:(NSEvent *)event { [self mouseMoved:event]; }
- (void)flagsChanged:(NSEvent *)event { [self mouseMoved:event]; }
- (void)mouseDragged:(NSEvent *)event { [self mouseMoved:event]; }
- (void)rightMouseDragged:(NSEvent *)event { [self mouseMoved:event]; }
- (void)otherMouseDragged:(NSEvent *)event { [self mouseMoved:event]; }

- (void)mouseDown:(NSEvent *)event {
	[self eventCommon:event];
	_UIWindowInputEvent(_uiWindow, UI_MSG_LEFT_DOWN, 0, 0);
}

- (void)mouseUp:(NSEvent *)event {
	[self eventCommon:event];
	_UIWindowInputEvent(_uiWindow, UI_MSG_LEFT_UP, 0, 0);
}

- (void)rightMouseDown:(NSEvent *)event {
	[self eventCommon:event];
	_UIWindowInputEvent(_uiWindow, UI_MSG_RIGHT_DOWN, 0, 0);
}

- (void)rightMouseUp:(NSEvent *)event {
	[self eventCommon:event];
	_UIWindowInputEvent(_uiWindow, UI_MSG_RIGHT_UP, 0, 0);
}

- (void)otherMouseDown:(NSEvent *)event {
	[self eventCommon:event];
	_UIWindowInputEvent(_uiWindow, UI_MSG_MIDDLE_DOWN, 0, 0);
}

- (void)otherMouseUp:(NSEvent *)event {
	[self eventCommon:event];
	_UIWindowInputEvent(_uiWindow, UI_MSG_MIDDLE_UP, 0, 0);
}

- (void)scrollWheel:(NSEvent *)event {
	[self eventCommon:event];
	_UIWindowInputEvent(_uiWindow, UI_MSG_MOUSE_WHEEL, -3 * event.deltaY, 0);
	_UIWindowInputEvent(_uiWindow, UI_MSG_MOUSE_MOVE, 0, 0);
}

// TODO Animations.
// TODO Drag and drop.
// TODO Reporting window close.

@end

int _UIWindowMessage(UIElement *element, UIMessage message, int di, void *dp) {
	if (message == UI_MSG_DEALLOCATE) {
		UIWindow *window = (UIWindow *) element;
		_UIWindowDestroyCommon(window);
		[window->window close];
	}

	return _UIWindowMessageCommon(element, message, di, dp);
}

UIWindow *UIWindowCreate(UIWindow *owner, uint32_t flags, const char *cTitle, int _width, int _height) {
	_UIMenusClose();
	UIWindow *window = (UIWindow *) UIElementCreate(sizeof(UIWindow), NULL, flags | UI_ELEMENT_WINDOW, _UIWindowMessage, "Window");
	_UIWindowAdd(window);
	if (owner) window->scale = owner->scale;

	NSRect frame = NSMakeRect(0, 0, _width ?: 800, _height ?: 600);
	NSWindowStyleMask styleMask = NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskTitled;
	NSWindow *nsWindow = [[NSWindow alloc] initWithContentRect:frame styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];
	[nsWindow center];
	[nsWindow setTitle:@(cTitle ?: "untitled")];
	UICocoaWindowDelegate *delegate = [UICocoaWindowDelegate alloc];
	[delegate setUiWindow:window];
	nsWindow.delegate = delegate;
	UICocoaMainView *view = [UICocoaMainView alloc];
	window->window = nsWindow;
	window->view = view;
	window->width = frame.size.width;
	window->height = frame.size.height;
	window->bits = (uint32_t *) UI_REALLOC(window->bits, window->width * window->height * 4);
	window->e.bounds = UI_RECT_2S(window->width, window->height);
	window->e.clip = UI_RECT_2S(window->width, window->height);
	[view setUiWindow:window];
	[view initWithFrame:frame];
	nsWindow.contentView = view;
	[view addTrackingArea:[[NSTrackingArea alloc] initWithRect:frame
		options:NSTrackingMouseMoved|NSTrackingActiveInKeyWindow|NSTrackingInVisibleRect owner:view userInfo:nil]];
	[nsWindow setInitialFirstResponder:view];
	[nsWindow makeKeyAndOrderFront:delegate];

	// TODO UI_WINDOW_MAXIMIZE.

	return window;
}

void _UIClipboardWriteText(UIWindow *window, char *text) {
	// TODO Clipboard support.
}

char *_UIClipboardReadTextStart(UIWindow *window, size_t *bytes) {
	// TODO Clipboard support.
	return NULL;
}

void _UIClipboardReadTextEnd(UIWindow *window, char *text) {
	UI_FREE(text);
}

void UIInitialise() {
	_UIInitialiseCommon();
}

void _UIWindowSetCursor(UIWindow *window, int cursor) {
	if      (cursor == UI_CURSOR_TEXT)          [[NSCursor IBeamCursor] set];
	else if (cursor == UI_CURSOR_SPLIT_V)       [[NSCursor resizeUpDownCursor] set];
	else if (cursor == UI_CURSOR_SPLIT_H)       [[NSCursor resizeLeftRightCursor] set];
	else if (cursor == UI_CURSOR_FLIPPED_ARROW) [[NSCursor pointingHandCursor] set];
	else if (cursor == UI_CURSOR_CROSS_HAIR)    [[NSCursor crosshairCursor] set];
	else if (cursor == UI_CURSOR_HAND)          [[NSCursor pointingHandCursor] set];
	else                                        [[NSCursor arrowCursor] set];
}

void _UIWindowEndPaint(UIWindow *window, UIPainter *painter) {
	for (int y = painter->clip.t; y < painter->clip.b; y++) {
		for (int x = painter->clip.l; x < painter->clip.r; x++) {
			uint32_t *p = &painter->bits[y * painter->width + x];
			*p = 0xFF000000 | (*p & 0xFF00) | ((*p & 0xFF0000) >> 16) | ((*p & 0xFF) << 16);
		}
	}

	[(UICocoaMainView *)window->view setNeedsDisplayInRect:((UICocoaMainView *)window->view).frame];
}

void _UIWindowGetScreenPosition(UIWindow *window, int *x, int *y) {
	NSPoint point = [window->window convertPointToScreen:NSMakePoint(0, 0)];
	*x = point.x, *y = point.y;
}

UIMenu *UIMenuCreate(UIElement *parent, uint32_t flags) {
	// TODO Fix the vertical position.

	if (parent->parent) {
		UIRectangle screenBounds = UIElementScreenBounds(parent);
		ui.menuX = screenBounds.l;
		ui.menuY = screenBounds.b;
	} else {
		_UIWindowGetScreenPosition(parent->window, &ui.menuX, &ui.menuY);
		ui.menuX += parent->window->cursorX;
		ui.menuY += parent->window->cursorY;
	}

	ui.menuIndex = 0;
	ui.menuWindow = parent->window;

	NSMenu *menu = [[NSMenu alloc] init];
	[menu setAutoenablesItems:NO];
	return menu;
}

void UIMenuAddItem(UIMenu *menu, uint32_t flags, const char *label, ptrdiff_t labelBytes, void (*invoke)(void *cp), void *cp) {
	if (ui.menuIndex == 128) return;
	ui.menuData[ui.menuIndex * 2 + 0] = (void *) invoke;
	ui.menuData[ui.menuIndex * 2 + 1] = cp;
	NSString *title = [[NSString alloc] initWithBytes:label length:(labelBytes == -1 ? strlen(label) : labelBytes) encoding:NSUTF8StringEncoding];
	NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(onMenuItemSelected:) keyEquivalent:@""];
	item.tag = ui.menuIndex++;
	if (flags & UI_BUTTON_CHECKED) [item setState:NSControlStateValueOn];
	[item setEnabled:((flags & UI_ELEMENT_DISABLED) ? NO : YES)];
	[item setTarget:(UICocoaMainView *)ui.menuWindow->view];
	[menu addItem:item];
	[title release];
	[item release];
}

void UIMenuShow(UIMenu *menu) {
	[menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(ui.menuX, ui.menuY) inView:nil];
	[menu release];
}

void UIWindowPack(UIWindow *window, int _width) {
	int width = _width ? _width : UIElementMessage(window->e.children[0], UI_MSG_GET_WIDTH, 0, 0);
	int height = UIElementMessage(window->e.children[0], UI_MSG_GET_HEIGHT, width, 0);
	[window->window setContentSize:NSMakeSize(width, height)];
}

bool _UIMessageLoopSingle(int *result) {
	// TODO Modal dialog support.
	return false;
}

void UIWindowPostMessage(UIWindow *window, UIMessage _message, void *dp) {
	_UIPostedMessage *message = (_UIPostedMessage *) UI_MALLOC(sizeof(_UIPostedMessage));
	message->message = _message;
	message->dp = dp;
	[(UICocoaMainView*)window->view performSelectorOnMainThread:@selector(handlePostedMessage:) withObject:(id)message waitUntilDone:NO];
}

int UICocoaMain(int argc, char **argv, int (*appMain)(int, char **)) {
	_cocoaArgc = argc, _cocoaArgv = argv, _cocoaAppMain = appMain;
	NSApplication *application = [NSApplication sharedApplication];
	application.delegate = [[UICocoaApplicationDelegate alloc] init];
	return NSApplicationMain(argc, (const char **) argv);
}