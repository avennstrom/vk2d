#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <vulkan/vulkan_core.h>

typedef struct window window_t;
typedef struct window_event window_event_t;

enum key_code {
	KEY_UNKNOWN,
	KEY_W,
	KEY_A,
	KEY_S,
	KEY_D,
	KEY_F,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,
	KEY_SPACE,
	KEY_CONTROL,
};

enum button {
	BUTTON_LEFT,
	BUTTON_RIGHT,
};

enum window_event_type {
	WINDOW_EVENT_NULL = 0,
	WINDOW_EVENT_SIZE,
	WINDOW_EVENT_KEY_DOWN,
	WINDOW_EVENT_KEY_UP,
	WINDOW_EVENT_BUTTON_DOWN,
	WINDOW_EVENT_BUTTON_UP,
	WINDOW_EVENT_MOUSE_MOVE,
};

struct window_event {
	enum window_event_type type;
	union {
		struct {
			enum key_code code;
		} key;
		struct {
			uint32_t width;
			uint32_t height;
		} size;
		struct {
			enum button button;
			int x, y;
		} button;
		struct {
			int x, y;
			int dx, dy;
		} mouse;
	} data;
};

const char* getWindowSurfaceExtensionName();
window_t* createWindow(uint32_t width, uint32_t height);
void destroyWindow(window_t* window);
//void* getNativeWindowHandle(window_t* window);
VkSurfaceKHR createWindowSurface(VkInstance instance, window_t* window);
bool pollWindowEvent(window_event_t* event, window_t* window);
void setMouseLock(window_t* window, bool lock);