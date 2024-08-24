#include "window.h"

#include <malloc.h>
#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <vulkan/vulkan_xlib.h>

struct window
{
	Display* display;
	Window window;
	int mouseX;
	int mouseY;
	uint2 size;
	
	int lockX;
	int lockY;
	bool mouseLock;

	Cursor invisibleCursor;
	Pixmap bitmapNoData;
};

const char* window_get_surface_extension_name()
{
	return VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
}

static int xlib_io_error_handler(Display*)
{
	//printf("X11 IO error lolol\n");
	return 0;
}

static void xlib_io_error_exit_handler(Display*, void*)
{
	//printf("X11 IO error exit\n");
}

window_t* window_create(uint2 size)
{
	window_t* window = calloc(1, sizeof(window_t));
	if (window == NULL) {
		return NULL;
	}

	XSetIOErrorHandler(xlib_io_error_handler);
	
	window->display = XOpenDisplay(NULL);
	if (window->display == NULL) {
		fprintf(stderr, "Failed to establish X11 connection\n");
		return NULL;
	}

	XSetIOErrorExitHandler(window->display, xlib_io_error_exit_handler, NULL);

	window->window = XCreateSimpleWindow(window->display, RootWindow(window->display, 0), 0, 0, size.x, size.y, 0, 0, WhitePixel(window->display, 0));

	XSelectInput(window->display, window->window, ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
	XMapWindow(window->display, window->window);

	window->size = size;

	XColor black;
	static char noData[] = { 0,0,0,0,0,0,0,0 };
	black.red = black.green = black.blue = 0;

	window->bitmapNoData = XCreateBitmapFromData(window->display, window->window, noData, 8, 8);
	window->invisibleCursor = XCreatePixmapCursor(window->display, window->bitmapNoData, window->bitmapNoData, &black, &black, 0, 0);
	
	return window;
}

void window_destroy(window_t* window)
{
	XSync(window->display, False);
	XFreeCursor(window->display, window->invisibleCursor);
	XFreePixmap(window->display, window->bitmapNoData);
	XDestroyWindow(window->display, window->window);
	XCloseDisplay(window->display);
}

VkSurfaceKHR window_create_surface(VkInstance instance, window_t* window)
{
	const VkXlibSurfaceCreateInfoKHR createInfo = {
		VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		.dpy = window->display,
		.window = window->window,
	};

	VkSurfaceKHR surface;
	if (vkCreateXlibSurfaceKHR(instance, &createInfo, NULL, &surface) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	return surface;
}

enum key_code MapKeyCode(XKeyEvent* event)
{
	const KeySym sym = XLookupKeysym(event, 0);
	//printf("sym: %x\n", sym);
	switch (sym) {
		case XK_w:			return KEY_W;
		case XK_a:			return KEY_A;
		case XK_s:			return KEY_S;
		case XK_d:			return KEY_D;
		case XK_f:			return KEY_F;
		case XK_Left:		return KEY_LEFT;
		case XK_Right:		return KEY_RIGHT;
		case XK_Up:			return KEY_UP;
		case XK_Down:		return KEY_DOWN;
		case XK_space:		return KEY_SPACE;
		case XK_Control_L:	return KEY_CONTROL;
		case XK_Shift_L:	return KEY_SHIFT;
		case XK_Delete:		return KEY_DELETE;
		case XK_F1:			return KEY_F1;
		case XK_Pause:		return KEY_PAUSE;
	}

	return KEY_UNKNOWN;
}

bool window_poll(window_event_t* event, window_t* window)
{
	XEvent xevent;

	if (XPending(window->display) <= 0) {
		return false;
	}

	XNextEvent(window->display, &xevent);

	*event = (window_event_t){WINDOW_EVENT_NULL};

	switch (xevent.type) {
		case DestroyNotify:
			*event = (window_event_t){
				WINDOW_EVENT_DESTROY,
			};
			break;
		case KeyPress:
			*event = (window_event_t){
				WINDOW_EVENT_KEY_DOWN,
				.data.key.code = MapKeyCode(&xevent.xkey),
			};
			break;
		case KeyRelease:
			*event = (window_event_t){ 
				WINDOW_EVENT_KEY_UP, 
				.data.key.code = MapKeyCode(&xevent.xkey),
			};
			break;
		case ConfigureNotify:
			window->size.x = xevent.xconfigure.width;
			window->size.y = xevent.xconfigure.height;
			*event = (window_event_t){ 
				WINDOW_EVENT_SIZE, 
				.data.size.size = window->size,
			};
			break;
		case ButtonPress:
			if (xevent.xbutton.button == 1) {
				*event = (window_event_t){
					WINDOW_EVENT_BUTTON_DOWN,
					.data.button.button = BUTTON_LEFT,
					.data.button.pos.x = xevent.xbutton.x,
					.data.button.pos.y = xevent.xbutton.y,
				};
			}
			else if (xevent.xbutton.button == 3) {
				*event = (window_event_t){
					WINDOW_EVENT_BUTTON_DOWN,
					.data.button.button = BUTTON_RIGHT,
					.data.button.pos.x = xevent.xbutton.x,
					.data.button.pos.y = xevent.xbutton.y,
				};
			}
			else if (xevent.xbutton.button == 4) {
				*event = (window_event_t){
					WINDOW_EVENT_MOUSE_SCROLL,
					.data.scroll.delta = 1,
				};
			}
			else if (xevent.xbutton.button == 5) {
				*event = (window_event_t){
					WINDOW_EVENT_MOUSE_SCROLL,
					.data.scroll.delta = -1,
				};
			}
			break;
		case ButtonRelease:
			if (xevent.xbutton.button == 1) {
				*event = (window_event_t){
					WINDOW_EVENT_BUTTON_UP,
					.data.button.button = BUTTON_LEFT,
					.data.button.pos.x = xevent.xbutton.x,
					.data.button.pos.y = xevent.xbutton.y,
				};
			}
			else if (xevent.xbutton.button == 3) {
				*event = (window_event_t){
					WINDOW_EVENT_BUTTON_UP,
					.data.button.button = BUTTON_RIGHT,
					.data.button.pos.x = xevent.xbutton.x,
					.data.button.pos.y = xevent.xbutton.y,
				};
			}
			break;
		case MotionNotify:
			if (window->mouseX == 0 && window->mouseY == 0) {
				window->mouseX = xevent.xmotion.x;
				window->mouseX = xevent.xmotion.y;
			}
			const int x = xevent.xmotion.x;
			const int y = xevent.xmotion.y;
			const int dx = x - window->mouseX;
			const int dy = y - window->mouseY;
			if (dx == 0 && dy == 0) {
				break;
			}
			if (window->mouseLock && x == window->size.x / 2 && y == window->size.y / 2) {
				window->mouseX = x;
				window->mouseY = y;
				break;
			}

			//printf("%d,%d\n", dx, dy);

			*event = (window_event_t){
				WINDOW_EVENT_MOUSE_MOVE,
				.data.mouse.pos.x = x,
				.data.mouse.pos.y = y,
				.data.mouse.delta.x = dx,
				.data.mouse.delta.y = dy,
			};

			window->mouseX = x;
			window->mouseY = y;
			if (window->mouseLock) {
				XWarpPointer(window->display, None, window->window, 0, 0, 0, 0, window->size.x / 2, window->size.y / 2);
			}
			break;
	}
	
	return true;
}

void window_lock_mouse(window_t* window, bool lock)
{
	window->mouseLock = lock;

	if (lock) {
		window->lockX = window->mouseX;
		window->lockY = window->mouseY;
		XDefineCursor(window->display, window->window, window->invisibleCursor);
	}
	else {
		XWarpPointer(window->display, None, window->window, 0, 0, 0, 0, window->lockX, window->lockY);
		XDefineCursor(window->display, window->window, None);
	}
}