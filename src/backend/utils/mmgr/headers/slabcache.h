#pragma once
#include "slabStorage.h"
struct DLL;

struct SlabBlock;     // forward declaration

typedef struct SlabBlock
{
    struct SlabAllocator *current_slab; /* back-pointer to owning allocator */
    struct SlabCache     *cache;        /* cache that owns this block */
} SlabBlock;

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct {

    struct DLL* headerForFull;
    struct DLL* tailForFull;
    struct DLL* headerForPartial;
    struct DLL* tailForPartial;
    size_t object_size;
    size_t unit_size;
    
    // Cache Manager 
} SlabCache;

SlabCache* SlabCacheInit(size_t c_object_size);                                                                                                             
void SlabCacheDestroy(SlabCache* cache);
void* SlabCacheAllocate(SlabCache* cache);
void SlabCacheDeallocator(SlabCache* cache,void* ptr);
bool isDDL_ForPartialEmpty(SlabCache* cache);
bool isPtrInSlabCache(SlabCache* cache, void* ptr);
size_t SlabCacheReturnFreeSpace(SlabCache* cache,void* ptr);

size_t nBlocksCountSlabCache(SlabCache* cache);
size_t freeChunksSlabCache(SlabCache* cache);
size_t totalSpaceSlabCache(SlabCache* cache);
size_t freeSpaceSlabCache(SlabCache* cache);