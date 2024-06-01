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
	int width;
	int height;
	
	int lockX;
	int lockY;
	bool mouseLock;

	Cursor invisibleCursor;
	Pixmap bitmapNoData;
};

const char* getWindowSurfaceExtensionName()
{
	return VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
}

window_t* createWindow(uint32_t width, uint32_t height)
{
	window_t* window = calloc(1, sizeof(window_t));
	if (window == NULL) {
		return NULL;
	}
	
	window->display = XOpenDisplay(NULL);
	if (window->display == NULL) {
		fprintf(stderr, "Failed to establish X11 connection\n");
		return NULL;
	}

	window->window = XCreateSimpleWindow(window->display, RootWindow(window->display, 0), 0, 0, width, height, 0, 0, WhitePixel(window->display, 0));

	XSelectInput(window->display, window->window, ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
	XMapWindow(window->display, window->window);

	window->width = width;
	window->height = height;

	XColor black;
	static char noData[] = { 0,0,0,0,0,0,0,0 };
	black.red = black.green = black.blue = 0;

	window->bitmapNoData = XCreateBitmapFromData(window->display, window->window, noData, 8, 8);
	window->invisibleCursor = XCreatePixmapCursor(window->display, window->bitmapNoData, window->bitmapNoData, &black, &black, 0, 0);
	
	return window;
}

void destroyWindow(window_t* window)
{
	XFreeCursor(window->display, window->invisibleCursor);
	XFreePixmap(window->display, window->bitmapNoData);
	XDestroyWindow(window->display, window->window);
	XCloseDisplay(window->display);
}

VkSurfaceKHR createWindowSurface(VkInstance instance, window_t* window)
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
	}

	return KEY_UNKNOWN;
}

bool pollWindowEvent(window_event_t* event, window_t* window)
{
	XEvent xevent;

	if (XPending(window->display) <= 0) {
		return false;
	}

	XNextEvent(window->display, &xevent);

	*event = (window_event_t){WINDOW_EVENT_NULL};

	switch (xevent.type) {
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
			window->width = xevent.xconfigure.width;
			window->height = xevent.xconfigure.height;
			*event = (window_event_t){ 
				WINDOW_EVENT_SIZE, 
				.data.size.width = xevent.xconfigure.width,
				.data.size.height = xevent.xconfigure.height,
			};
			break;
		case ButtonPress:
			if (xevent.xbutton.button == 1) {
				*event = (window_event_t){
					WINDOW_EVENT_BUTTON_DOWN,
					.data.button.button = BUTTON_LEFT,
					.data.button.x = xevent.xbutton.x,
					.data.button.y = xevent.xbutton.y,
				};
			}
			else if (xevent.xbutton.button == 3) {
				*event = (window_event_t){
					WINDOW_EVENT_BUTTON_DOWN,
					.data.button.button = BUTTON_RIGHT,
					.data.button.x = xevent.xbutton.x,
					.data.button.y = xevent.xbutton.y,
				};
			}
			break;
		case ButtonRelease:
			if (xevent.xbutton.button == 1) {
				*event = (window_event_t){
					WINDOW_EVENT_BUTTON_UP,
					.data.button.button = BUTTON_LEFT,
					.data.button.x = xevent.xbutton.x,
					.data.button.y = xevent.xbutton.y,
				};
			}
			else if (xevent.xbutton.button == 3) {
				*event = (window_event_t){
					WINDOW_EVENT_BUTTON_UP,
					.data.button.button = BUTTON_RIGHT,
					.data.button.x = xevent.xbutton.x,
					.data.button.y = xevent.xbutton.y,
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
			if (window->mouseLock && x == window->width / 2 && y == window->height / 2) {
				window->mouseX = x;
				window->mouseY = y;
				break;
			}

			//printf("%d,%d\n", dx, dy);

			*event = (window_event_t){
				WINDOW_EVENT_MOUSE_MOVE,
				.data.mouse.x = x,
				.data.mouse.y = y,
				.data.mouse.dx = dx,
				.data.mouse.dy = dy,
			};

			window->mouseX = x;
			window->mouseY = y;
			if (window->mouseLock) {
				XWarpPointer(window->display, None, window->window, 0, 0, 0, 0, window->width / 2, window->height / 2);
			}
			break;
	}
	
	return true;
}

void setMouseLock(window_t* window, bool lock)
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