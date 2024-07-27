#include "game_resource.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

int game_resource_open(game_resource_t* gameResource)
{
	gameResource->fd = open("dat/gameresource.bin", O_RDONLY);
	if (gameResource->fd == -1)
	{
		fprintf(stderr, "Failed to open game resource file.\n");
		return 1;
	}

	gameResource->len = lseek(gameResource->fd, 0, SEEK_END);
	lseek(gameResource->fd, 0, SEEK_SET);

	gameResource->mem = mmap(NULL, gameResource->len, PROT_READ, MAP_PRIVATE, gameResource->fd, 0);
	if (gameResource->mem == NULL)
	{
		fprintf(stderr, "Failed to map game resource file.\n");
		return 1;
	}
	
	const FILEFORMAT_game_resource_header_t* header = (FILEFORMAT_game_resource_header_t*)gameResource->mem;

	gameResource->modelCount	= header->modelCount;
	gameResource->models		= (FILEFORMAT_game_resource_model_entry_t*)(gameResource->mem + sizeof(FILEFORMAT_game_resource_header_t));

	return 0;
}

void game_resource_close(game_resource_t* gameResource)
{
	munmap((void*)gameResource->mem, gameResource->len);
	close(gameResource->fd);
}