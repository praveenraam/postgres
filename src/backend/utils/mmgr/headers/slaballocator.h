#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "./slabcache.h"
#include "./dll.h"
#include "postgres.h"

#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "utils/memdebug.h"
#include "utils/memutils.h"
#include "utils/memutils_internal.h"
#include "utils/memutils_memorychunk.h"

struct DLL;

typedef struct SlabMemoryChunkData
{
    MemoryContext context;  // owning context (the SA_Allocator's MemoryContext)
    uint16 cache_id;        // optional - if you want to identify which SlabCache
    uint16 reserved;        // align to 8 bytes
} SlabMemoryChunkData;

typedef struct {
    MemoryContextData header;
    struct DLL* headerForCacheList;
    struct DLL* tailForCacheList;
    pthread_mutex_t allocator_mutex;;

} SlabAllocator;

SlabAllocator* getInstanceOfSA();
void* SA_Allocater(MemoryContext context, Size object_size, int flags);
void SA_Deallocater(void* ptr);
void* SA_Reallocater(void* from, Size new_required_size, int flags);
void SA_Reset(MemoryContext context);
void SA_DeleteContext(MemoryContext context);
bool SA_isEmpty(MemoryContext context);
size_t SA_GetSizeOfObject(void* ptr);
MemoryContext SA_ContextCreate(MemoryContext parent, const char *name);
MemoryContext SA_get_chunk_context(void* ptr);
Size SA_get_chunk_space(void* ptr);
void SA_stats(MemoryContext context, MemoryStatsPrintFunc printfunc, void *pasthru, MemoryContextCounters *totals, bool print_to_stderr);


#ifdef MEMORY_CONTEXT_CHECKING
    void SA_Check(MemoryContext context);
#endif							/* MEMORY_CONTEXT_CHECKING */

