#include "content.h"

#include <stdio.h>
#include <memory.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

int content_open(content_t* content)
{
	content->fd = open("dat/content.bin", O_RDONLY);
	if (content->fd == -1)
	{
		fprintf(stderr, "Failed to open game resource file.\n");
		return 1;
	}

	content->len = lseek(content->fd, 0, SEEK_END);
	lseek(content->fd, 0, SEEK_SET);

	content->mem = mmap(NULL, content->len, PROT_READ, MAP_PRIVATE, content->fd, 0);
	if (content->mem == NULL)
	{
		fprintf(stderr, "Failed to map game resource file.\n");
		return 1;
	}

	return 0;
}

void content_close(content_t* content)
{
	munmap(content->mem, content->len);
	close(content->fd);
}

void content_read(void* target, content_t* content, size_t offset, size_t size)
{
	memcpy(target, content->mem + offset, size);
}