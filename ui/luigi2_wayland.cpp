#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>

#ifndef __NR_memfd_create
#include <sys/syscall.h>
#define memfd_create(name, flags) syscall(__NR_memfd_create, name, flags)
#endif

extern "C" {
#include "../xdg-shell-client-protocol.h"
#include "../xdg-shell-protocol.c"
}

/////////////////////////////////////////
// Keyboard definitions
/////////////////////////////////////////

const int UI_KEYCODE_A = XKB_KEY_a;
const int UI_KEYCODE_BACKSPACE = XKB_KEY_BackSpace;
const int UI_KEYCODE_DELETE = XKB_KEY_Delete;
const int UI_KEYCODE_DOWN = XKB_KEY_Down;
const int UI_KEYCODE_END = XKB_KEY_End;
const int UI_KEYCODE_ENTER = XKB_KEY_Return;
const int UI_KEYCODE_ESCAPE = XKB_KEY_Escape;
const int UI_KEYCODE_F1 = XKB_KEY_F1;
const int UI_KEYCODE_HOME = XKB_KEY_Home;
const int UI_KEYCODE_LEFT = XKB_KEY_Left;
const int UI_KEYCODE_RIGHT = XKB_KEY_Right;
const int UI_KEYCODE_SPACE = XKB_KEY_space;
const int UI_KEYCODE_TAB = XKB_KEY_Tab;
const int UI_KEYCODE_UP = XKB_KEY_Up;
const int UI_KEYCODE_INSERT = XKB_KEY_Insert;
const int UI_KEYCODE_0 = XKB_KEY_0;
const int UI_KEYCODE_BACKTICK = XKB_KEY_grave;
const int UI_KEYCODE_PAGE_DOWN = XKB_KEY_Page_Down;
const int UI_KEYCODE_PAGE_UP = XKB_KEY_Page_Up;

/////////////////////////////////////////
// Forward declarations
/////////////////////////////////////////

void _UIWindowDestroyCommon(UIWindow *window);
void _UIInitialiseCommon();
int _UIWindowMessageCommon(UIElement *element, UIMessage message, int di, void *dp);
void _UIWindowAdd(UIWindow *window);
void _UIProcessAnimations();
void _UIUpdate();
bool _UIWindowInputEvent(UIWindow *window, UIMessage message, int di, void *dp);
void _UIWindowSetCursor(UIWindow *window, int cursor);
bool _UIMenusClose();

static UIWindow *wl_pointer_window = nullptr;
static UIWindow *wl_keyboard_window = nullptr;

static struct xkb_context *ui_wl_xkb_context = nullptr;
static struct xkb_keymap *ui_wl_xkb_keymap = nullptr;
static struct xkb_state *ui_wl_xkb_state = nullptr;

static struct wl_cursor_theme *ui_wl_cursor_theme = nullptr;
static struct wl_surface *ui_wl_cursor_surface = nullptr;

static struct wl_data_device_manager *ui_wl_data_device_manager = nullptr;
static struct wl_data_device *ui_wl_data_device = nullptr;
static struct wl_data_offer *ui_wl_selection_offer = nullptr;
static char *ui_wl_clipboard_text = nullptr;

static uint32_t ui_wl_pointer_enter_serial = 0;
static uint32_t ui_wl_last_serial = 0;
static int ui_wl_cursor_current_scale = 0;

static int ui_wl_wakeup_pipe[2];

struct UIPostedMessage {
    UIWindow *window;
    UIMessage message;
    void *dp;
};

/////////////////////////////////////////
// Frame Sync
/////////////////////////////////////////

struct WaylandWindowData {
	struct wl_callback *frame_callback;
	bool frame_pending;
	UIRectangle pending_updateRegion;
};

struct WaylandWindowMap {
	UIWindow *window;
	WaylandWindowData data;
};

static WaylandWindowMap wl_window_map[32]; // Support up to 32 simultaneous UIWindows

static WaylandWindowData* _GetWindowData(UIWindow *window) {
	for (int i = 0; i < 32; i++) {
		if (wl_window_map[i].window == window) return &wl_window_map[i].data;
		if (wl_window_map[i].window == nullptr) {
		wl_window_map[i].window = window;
		wl_window_map[i].data.frame_callback = nullptr;
		wl_window_map[i].data.frame_pending = false;
		wl_window_map[i].data.pending_updateRegion = UI_RECT_1(0);
		return &wl_window_map[i].data;
		}
	}
	return nullptr;
}

static void _FreeWindowData(UIWindow *window) {
	for (int i = 0; i < 32; i++) {
		if (wl_window_map[i].window == window) {
		if (wl_window_map[i].data.frame_callback) {
			wl_callback_destroy(wl_window_map[i].data.frame_callback);
		}
		wl_window_map[i].window = nullptr;
		break;
		}
	}
}

static void _UIWaylandFrameDone(void *data, struct wl_callback *callback, uint32_t time) {
	UIWindow *window = (UIWindow *) data;
	WaylandWindowData *wdata = _GetWindowData(window);
	if (wdata) {
		if (wdata->frame_callback) {
		wl_callback_destroy(wdata->frame_callback);
		wdata->frame_callback = nullptr;
		}
		wdata->frame_pending = false;
		
		// Restore accumulated damage so it gets painted in the next loop
		if (UI_RECT_VALID(wdata->pending_updateRegion)) {
		if (UI_RECT_VALID(window->updateRegion)) {
			window->updateRegion = UIRectangleBounding(window->updateRegion, wdata->pending_updateRegion);
		} else {
			window->updateRegion = wdata->pending_updateRegion;
		}
		wdata->pending_updateRegion = UI_RECT_1(0);
		}
	}
}

static const struct wl_callback_listener _uiWaylandFrameListener = {
	/* .done = */ _UIWaylandFrameDone
};

/////////////////////////////////////////
// Pointer
/////////////////////////////////////////

static void _UIWaylandPointerEnter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	ui_wl_pointer_enter_serial = serial;
	if (!surface) return;
	wl_pointer_window = (UIWindow *) wl_surface_get_user_data(surface);
	if (wl_pointer_window) {
		int scale = (int)(wl_pointer_window->scale + 0.5f);
		if (scale < 1) scale = 1;
		wl_pointer_window->cursorX = wl_fixed_to_int(surface_x) * scale;
		wl_pointer_window->cursorY = wl_fixed_to_int(surface_y) * scale;
		_UIWindowSetCursor(wl_pointer_window, wl_pointer_window->cursorStyle);
	}
}

static void _UIWaylandPointerLeave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface) {
	ui_wl_last_serial = serial;
	wl_pointer_window = nullptr;
}

static void _UIWaylandPointerMotion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	if (wl_pointer_window) {
		int scale = (int)(wl_pointer_window->scale + 0.5f);
		if (scale < 1) scale = 1;
		wl_pointer_window->cursorX = wl_fixed_to_int(surface_x) * scale;
		wl_pointer_window->cursorY = wl_fixed_to_int(surface_y) * scale;
		_UIWindowInputEvent(wl_pointer_window, UI_MSG_MOUSE_MOVE, 0, 0);
	}
}

static void _UIWaylandPointerButton(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	ui_wl_last_serial = serial;
	if (!wl_pointer_window) return;
	
	UIMessage msg;
	bool is_down = (state == WL_POINTER_BUTTON_STATE_PRESSED);

	if (button == BTN_LEFT) msg = is_down ? UI_MSG_LEFT_DOWN : UI_MSG_LEFT_UP;
	else if (button == BTN_RIGHT) msg = is_down ? UI_MSG_RIGHT_DOWN : UI_MSG_RIGHT_UP;
	else if (button == BTN_MIDDLE) msg = is_down ? UI_MSG_MIDDLE_DOWN : UI_MSG_MIDDLE_UP;
	else return;

	_UIWindowInputEvent(wl_pointer_window, msg, 0, 0);
}

static void _UIWaylandPointerAxis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
	if (!wl_pointer_window || axis != WL_POINTER_AXIS_VERTICAL_SCROLL) return;
	int delta = (int)(wl_fixed_to_double(value) * 15.0f); 
	if (delta == 0) return;
	_UIWindowInputEvent(wl_pointer_window, UI_MSG_MOUSE_WHEEL, delta, 0);
}

static const struct wl_pointer_listener _uiWaylandPointerListener = {
	/* .enter = */ _UIWaylandPointerEnter,
	/* .leave = */ _UIWaylandPointerLeave,
	/* .motion = */ _UIWaylandPointerMotion,
	/* .button = */ _UIWaylandPointerButton,
	/* .axis = */ _UIWaylandPointerAxis,
	/* .frame = */ [](void *, struct wl_pointer *) {},
	/* .axis_source = */ [](void *, struct wl_pointer *, uint32_t) {},
	/* .axis_stop = */ [](void *, struct wl_pointer *, uint32_t, uint32_t) {},
	/* .axis_discrete = */ [](void *, struct wl_pointer *, uint32_t, int32_t) {}
};

/////////////////////////////////////////
// Keyboard
/////////////////////////////////////////

static void _UIWaylandKeyboardKeymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size) {
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }

	char *map_shm = (char *) mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (map_shm == MAP_FAILED) { close(fd); return; }

	struct xkb_keymap *keymap = xkb_keymap_new_from_string(ui_wl_xkb_context, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map_shm, size);
	close(fd);

	if (ui_wl_xkb_state) xkb_state_unref(ui_wl_xkb_state);
	if (ui_wl_xkb_keymap) xkb_keymap_unref(ui_wl_xkb_keymap);
	
	ui_wl_xkb_keymap = keymap;
	ui_wl_xkb_state = xkb_state_new(keymap);
}

static void _UIWaylandKeyboardEnter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
	ui_wl_last_serial = serial;
	if (!surface) return;
	wl_keyboard_window = (UIWindow *) wl_surface_get_user_data(surface);
	if (wl_keyboard_window) {
		wl_keyboard_window->ctrl = wl_keyboard_window->shift = wl_keyboard_window->alt = false;
		UIElementMessage(&wl_keyboard_window->e, UI_MSG_WINDOW_ACTIVATE, 0, 0);
	}
}

static void _UIWaylandKeyboardLeave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface) {
	ui_wl_last_serial = serial;
	wl_keyboard_window = nullptr;
}

static void _UIWaylandKeyboardKey(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
	ui_wl_last_serial = serial;
	if (!wl_keyboard_window || !ui_wl_xkb_state) return;

	xkb_keysym_t sym = xkb_state_key_get_one_sym(ui_wl_xkb_state, key + 8);
	UIKeyTyped m = { 0 };
	m.code = sym;
	
	char text[64] = {0};
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		m.textBytes = xkb_state_key_get_utf8(ui_wl_xkb_state, key + 8, text, sizeof(text));
		m.text = text;
		_UIWindowInputEvent(wl_keyboard_window, UI_MSG_KEY_TYPED, 0, &m);
	} else {
		_UIWindowInputEvent(wl_keyboard_window, UI_MSG_KEY_RELEASED, 0, &m);
	}
}

static void _UIWaylandKeyboardModifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
	if (!ui_wl_xkb_state) return;
	xkb_state_update_mask(ui_wl_xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
	
	if (wl_keyboard_window) {
		wl_keyboard_window->ctrl = xkb_state_mod_name_is_active(ui_wl_xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE);
		wl_keyboard_window->shift = xkb_state_mod_name_is_active(ui_wl_xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE);
		wl_keyboard_window->alt = xkb_state_mod_name_is_active(ui_wl_xkb_state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE);
	}
}

static const struct wl_keyboard_listener _uiWaylandKeyboardListener = {
	/* .keymap = */ _UIWaylandKeyboardKeymap,
	/* .enter = */ _UIWaylandKeyboardEnter,
	/* .leave = */ _UIWaylandKeyboardLeave,
	/* .key = */ _UIWaylandKeyboardKey,
	/* .modifiers = */ _UIWaylandKeyboardModifiers,
	/* .repeat_info = */ [](void *, struct wl_keyboard *, int32_t, int32_t) {}
};

static void _UIWaylandSeatCapabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {
	if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
		ui.pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(ui.pointer, &_uiWaylandPointerListener, NULL);
	}
	if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
		ui.keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(ui.keyboard, &_uiWaylandKeyboardListener, NULL);
	}
}

static const struct wl_seat_listener _uiWaylandSeatListener = {
	/* .capabilities = */ _UIWaylandSeatCapabilities,
	/* .name = */ [](void *, struct wl_seat *, const char *) {}
};

/////////////////////////////////////////
// Clipboard
/////////////////////////////////////////

static void _UIWaylandDataSourceSend(void *data, struct wl_data_source *source, const char *mime_type, int32_t fd) {
	if (ui_wl_clipboard_text) {
		write(fd, ui_wl_clipboard_text, strlen(ui_wl_clipboard_text));
	}
	close(fd);
}

static const struct wl_data_source_listener _uiWaylandDataSourceListener = {
	/* .target = */ [](void *, struct wl_data_source *, const char *) {},
	/* .send = */ _UIWaylandDataSourceSend,
	/* .cancelled = */ [](void *, struct wl_data_source *source) { wl_data_source_destroy(source); },
	/* .dnd_drop_performed = */ [](void *, struct wl_data_source *) {},
	/* .dnd_finished = */ [](void *, struct wl_data_source *) {},
	/* .action = */ [](void *, struct wl_data_source *, uint32_t) {}
};

static void _UIWaylandDataDeviceSelection(void *data, struct wl_data_device *data_device, struct wl_data_offer *id) {
	if (ui_wl_selection_offer) wl_data_offer_destroy(ui_wl_selection_offer);
	ui_wl_selection_offer = id;
}

static const struct wl_data_device_listener _uiWaylandDataDeviceListener = {
	/* .data_offer = */ [](void *, struct wl_data_device *, struct wl_data_offer *) {},
	/* .enter = */ [](void *, struct wl_data_device *, uint32_t, struct wl_surface *, wl_fixed_t, wl_fixed_t, struct wl_data_offer *) {},
	/* .leave = */ [](void *, struct wl_data_device *) {},
	/* .motion = */ [](void *, struct wl_data_device *, uint32_t, wl_fixed_t, wl_fixed_t) {},
	/* .drop = */ [](void *, struct wl_data_device *) {},
	/* .selection = */ _UIWaylandDataDeviceSelection
};

/////////////////////////////////////////
// SHM Buffer
/////////////////////////////////////////

static int _UIWaylandCreateAnonymousFile(off_t size) {
	int fd = memfd_create("luigi-wayland-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (fd < 0) return -1;
	fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK);
	if (ftruncate(fd, size) < 0) { close(fd); return -1; }
	return fd;
}

static void _UIWaylandReallocateBuffer(UIWindow *window) {
	int stride = window->width * 4;
	int size = stride * window->height;

	if (size > window->shm_capacity) {
		if (window->wl_shm_pool) {
			wl_shm_pool_destroy(window->wl_shm_pool);
			munmap(window->shm_data, window->shm_capacity);
			close(window->shm_fd);
		}
		window->shm_capacity = size * 2;
		window->shm_fd = _UIWaylandCreateAnonymousFile(window->shm_capacity);
		if (window->shm_fd < 0) {
			fprintf(stderr, "Error: Could not allocate shm file descriptor for Wayland.\n");
			exit(EXIT_FAILURE);
		}
		window->shm_data = mmap(NULL, window->shm_capacity, PROT_READ | PROT_WRITE, MAP_SHARED, window->shm_fd, 0);
		if (window->shm_data == MAP_FAILED) {
			fprintf(stderr, "Error: Could not mmap shm buffer.\n");
			close(window->shm_fd);
			exit(EXIT_FAILURE);
		}
		window->wl_shm_pool = wl_shm_create_pool(ui.shm, window->shm_fd, window->shm_capacity);
	}
	if (window->wl_buffer) {
		wl_buffer_destroy(window->wl_buffer);
	}
	window->wl_buffer = wl_shm_pool_create_buffer(window->wl_shm_pool, 0, window->width, window->height, stride, ui.shm_format);
	window->bits = (uint32_t *) window->shm_data;
}

/////////////////////////////////////////
// XDG Surface Listeners
/////////////////////////////////////////

static void _UIWaylandXdgSurfaceConfigure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
	UIWindow *window = (UIWindow *) data;
	xdg_surface_ack_configure(xdg_surface, serial);
	if (window->wait_for_configure) window->wait_for_configure = false;
}

static const struct xdg_surface_listener _uiWaylandXdgSurfaceListener = {
	/* .configure = */ _UIWaylandXdgSurfaceConfigure,
};

static void _UIWaylandXdgToplevelConfigure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
	UIWindow *window = (UIWindow *) data;
	int scale = (int)(window->scale + 0.5f);
	if (scale < 1) scale = 1;

	if (width > 0 && height > 0) {
		int physical_width = width * scale;
		int physical_height = height * scale;

		if (window->width != physical_width || window->height != physical_height) {
			window->width = physical_width;
			window->height = physical_height;
			
			_UIWaylandReallocateBuffer(window);
			window->e.bounds = UI_RECT_2S(window->width, window->height);
			window->e.clip = UI_RECT_2S(window->width, window->height);
			
			if (window->configured) {
				UIElementRelayout(&window->e);
				window->updateRegion = window->e.bounds;
			}
		}
	}
}

static void _UIWaylandXdgToplevelClose(void *data, struct xdg_toplevel *xdg_toplevel) {
	UIWindow *window = (UIWindow *) data;
	if (UIElementMessage(&window->e, UI_MSG_WINDOW_CLOSE, 0, 0)) {
		_UIUpdate();
	} else {
		ui.quit = true;
	}
}

static const struct xdg_toplevel_listener _uiWaylandXdgToplevelListener = {
	/* .configure = */ _UIWaylandXdgToplevelConfigure,
	/* .close = */ _UIWaylandXdgToplevelClose,
};

/////////////////////////////////////////
// XDG Popup Listeners
/////////////////////////////////////////

static void _UIWaylandXdgPopupConfigure(void *data, struct xdg_popup *xdg_popup, int32_t x, int32_t y, int32_t width, int32_t height) {
}

static void _UIWaylandXdgPopupDone(void *data, struct xdg_popup *xdg_popup) {
	// DO NOT call _UIUpdate() directly here to avoid tearing down the surface while wayland is dispatching.
	// Let the main event loop cleanly reap the destroyed window.
	_UIMenusClose();
}

static const struct xdg_popup_listener _uiWaylandXdgPopupListener = {
	/* .configure = */ _UIWaylandXdgPopupConfigure,
	/* .popup_done = */ _UIWaylandXdgPopupDone,
};

/////////////////////////////////////////
// Wayland Registry Listeners
/////////////////////////////////////////

static void _UIWaylandXdgWmBasePing(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
	xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener _uiWaylandXdgWmBaseListener = {
	/* .ping = */ _UIWaylandXdgWmBasePing,
};

static void _UIWaylandShmFormat(void *data, struct wl_shm *wl_shm, uint32_t format) {
	if (format == WL_SHM_FORMAT_ARGB8888 || format == WL_SHM_FORMAT_XRGB8888) {
		ui.shm_format = format;
	}
}

static const struct wl_shm_listener _uiWaylandShmListener = {
	/* .format = */ _UIWaylandShmFormat,
};

static void _UIWaylandRegistryGlobal(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	if (0 == strcmp(interface, wl_compositor_interface.name)) {
		ui.compositor = (struct wl_compositor *) wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	} else if (0 == strcmp(interface, wl_shm_interface.name)) {
		ui.shm = (struct wl_shm *) wl_registry_bind(registry, name, &wl_shm_interface, 1);
		wl_shm_add_listener(ui.shm, &_uiWaylandShmListener, NULL);
	} else if (0 == strcmp(interface, xdg_wm_base_interface.name)) {
		ui.xdg_wm_base = (struct xdg_wm_base *) wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(ui.xdg_wm_base, &_uiWaylandXdgWmBaseListener, NULL);
	} else if (0 == strcmp(interface, wl_seat_interface.name)) {
		ui.seat = (struct wl_seat *) wl_registry_bind(registry, name, &wl_seat_interface, 7);
		wl_seat_add_listener(ui.seat, &_uiWaylandSeatListener, NULL);
	} else if (0 == strcmp(interface, wl_data_device_manager_interface.name)) {
		ui_wl_data_device_manager = (struct wl_data_device_manager *) wl_registry_bind(registry, name, &wl_data_device_manager_interface, 3);
	}
}

static const struct wl_registry_listener _uiWaylandRegistryListener = {
	/* .global = */ _UIWaylandRegistryGlobal,
	/* .global_remove = */ [](void *, struct wl_registry *, uint32_t) {},
};

/////////////////////////////////////////
// Initialisation
/////////////////////////////////////////

void UIInitialise() {
	_UIInitialiseCommon();

	if (pipe(ui_wl_wakeup_pipe) == -1) {
		fprintf(stderr, "Error: Could not create wakeup pipe.\n");
		exit(EXIT_FAILURE);
	}
	fcntl(ui_wl_wakeup_pipe[0], F_SETFL, O_NONBLOCK);

	ui.display = wl_display_connect(NULL);
	if (!ui.display) {
		fprintf(stderr, "Error: Could not connect to Wayland display.\n");
		exit(EXIT_FAILURE);
	}

	ui.registry = wl_display_get_registry(ui.display);
	wl_registry_add_listener(ui.registry, &_uiWaylandRegistryListener, NULL);

	wl_display_roundtrip(ui.display);
	wl_display_roundtrip(ui.display);

	if (!ui.compositor) {
		fprintf(stderr, "Error: Wayland compositor interface not found.\n");
		exit(EXIT_FAILURE);
	}
	if (!ui.shm) {
		fprintf(stderr, "Error: Wayland shm interface not found.\n");
		exit(EXIT_FAILURE);
	}
	if (!ui.xdg_wm_base) {
		fprintf(stderr, "Error: Wayland xdg_wm_base interface not found.\n");
		exit(EXIT_FAILURE);
	}

	if (ui_wl_data_device_manager && ui.seat) {
		ui_wl_data_device = wl_data_device_manager_get_data_device(ui_wl_data_device_manager, ui.seat);
		wl_data_device_add_listener(ui_wl_data_device, &_uiWaylandDataDeviceListener, NULL);
	}

	if (!ui.shm_format) ui.shm_format = WL_SHM_FORMAT_XRGB8888;
	ui_wl_xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
}

/////////////////////////////////////////
// Windows
/////////////////////////////////////////

int _UIWindowMessage(UIElement *element, UIMessage message, int di, void *dp) {
	UIWindow *window = (UIWindow *) element;
	
	// Intercept painting to enable vsync/frame throttling
	if (message == UI_MSG_WINDOW_UPDATE_BEFORE_PAINT) {
		WaylandWindowData *wdata = _GetWindowData(window);
		if (wdata && wdata->frame_pending) {
		// Accumulate damage and skip the paint cycle until compositor is ready
		if (UI_RECT_VALID(wdata->pending_updateRegion)) {
			if (UI_RECT_VALID(window->updateRegion)) {
				wdata->pending_updateRegion = UIRectangleBounding(wdata->pending_updateRegion, window->updateRegion);
			}
		} else {
			wdata->pending_updateRegion = window->updateRegion;
		}
		window->updateRegion = UI_RECT_1(0);
		}
	} else if (message == UI_MSG_DEALLOCATE) {
		if (wl_pointer_window == window) wl_pointer_window = nullptr;
		if (wl_keyboard_window == window) wl_keyboard_window = nullptr;

		// prevent free() on mmap-allocated memory
		window->bits = nullptr;
		_UIWindowDestroyCommon(window);
		_FreeWindowData(window);
		
		if (window->wl_buffer) {
			wl_buffer_destroy(window->wl_buffer);
			window->wl_buffer = nullptr;
		}

		if (window->wl_shm_pool) {
			wl_shm_pool_destroy(window->wl_shm_pool);
			munmap(window->shm_data, window->shm_capacity);
			close(window->shm_fd);
			window->wl_shm_pool = nullptr;
		}

		if (window->xdg_toplevel) { xdg_toplevel_destroy(window->xdg_toplevel); window->xdg_toplevel = nullptr; }
		if (window->xdg_popup) { xdg_popup_destroy(window->xdg_popup); window->xdg_popup = nullptr; }
		if (window->xdg_surface) { xdg_surface_destroy(window->xdg_surface); window->xdg_surface = nullptr; }
		if (window->wl_surface) { wl_surface_destroy(window->wl_surface); window->wl_surface = nullptr; }
	}

	return _UIWindowMessageCommon(element, message, di, dp);
}

UIWindow *UIWindowCreate(UIWindow *owner, uint32_t flags, const char *cTitle, int _width, int _height) {
	_UIMenusClose();

	UIWindow *window = (UIWindow *) UIElementCreate(sizeof(UIWindow), NULL, flags | UI_ELEMENT_WINDOW, _UIWindowMessage, "Window");
	_UIWindowAdd(window);
	if (owner) window->scale = owner->scale;

	window->width = (flags & UI_WINDOW_MENU) ? 1 : _width ? _width : 800;
	window->height = (flags & UI_WINDOW_MENU) ? 1 : _height ? _height : 600;

	window->wl_surface = wl_compositor_create_surface(ui.compositor);
	wl_surface_set_user_data(window->wl_surface, window);

	if (flags & UI_WINDOW_MENU) {
		// Wayland requires size and position to create popup
		// We leave this blank and do them in UIMenuShow
		window->configured = false;
	} else {
		window->xdg_surface = xdg_wm_base_get_xdg_surface(ui.xdg_wm_base, window->wl_surface);
		xdg_surface_add_listener(window->xdg_surface, &_uiWaylandXdgSurfaceListener, window);
		
		window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
		xdg_toplevel_add_listener(window->xdg_toplevel, &_uiWaylandXdgToplevelListener, window);
		
		if (cTitle) xdg_toplevel_set_title(window->xdg_toplevel, cTitle);
		xdg_toplevel_set_app_id(window->xdg_toplevel, "gf2");
		if (flags & UI_WINDOW_MAXIMIZE) xdg_toplevel_set_maximized(window->xdg_toplevel);

		window->wait_for_configure = true;
		wl_surface_commit(window->wl_surface);
		
		while (window->wait_for_configure) {
			if (wl_display_dispatch(ui.display) == -1) break;
		}

		if (!window->wl_buffer) {
			_UIWaylandReallocateBuffer(window);
			window->e.bounds = UI_RECT_2S(window->width, window->height);
			window->e.clip = UI_RECT_2S(window->width, window->height);
		}
		window->configured = true;
	}

	return window;
}

void _UIWindowEndPaint(UIWindow *window, UIPainter *painter) {
	(void) painter;

	if (!window->wl_surface || !window->wl_buffer || !window->configured) return;

	int scale = (int)(window->scale + 0.5f);
	if (scale < 1) scale = 1;
	wl_surface_set_buffer_scale(window->wl_surface, scale);

	wl_surface_attach(window->wl_surface, window->wl_buffer, 0, 0);
	wl_surface_damage_buffer(window->wl_surface, 
		window->updateRegion.l, window->updateRegion.t, 
		UI_RECT_WIDTH(window->updateRegion), UI_RECT_HEIGHT(window->updateRegion));
	
	// Request vsync frame callback
	WaylandWindowData *wdata = _GetWindowData(window);
	if (wdata) {
		wdata->frame_callback = wl_surface_frame(window->wl_surface);
		wl_callback_add_listener(wdata->frame_callback, &_uiWaylandFrameListener, window);
		wdata->frame_pending = true;
	}

	wl_surface_commit(window->wl_surface);
	wl_display_flush(ui.display);
}

void UIWindowPack(UIWindow *window, int _width) {
	int width = _width ? _width : UIElementMessage(window->e.children[0], UI_MSG_GET_WIDTH, 0, 0);
	int height = UIElementMessage(window->e.children[0], UI_MSG_GET_HEIGHT, width, 0);
	
	if (window->width != width || window->height != height) {
		window->width = width;
		window->height = height;
		_UIWaylandReallocateBuffer(window);
		window->e.bounds = UI_RECT_2S(width, height);
		window->e.clip = UI_RECT_2S(width, height);
		UIElementRelayout(&window->e);
	}
}

bool _UIMessageLoopSingle(int *result) {
	while (wl_display_prepare_read(ui.display) != 0) {
		wl_display_dispatch_pending(ui.display);
	}
	wl_display_flush(ui.display);

	struct pollfd fds[2];
	fds[0].fd = wl_display_get_fd(ui.display);
	fds[0].events = POLLIN;
	fds[1].fd = ui_wl_wakeup_pipe[0];
	fds[1].events = POLLIN;

	int timeout = ui.animatingCount ? 10 : -1; 
	poll(fds, 2, timeout);
	
	if (fds[0].revents & POLLIN) {
		wl_display_read_events(ui.display);
		wl_display_dispatch_pending(ui.display);
	} else {
		wl_display_cancel_read(ui.display);
	}
	
	if (fds[1].revents & POLLIN) {
		UIPostedMessage msg;
		while (read(ui_wl_wakeup_pipe[0], &msg, sizeof(msg)) == sizeof(msg)) {
			UIElementMessage(&msg.window->e, msg.message, 0, msg.dp);
		}
	}

	if (ui.animatingCount) _UIProcessAnimations();
	else _UIUpdate();

	return true;
}

void _UIWindowSetCursor(UIWindow *window, int cursor) {
    if (!ui.pointer) return;

    int scale = (int)(window->scale + 0.5f);
    if (scale < 1) scale = 1;

    if (!ui_wl_cursor_theme || ui_wl_cursor_current_scale != scale) {
        if (ui_wl_cursor_theme) wl_cursor_theme_destroy(ui_wl_cursor_theme);
        ui_wl_cursor_theme = wl_cursor_theme_load(NULL, 24 * scale, ui.shm);
        ui_wl_cursor_current_scale = scale;
    }

    if (!ui_wl_cursor_theme) return;
    if (!ui_wl_cursor_surface) ui_wl_cursor_surface = wl_compositor_create_surface(ui.compositor);

    const char *cursor_name = "left_ptr";
    switch (cursor) {
        case UI_CURSOR_TEXT:            cursor_name = "xterm"; break;
        case UI_CURSOR_SPLIT_V:         cursor_name = "sb_v_double_arrow"; break;
        case UI_CURSOR_SPLIT_H:         cursor_name = "sb_h_double_arrow"; break;
        case UI_CURSOR_HAND:
        case UI_CURSOR_FLIPPED_ARROW:   cursor_name = "hand1"; break;
        case UI_CURSOR_CROSS_HAIR:      cursor_name = "crosshair"; break;
        case UI_CURSOR_RESIZE_UP:
        case UI_CURSOR_RESIZE_DOWN:     cursor_name = "ns-resize"; break;
        case UI_CURSOR_RESIZE_LEFT:
        case UI_CURSOR_RESIZE_RIGHT:    cursor_name = "ew-resize"; break;
        case UI_CURSOR_RESIZE_UP_LEFT:
        case UI_CURSOR_RESIZE_DOWN_RIGHT: cursor_name = "nwse-resize"; break;
        case UI_CURSOR_RESIZE_UP_RIGHT:
        case UI_CURSOR_RESIZE_DOWN_LEFT:  cursor_name = "nesw-resize"; break;
    }

    struct wl_cursor *wl_c = wl_cursor_theme_get_cursor(ui_wl_cursor_theme, cursor_name);
    if (!wl_c || wl_c->image_count == 0) {
        wl_c = wl_cursor_theme_get_cursor(ui_wl_cursor_theme, "left_ptr"); 
        if (!wl_c || wl_c->image_count == 0) return;
    }

    struct wl_cursor_image *image = wl_c->images[0];
    struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);

    wl_surface_set_buffer_scale(ui_wl_cursor_surface, scale);
    wl_pointer_set_cursor(ui.pointer, ui_wl_pointer_enter_serial, ui_wl_cursor_surface, 
                          image->hotspot_x / scale, image->hotspot_y / scale);
    wl_surface_attach(ui_wl_cursor_surface, buffer, 0, 0);
    wl_surface_damage_buffer(ui_wl_cursor_surface, 0, 0, image->width, image->height);
    wl_surface_commit(ui_wl_cursor_surface);
}

void _UIWindowGetScreenPosition(UIWindow *window, int *_x, int *_y) {
    // Global coordinates are disallowed in wayland. Popups must use relative positions.
    *_x = 0;
    *_y = 0;
}

void UIWindowPostMessage(UIWindow *window, UIMessage message, void *_dp) {
    UIPostedMessage msg = { window, message, _dp };
    write(ui_wl_wakeup_pipe[1], &msg, sizeof(msg));
}

void _UIClipboardWriteText(UIWindow *window, char *text) {
    if (!ui_wl_data_device_manager || !ui_wl_data_device) {
        UI_FREE(text);
        return;
    }
    
    if (ui_wl_clipboard_text) UI_FREE(ui_wl_clipboard_text);
    ui_wl_clipboard_text = text;

    struct wl_data_source *source = wl_data_device_manager_create_data_source(ui_wl_data_device_manager);
    wl_data_source_add_listener(source, &_uiWaylandDataSourceListener, NULL);
    
    wl_data_source_offer(source, "text/plain;charset=utf-8");
    wl_data_source_offer(source, "text/plain");
    wl_data_source_offer(source, "UTF8_STRING");

    wl_data_device_set_selection(ui_wl_data_device, source, ui_wl_last_serial);
}

char *_UIClipboardReadTextStart(UIWindow *window, size_t *bytes) {
    if (!ui_wl_selection_offer) return NULL;

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) return NULL;

    wl_data_offer_receive(ui_wl_selection_offer, "text/plain;charset=utf-8", pipe_fd[1]);
    close(pipe_fd[1]);
    
    wl_display_roundtrip(ui.display); 

    char *buffer = NULL;
    size_t size = 0;
    char chunk[1024];
    ssize_t n;
    
    while ((n = read(pipe_fd[0], chunk, sizeof(chunk))) > 0) {
        buffer = (char *) UI_REALLOC(buffer, size + n + 1);
        memcpy(buffer + size, chunk, n);
        size += n;
        buffer[size] = '\0';
    }
    
    close(pipe_fd[0]);

    if (bytes) *bytes = size;
    return buffer;
}

void _UIClipboardReadTextEnd(UIWindow *window, char *text) {
    if (text) UI_FREE(text);
}

void UIMenuShow(UIMenu *menu) {
    int width, height;
    _UIMenuPrepare(menu, &width, &height);

    UIWindow *window = (UIWindow *) menu->e.window;
    UIWindow *owner = menu->parentWindow;

    if (!owner || !owner->xdg_surface) {
        fprintf(stderr, "Error: Cannot create Wayland popup without an owner surface.\n");
        return;
    }

    int scale = (int)(window->scale + 0.5f);
    if (scale < 1) scale = 1;

    int logical_width = width / scale;
    int logical_height = height / scale;
    if (logical_width < 1) logical_width = 1;
    if (logical_height < 1) logical_height = 1;

    window->width = logical_width * scale;
    window->height = logical_height * scale;

    struct xdg_positioner *positioner = xdg_wm_base_create_positioner(ui.xdg_wm_base);
    
    xdg_positioner_set_anchor_rect(positioner, menu->pointX / scale, menu->pointY / scale, 1, 1);
    xdg_positioner_set_size(positioner, logical_width, logical_height);
    xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP_LEFT);
    xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    xdg_positioner_set_constraint_adjustment(positioner, 
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | 
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);

    window->xdg_surface = xdg_wm_base_get_xdg_surface(ui.xdg_wm_base, window->wl_surface);
    xdg_surface_add_listener(window->xdg_surface, &_uiWaylandXdgSurfaceListener, window);
    
    window->xdg_popup = xdg_surface_get_popup(window->xdg_surface, owner->xdg_surface, positioner);
    xdg_popup_add_listener(window->xdg_popup, &_uiWaylandXdgPopupListener, window);
    xdg_popup_grab(window->xdg_popup, ui.seat, ui_wl_last_serial);
    
    xdg_positioner_destroy(positioner);
    
    window->wait_for_configure = true;
    wl_surface_commit(window->wl_surface);
    
    while (window->wait_for_configure) {
        if (wl_display_dispatch(ui.display) == -1) break;
    }

    _UIWaylandReallocateBuffer(window);
    window->e.bounds = UI_RECT_2S(window->width, window->height);
    window->e.clip = UI_RECT_2S(window->width, window->height);
    window->configured = true;

    UIElementRelayout(&window->e);
    window->updateRegion = window->e.bounds;
    _UIUpdate();
}