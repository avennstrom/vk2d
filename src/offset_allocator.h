// (C) Sebastian Aaltonen 2023
// MIT License (see file: LICENSE)

#include <stdint.h>
#include <stdbool.h>

#define NUM_TOP_BINS	32
#define BINS_PER_LEAF	8
#define NUM_LEAF_BINS	(NUM_TOP_BINS * BINS_PER_LEAF)

typedef struct offset_allocation
{	
	uint32_t offset/* = NO_SPACE*/;
	uint32_t metadata/* = NO_SPACE*/; // internal: node index
} offset_allocation_t;

typedef struct offset_allocator_storage_report
{
	uint32_t totalFreeSpace;
	uint32_t largestFreeRegion;
} offset_allocator_storage_report_t;

typedef struct offset_allocator_node
{
	uint32_t dataOffset;
	uint32_t dataSize;
	uint32_t binListPrev   /*= unused*/;
	uint32_t binListNext   /*= unused*/;
	uint32_t neighborPrev  /*= unused*/;
	uint32_t neighborNext  /*= unused*/;
	bool used; // TODO: Merge as bit flag
} offset_allocator_node_t;

typedef struct offset_allocator
{
	uint32_t m_size;
	uint32_t m_maxAllocs;
	uint32_t m_freeStorage;

	uint32_t m_usedBinsTop;
	uint8_t m_usedBins[NUM_TOP_BINS];
	uint32_t m_binIndices[NUM_LEAF_BINS];
			
	offset_allocator_node_t* m_nodes;
	uint32_t* m_freeNodes;
	uint32_t m_freeOffset;
} offset_allocator_t;

int offset_allocator_create(offset_allocator_t* allocator, uint32_t size, uint32_t maxAllocs);
void offset_allocator_destroy(offset_allocator_t* allocator);
bool offset_allocator_alloc(offset_allocation_t* allocation, offset_allocator_t* allocator, uint32_t size);
void offset_allocator_free(offset_allocator_t* allocator, offset_allocation_t allocation);
void offset_allocator_storage_report(offset_allocator_storage_report_t* report, const offset_allocator_t* allocator);