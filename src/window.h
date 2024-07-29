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
	KEY_SHIFT,
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
	WINDOW_EVENT_DESTROY,
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

const char* window_get_surface_extension_name();
window_t* window_create(uint32_t width, uint32_t height);
void window_destroy(window_t* window);
VkSurfaceKHR window_create_surface(VkInstance instance, window_t* window);
bool window_poll(window_event_t* event, window_t* window);
void window_lock_mouse(window_t* window, bool lock);