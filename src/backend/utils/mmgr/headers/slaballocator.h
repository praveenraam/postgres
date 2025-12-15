#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "./slabcache.h"
#include "./dll.h"

struct DLL;

typedef struct SlabAllocator {
    MemoryContextData header;
    struct DLL* headerForCacheList;
    struct DLL* tailForCacheList;
} SlabAllocator;

// ---- function prototypes ----
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
void SA_stats(MemoryContext context, MemoryStatsPrintFunc printfunc, void *pasthru, 
              MemoryContextCounters *totals, bool print_to_stderr);

#ifdef MEMORY_CONTEXT_CHECKING
void SA_Check(MemoryContext context);
#endif
