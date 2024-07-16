#include "offset_allocator.h"

#include <assert.h>
#include <stdio.h>

static int test_offset_allocator(void)
{
	printf("Testing offset allocator...\n");

	bool r;

	offset_allocator_t allocator = {0};
	offset_allocation_t allocs[4];
	
	offset_allocator_create(&allocator, 64, 4);
	
	r = offset_allocator_alloc(&allocs[0], &allocator, 32);
	assert(r == true);
	r = offset_allocator_alloc(&allocs[1], &allocator, 32);
	assert(r == true);
	r = offset_allocator_alloc(&allocs[2], &allocator, 32);
	assert(r == false);
	offset_allocator_free(&allocator, allocs[1]);
	r = offset_allocator_alloc(&allocs[1], &allocator, 32);
	assert(r == true);
	offset_allocator_free(&allocator, allocs[0]);
	r = offset_allocator_alloc(&allocs[0], &allocator, 32);
	assert(r == true);

	offset_allocator_free(&allocator, allocs[1]);

	r = offset_allocator_alloc(&allocs[0], &allocator, 34);
	assert(r == false);

	offset_allocator_destroy(&allocator);

	printf("Done\n");
	return 0;
}

int run_tests(void)
{
	if (test_offset_allocator()) return 1;

	printf("All tests passed!\n");
	return 0;
}