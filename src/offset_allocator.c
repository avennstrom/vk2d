// (C) Sebastian Aaltonen 2023
// MIT License (see file: LICENSE)

#include "offset_allocator.h"

#include <assert.h>

#ifdef DEBUG_VERBOSE
#include <stdio.h>
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <string.h>
#include <stdlib.h>

static const uint32_t TOP_BINS_INDEX_SHIFT = 3;
static const uint32_t LEAF_BINS_INDEX_MASK = 0x7;

static const uint32_t NO_SPACE = 0xffffffff;
static const uint32_t NODE_UNUSED = 0xffffffff;

static uint32_t lzcnt_nonzero(uint32_t v)
{
#ifdef _MSC_VER
	unsigned long retVal;
	_BitScanReverse(&retVal, v);
	return 31 - retVal;
#else
	return __builtin_clz(v);
#endif
}

static uint32_t tzcnt_nonzero(uint32_t v)
{
#ifdef _MSC_VER
	unsigned long retVal;
	_BitScanForward(&retVal, v);
	return retVal;
#else
	return __builtin_ctz(v);
#endif
}


static const uint32_t MANTISSA_BITS = 3;
static const uint32_t MANTISSA_VALUE = 1 << MANTISSA_BITS;
static const uint32_t MANTISSA_MASK = MANTISSA_VALUE - 1;

// Bin sizes follow floating point (exponent + mantissa) distribution (piecewise linear log approx)
// This ensures that for each size class, the average overhead percentage stays the same
uint32_t SmallFloat_uintToFloatRoundUp(uint32_t size)
{
	uint32_t exp = 0;
	uint32_t mantissa = 0;
	
	if (size < MANTISSA_VALUE)
	{
		// Denorm: 0..(MANTISSA_VALUE-1)
		mantissa = size;
	}
	else
	{
		// Normalized: Hidden high bit always 1. Not stored. Just like float.
		uint32_t leadingZeros = lzcnt_nonzero(size);
		uint32_t highestSetBit = 31 - leadingZeros;
		
		uint32_t mantissaStartBit = highestSetBit - MANTISSA_BITS;
		exp = mantissaStartBit + 1;
		mantissa = (size >> mantissaStartBit) & MANTISSA_MASK;
		
		uint32_t lowBitsMask = (1 << mantissaStartBit) - 1;
		
		// Round up!
		if ((size & lowBitsMask) != 0)
			mantissa++;
	}
	
	return (exp << MANTISSA_BITS) + mantissa; // + allows mantissa->exp overflow for round up
}

uint32_t SmallFloat_uintToFloatRoundDown(uint32_t size)
{
	uint32_t exp = 0;
	uint32_t mantissa = 0;
	
	if (size < MANTISSA_VALUE)
	{
		// Denorm: 0..(MANTISSA_VALUE-1)
		mantissa = size;
	}
	else
	{
		// Normalized: Hidden high bit always 1. Not stored. Just like float.
		uint32_t leadingZeros = lzcnt_nonzero(size);
		uint32_t highestSetBit = 31 - leadingZeros;
		
		uint32_t mantissaStartBit = highestSetBit - MANTISSA_BITS;
		exp = mantissaStartBit + 1;
		mantissa = (size >> mantissaStartBit) & MANTISSA_MASK;
	}
	
	return (exp << MANTISSA_BITS) | mantissa;
}

uint32_t SmallFloat_floatToUint(uint32_t floatValue)
{
	uint32_t exponent = floatValue >> MANTISSA_BITS;
	uint32_t mantissa = floatValue & MANTISSA_MASK;
	if (exponent == 0)
	{
		// Denorms
		return mantissa;
	}
	else
	{
		return (mantissa | MANTISSA_VALUE) << (exponent - 1);
	}
}

// Utility functions
static uint32_t findLowestSetBitAfter(uint32_t bitMask, uint32_t startBitIndex)
{
	uint32_t maskBeforeStartIndex = (1 << startBitIndex) - 1;
	uint32_t maskAfterStartIndex = ~maskBeforeStartIndex;
	uint32_t bitsAfter = bitMask & maskAfterStartIndex;
	if (bitsAfter == 0) return NO_SPACE;
	return tzcnt_nonzero(bitsAfter);
}

static uint32_t insertNodeIntoBin(offset_allocator_t* allocator, uint32_t size, uint32_t dataOffset)
{
	// Round down to bin index to ensure that bin >= alloc
	uint32_t binIndex = SmallFloat_uintToFloatRoundDown(size);
	
	uint32_t topBinIndex = binIndex >> TOP_BINS_INDEX_SHIFT;
	uint32_t leafBinIndex = binIndex & LEAF_BINS_INDEX_MASK;
	
	// Bin was empty before?
	if (allocator->m_binIndices[binIndex] == NODE_UNUSED)
	{
		// Set bin mask bits
		allocator->m_usedBins[topBinIndex] |= 1 << leafBinIndex;
		allocator->m_usedBinsTop |= 1 << topBinIndex;
	}
	
	// Take a freelist node and insert on top of the bin linked list (next = old top)
	uint32_t topNodeIndex = allocator->m_binIndices[binIndex];
	uint32_t nodeIndex = allocator->m_freeNodes[allocator->m_freeOffset--];
#ifdef DEBUG_VERBOSE
	printf("Getting node %u from freelist[%u]\n", nodeIndex, m_freeOffset + 1);
#endif
	allocator->m_nodes[nodeIndex] = (offset_allocator_node_t){.dataOffset = dataOffset, .dataSize = size, .binListNext = topNodeIndex};
	if (topNodeIndex != NODE_UNUSED)
	{
		allocator->m_nodes[topNodeIndex].binListPrev = nodeIndex;
	}
	allocator->m_binIndices[binIndex] = nodeIndex;
	
	allocator->m_freeStorage += size;
#ifdef DEBUG_VERBOSE
	printf("Free storage: %u (+%u) (insertNodeIntoBin)\n", m_freeStorage, size);
#endif

	return nodeIndex;
}

static void removeNodeFromBin(offset_allocator_t* allocator, uint32_t nodeIndex)
{
	offset_allocator_node_t* node = &allocator->m_nodes[nodeIndex];
	
	if (node->binListPrev != NODE_UNUSED)
	{
		// Easy case: We have previous node. Just remove this node from the middle of the list.
		allocator->m_nodes[node->binListPrev].binListNext = node->binListNext;
		if (node->binListNext != NODE_UNUSED)
		{
			allocator->m_nodes[node->binListNext].binListPrev = node->binListPrev;
		}
	}
	else
	{
		// Hard case: We are the first node in a bin. Find the bin.
		
		// Round down to bin index to ensure that bin >= alloc
		uint32_t binIndex = SmallFloat_uintToFloatRoundDown(node->dataSize);
		
		uint32_t topBinIndex = binIndex >> TOP_BINS_INDEX_SHIFT;
		uint32_t leafBinIndex = binIndex & LEAF_BINS_INDEX_MASK;
		
		allocator->m_binIndices[binIndex] = node->binListNext;
		if (node->binListNext != NODE_UNUSED)
		{
			allocator->m_nodes[node->binListNext].binListPrev = NODE_UNUSED;
		}

		// Bin empty?
		if (allocator->m_binIndices[binIndex] == NODE_UNUSED)
		{
			// Remove a leaf bin mask bit
			allocator->m_usedBins[topBinIndex] &= ~(1 << leafBinIndex);
			
			// All leaf bins empty?
			if (allocator->m_usedBins[topBinIndex] == 0)
			{
				// Remove a top bin mask bit
				allocator->m_usedBinsTop &= ~(1 << topBinIndex);
			}
		}
	}
	
	// Insert the node to freelist
#ifdef DEBUG_VERBOSE
	printf("Putting node %u into freelist[%u] (removeNodeFromBin)\n", nodeIndex, m_freeOffset + 1);
#endif
	allocator->m_freeNodes[++allocator->m_freeOffset] = nodeIndex;

	allocator->m_freeStorage -= node->dataSize;
#ifdef DEBUG_VERBOSE
	printf("Free storage: %u (-%u) (removeNodeFromBin)\n", m_freeStorage, node.dataSize);
#endif
}

static int offset_allocator_reset(offset_allocator_t* allocator)
{
	allocator->m_freeStorage = 0;
	allocator->m_usedBinsTop = 0;
	allocator->m_freeOffset = allocator->m_maxAllocs - 1;

	for (uint32_t i = 0 ; i < NUM_TOP_BINS; i++)
		allocator->m_usedBins[i] = 0;
	
	for (uint32_t i = 0 ; i < NUM_LEAF_BINS; i++)
		allocator->m_binIndices[i] = NODE_UNUSED;
	
	if (allocator->m_nodes) free(allocator->m_nodes);
	if (allocator->m_freeNodes) free(allocator->m_freeNodes);

	allocator->m_nodes = calloc(allocator->m_maxAllocs, sizeof(offset_allocator_node_t));
	allocator->m_freeNodes = calloc(allocator->m_maxAllocs, sizeof(uint32_t));

	for (size_t i = 0; i < allocator->m_maxAllocs; ++i)
	{
		allocator->m_nodes[i].binListPrev	= NODE_UNUSED;
		allocator->m_nodes[i].binListNext	= NODE_UNUSED;
		allocator->m_nodes[i].neighborPrev	= NODE_UNUSED;
		allocator->m_nodes[i].neighborNext	= NODE_UNUSED;
	}
	
	// Freelist is a stack. Nodes in inverse order so that [0] pops first.
	for (uint32_t i = 0; i < allocator->m_maxAllocs; i++)
	{
		allocator->m_freeNodes[i] = allocator->m_maxAllocs - i - 1;
	}
	
	// Start state: Whole storage as one big node
	// Algorithm will split remainders and push them back as smaller nodes
	insertNodeIntoBin(allocator, allocator->m_size, 0);

	return 0;
}

int offset_allocator_create(offset_allocator_t* allocator, uint32_t size, uint32_t maxAllocs)
{
	allocator->m_size = size;
	allocator->m_maxAllocs = maxAllocs;
	allocator->m_nodes = NULL;
	allocator->m_freeNodes = NULL;

	return offset_allocator_reset(allocator);
}

void offset_allocator_destroy(offset_allocator_t* allocator)
{
	free(allocator->m_nodes);
	free(allocator->m_freeNodes);
}

bool offset_allocator_alloc(offset_allocation_t* allocation, offset_allocator_t* allocator, uint32_t size)
{
	// Out of allocations?
	if (allocator->m_freeOffset == 0)
	{
		//return {.offset = Allocation::NO_SPACE, .metadata = Allocation::NO_SPACE};
		return false;
	}
	
	// Round up to bin index to ensure that alloc >= bin
	// Gives us min bin index that fits the size
	uint32_t minBinIndex = SmallFloat_uintToFloatRoundUp(size);
	
	uint32_t minTopBinIndex = minBinIndex >> TOP_BINS_INDEX_SHIFT;
	uint32_t minLeafBinIndex = minBinIndex & LEAF_BINS_INDEX_MASK;
	
	uint32_t topBinIndex = minTopBinIndex;
	uint32_t leafBinIndex = NO_SPACE;

	// If top bin exists, scan its leaf bin. This can fail (NO_SPACE).
	if (allocator->m_usedBinsTop & (1 << topBinIndex))
	{
		leafBinIndex = findLowestSetBitAfter(allocator->m_usedBins[topBinIndex], minLeafBinIndex);
	}

	// If we didn't find space in top bin, we search top bin from +1
	if (leafBinIndex == NO_SPACE)
	{
		topBinIndex = findLowestSetBitAfter(allocator->m_usedBinsTop, minTopBinIndex + 1);
		
		// Out of space?
		if (topBinIndex == NO_SPACE)
		{
			//return {.offset = Allocation::NO_SPACE, .metadata = Allocation::NO_SPACE};
			return false;
		}

		// All leaf bins here fit the alloc, since the top bin was rounded up. Start leaf search from bit 0.
		// NOTE: This search can't fail since at least one leaf bit was set because the top bit was set.
		leafBinIndex = tzcnt_nonzero(allocator->m_usedBins[topBinIndex]);
	}
			
	uint32_t binIndex = (topBinIndex << TOP_BINS_INDEX_SHIFT) | leafBinIndex;
	
	// Pop the top node of the bin. Bin top = node.next.
	uint32_t nodeIndex = allocator->m_binIndices[binIndex];
	offset_allocator_node_t* node = &allocator->m_nodes[nodeIndex];
	uint32_t nodeTotalSize = node->dataSize;
	node->dataSize = size;
	node->used = true;
	allocator->m_binIndices[binIndex] = node->binListNext;
	if (node->binListNext != NODE_UNUSED)
	{
		allocator->m_nodes[node->binListNext].binListPrev = NODE_UNUSED;
	}
	allocator->m_freeStorage -= nodeTotalSize;
#ifdef DEBUG_VERBOSE
	printf("Free storage: %u (-%u) (allocate)\n", m_freeStorage, nodeTotalSize);
#endif

	// Bin empty?
	if (allocator->m_binIndices[binIndex] == NODE_UNUSED)
	{
		// Remove a leaf bin mask bit
		allocator->m_usedBins[topBinIndex] &= ~(1 << leafBinIndex);
		
		// All leaf bins empty?
		if (allocator->m_usedBins[topBinIndex] == 0)
		{
			// Remove a top bin mask bit
			allocator->m_usedBinsTop &= ~(1 << topBinIndex);
		}
	}
	
	// Push back reminder N elements to a lower bin
	uint32_t reminderSize = nodeTotalSize - size;
	if (reminderSize > 0)
	{
		uint32_t newNodeIndex = insertNodeIntoBin(allocator, reminderSize, node->dataOffset + size);
		
		// Link nodes next to each other so that we can merge them later if both are free
		// And update the old next neighbor to point to the new node (in middle)
		if (node->neighborNext != NODE_UNUSED)
		{
			allocator->m_nodes[node->neighborNext].neighborPrev = newNodeIndex;
		}
		allocator->m_nodes[newNodeIndex].neighborPrev = nodeIndex;
		allocator->m_nodes[newNodeIndex].neighborNext = node->neighborNext;
		node->neighborNext = newNodeIndex;
	}
	
	//return {.offset = node.dataOffset, .metadata = nodeIndex};
	allocation->offset		= node->dataOffset;
	allocation->metadata	= nodeIndex;
	return true;
}
    
void offset_allocator_free(offset_allocator_t* allocator, offset_allocation_t allocation)
{
	assert(allocation.metadata != NO_SPACE);
	if (!allocator->m_nodes)
	{
		return;
	}
	
	uint32_t nodeIndex = allocation.metadata;
	offset_allocator_node_t* node = &allocator->m_nodes[nodeIndex];
	
	// Double delete check
	assert(node->used == true);
	
	// Merge with neighbors...
	uint32_t offset = node->dataOffset;
	uint32_t size = node->dataSize;
	
	if ((node->neighborPrev != NODE_UNUSED) && (allocator->m_nodes[node->neighborPrev].used == false))
	{
		// Previous (contiguous) free node: Change offset to previous node offset. Sum sizes
		offset_allocator_node_t* prevNode = &allocator->m_nodes[node->neighborPrev];
		offset = prevNode->dataOffset;
		size += prevNode->dataSize;
		
		// Remove node from the bin linked list and put it in the freelist
		removeNodeFromBin(allocator, node->neighborPrev);
		
		assert(prevNode->neighborNext == nodeIndex);
		node->neighborPrev = prevNode->neighborPrev;
	}
	
	if ((node->neighborNext != NODE_UNUSED) && (allocator->m_nodes[node->neighborNext].used == false))
	{
		// Next (contiguous) free node: Offset remains the same. Sum sizes.
		offset_allocator_node_t* nextNode = &allocator->m_nodes[node->neighborNext];
		size += nextNode->dataSize;
		
		// Remove node from the bin linked list and put it in the freelist
		removeNodeFromBin(allocator, node->neighborNext);
		
		assert(nextNode->neighborPrev == nodeIndex);
		node->neighborNext = nextNode->neighborNext;
	}

	uint32_t neighborNext = node->neighborNext;
	uint32_t neighborPrev = node->neighborPrev;
	
	// Insert the removed node to freelist
#ifdef DEBUG_VERBOSE
	printf("Putting node %u into freelist[%u] (free)\n", nodeIndex, m_freeOffset + 1);
#endif
	allocator->m_freeNodes[++allocator->m_freeOffset] = nodeIndex;

	// Insert the (combined) free node to bin
	uint32_t combinedNodeIndex = insertNodeIntoBin(allocator, size, offset);

	// Connect neighbors with the new combined node
	if (neighborNext != NODE_UNUSED)
	{
		allocator->m_nodes[combinedNodeIndex].neighborNext = neighborNext;
		allocator->m_nodes[neighborNext].neighborPrev = combinedNodeIndex;
	}
	if (neighborPrev != NODE_UNUSED)
	{
		allocator->m_nodes[combinedNodeIndex].neighborPrev = neighborPrev;
		allocator->m_nodes[neighborPrev].neighborNext = combinedNodeIndex;
	}
}

void offset_allocator_storage_report(offset_allocator_storage_report_t* report, const offset_allocator_t* allocator)
{
	uint32_t largestFreeRegion = 0;
	uint32_t freeStorage = 0;
	
	// Out of allocations? -> Zero free space
	if (allocator->m_freeOffset > 0)
	{
		freeStorage = allocator->m_freeStorage;
		if (allocator->m_usedBinsTop)
		{
			uint32_t topBinIndex = 31 - lzcnt_nonzero(allocator->m_usedBinsTop);
			uint32_t leafBinIndex = 31 - lzcnt_nonzero(allocator->m_usedBins[topBinIndex]);
			largestFreeRegion = SmallFloat_floatToUint((topBinIndex << TOP_BINS_INDEX_SHIFT) | leafBinIndex);
			assert(freeStorage >= largestFreeRegion);
		}
	}

	//return {.totalFreeSpace = freeStorage, .largestFreeRegion = largestFreeRegion};
	report->totalFreeSpace		= freeStorage;
	report->largestFreeRegion	= largestFreeRegion;
}

// uint32_t Allocator::allocationSize(Allocation allocation) const
// {
//     if (allocation.metadata == Allocation::NO_SPACE) return 0;
//     if (!m_nodes) return 0;
//     return m_nodes[allocation.metadata].dataSize;
// }