#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct content
{
	int			fd;
	uint8_t*	mem;
	size_t		len;
} content_t;

int content_open(content_t* content);
void content_close(content_t* content);

void content_read(void* target, content_t* content, size_t offset, size_t size);
