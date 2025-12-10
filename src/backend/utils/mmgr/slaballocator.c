#include "./headers/slaballocator.h"
#include <string.h>

static SlabAllocator* gInstance = NULL;

static inline uint64
SA_EncodeHdrMask(MemoryContext context)
{
    // In PostgreSQL, this encodes context pointer + alignment info.
    // For now, just cast the context pointer to an integer.
    return (uint64)(uintptr_t)context;
}

#define SA_SetChunkHdrMask(chunk, context) \
    ((chunk)->hdrmask = SA_EncodeHdrMask(context))


SlabAllocator* getInstanceOfSA() {
    if (gInstance == NULL) {
        gInstance = (SlabAllocator*)malloc(sizeof(SlabAllocator));
        if (gInstance != NULL) {

            memset(gInstance, 0, sizeof(SlabAllocator));            

            gInstance->headerForCacheList = NULL;
            gInstance->tailForCacheList = NULL;
            
            pthread_mutex_init(&gInstance->allocator_mutex, NULL);
        }
    }
    return gInstance;
}

void* SA_Allocater(MemoryContext context, Size object_size, int flags){
//    fprintf(stderr, "Entered SA_Allocater (size=%zu)\n", (size_t) object_size);

    SlabAllocator *instance = (SlabAllocator *) context;

    if (instance == NULL)
        return NULL;

    if (instance == NULL)
        return NULL;

    
    pthread_mutex_lock(&instance->allocator_mutex);

    if(instance->headerForCacheList == NULL){
        SlabCache* cache = SlabCacheInit(object_size);
        instance->headerForCacheList = DLL_Init_SA(cache);
        instance->tailForCacheList = instance->headerForCacheList;
    }

    SlabCache* cache = NULL;
    DLL* current = instance->headerForCacheList;
    while(current != NULL){

        if(current->slabCacheInDLL->object_size == object_size){
            cache = current->slabCacheInDLL;
            break;
        }
        current = current->next;
    }

    if(cache == NULL){
        cache = SlabCacheInit(object_size);
        instance->tailForCacheList = DLL_InsertAtEnd_SA(instance->tailForCacheList,cache);
    }

    pthread_mutex_unlock(&instance->allocator_mutex);

    if (cache == NULL) {
        return NULL;
    }

    void *raw_unit = SlabCacheAllocate(cache);
    if (raw_unit == NULL){
        return NULL;
    }

    SlabBlock *block = (SlabBlock *) raw_unit;
    block->current_slab = instance;
    block->cache        = cache;

    MemoryChunk *chunk = (MemoryChunk *)
        ((char *) raw_unit + sizeof(SlabBlock));


    #ifdef MEMORY_CONTEXT_CHECKING
        chunk->requested_size = object_size;
    #endif

    MemoryChunkSetHdrMask(chunk, (void*)block, object_size, MCTX_MY_SLAB_ALLOCATER_ID);
    void *user_ptr = (void *)((char *)chunk + sizeof(MemoryChunk));

    // fprintf(stderr,
    //         "SA_Allocater: unit=%p block=%p chunk=%p user=%p\n",
    //         raw_unit, (void*)block, (void*)chunk, user_ptr);

    return user_ptr;
}

void SA_Deallocater(void* ptr){
    // fprintf(stderr, "Entered SA_Deallocater ptr=%p\n", ptr);
    
    if (ptr == NULL)
        return;

    if(ptr == NULL) return;
    
    MemoryChunk *chunk = PointerGetMemoryChunk(ptr);
    // fprintf(stderr, "  chunk=%p\n", (void*)chunk);
    
    if (MemoryChunkIsExternal(chunk))
    {
        fprintf(stderr,
                "SA_Deallocater: external chunk passed, cannot free via SlabAllocator\n");
        return;
    }

    SlabBlock *block = (SlabBlock *) MemoryChunkGetBlock(chunk);
    // fprintf(stderr, "  block=%p\n", (void*)block);


    SlabAllocator *instance = block->current_slab;
    SlabCache     *cache    = block->cache;

    if (instance == NULL || cache == NULL)
    {
        fprintf(stderr,
                "SA_Deallocater: corrupted block header (instance/cache NULL)\n");
        return;
    }
    
    pthread_mutex_lock(&instance->allocator_mutex);

    if(instance->headerForCacheList == NULL){
        pthread_mutex_unlock(&instance->allocator_mutex);
        return;
    }   

    DLL* current = instance->headerForCacheList;
    while(current != NULL){
        SlabCache* cache = current->slabCacheInDLL;
        SlabCacheDeallocator(cache, (void*) block);

        current = current->next;
    }
    pthread_mutex_unlock(&instance->allocator_mutex);
}

void* SA_Reallocater(void* from_address, Size new_required_size, int flags)
{
    // fprintf(stderr,"Entered ReAllocatoer for %p\n",from_address);

    if (new_required_size == 0)
    {
        SA_Deallocater(from_address);
        return NULL;
    }

    if(from_address == NULL){
        return SA_Allocater((MemoryContext)NULL, new_required_size,0);
    }

    MemoryChunk *old_chunk = PointerGetMemoryChunk(from_address);

    if (MemoryChunkIsExternal(old_chunk))
    {
        void *new_address =
            SA_Allocater((MemoryContext) NULL, new_required_size, flags);

        if (new_address == NULL)
            return NULL;

        Size current_size = SA_GetSizeOfObject(from_address);
        Size copy_size = (new_required_size < current_size)
                         ? new_required_size
                         : current_size;

        if (copy_size > 0)
            memcpy(new_address, from_address, copy_size);

        SA_Deallocater(from_address);
        return new_address;
    }

    SlabBlock *block = (SlabBlock *) MemoryChunkGetBlock(old_chunk);
    SlabAllocator *instance = block->current_slab;

    MemoryContext ctx = (instance != NULL) ? (MemoryContext) instance : (MemoryContext) NULL;

    void *new_address = SA_Allocater(ctx, new_required_size, flags);
    if (new_address == NULL)
        return NULL;

    Size current_size = SA_GetSizeOfObject(from_address);
    if (current_size == 0){
        SA_Deallocater(from_address);
        return new_address;
    }

    size_t copy_size = (new_required_size < current_size) ? new_required_size : current_size;

    memcpy(new_address, from_address, copy_size);
    
    SA_Deallocater(from_address);

    return new_address;
}

size_t SA_GetSizeOfObject(void* ptr){

    if (ptr == NULL)
        return 0;

    MemoryChunk *chunk = PointerGetMemoryChunk(ptr);

    if(MemoryChunkIsExternal(chunk)){
        return 0;
    }

    return MemoryChunkGetValue(chunk);
}

void SA_Reset(MemoryContext context){
    SlabAllocator *instance = (SlabAllocator *) context;

    pthread_mutex_lock(&instance->allocator_mutex);

    if(instance->headerForCacheList == NULL){
        pthread_mutex_unlock(&instance->allocator_mutex);
        return;
    }

    SlabCache* cache = NULL;
    DLL* current = instance->headerForCacheList;
    while(current != NULL){
        SlabCacheDestroy(current->slabCacheInDLL);
        DLL* nextCache = current->next;
        current = NULL;
        current = nextCache;
    }
    pthread_mutex_unlock(&instance->allocator_mutex);
}

void SA_DeleteContext(MemoryContext context){
    if (gInstance == NULL) {
        return;
    }

    SA_Reset(context);

    pthread_mutex_destroy(&gInstance->allocator_mutex);
    free(gInstance);
    free(context);
    gInstance = NULL;
}

bool SA_isEmpty(MemoryContext context){
    SlabAllocator *instance = (SlabAllocator *) context;

    pthread_mutex_lock(&instance->allocator_mutex);

    bool is_empty = (instance->headerForCacheList == NULL);

    pthread_mutex_unlock(&instance->allocator_mutex);

    return is_empty;
}

MemoryContext SA_ContextCreate(MemoryContext parent, const char *name)
{
    // fprintf(stderr,"Calling context create updated\n");

    SlabAllocator *sb = (SlabAllocator *) malloc(sizeof(SlabAllocator));
    if (!sb) return NULL;

    memset(sb, 0, sizeof(SlabAllocator));

    sb->headerForCacheList = NULL;
    sb->tailForCacheList   = NULL;

    pthread_mutex_init(&sb->allocator_mutex, NULL);

    MemoryContextCreate(
        (MemoryContext) sb,
        T_MySlabContext,
        MCTX_MY_SLAB_ALLOCATER_ID,
        parent,
        name
    );

    return (MemoryContext) sb;
}

MemoryContext SA_get_chunk_context(void* ptr){
    if (ptr == NULL)
        return NULL;

    MemoryChunk *chunk = PointerGetMemoryChunk(ptr);

    if (MemoryChunkIsExternal(chunk))
        return NULL;

    SlabBlock *block = (SlabBlock *) MemoryChunkGetBlock(chunk);

    if (block == NULL || block->current_slab == NULL)
        return NULL;

    return (MemoryContext) block->current_slab;
}

Size SA_get_chunk_space(void* ptr){

    if (!ptr)
        return 0;

    MemoryChunk *chunk = PointerGetMemoryChunk(ptr);

    if (MemoryChunkIsExternal(chunk))
        return 0;

    return MemoryChunkGetValue(chunk);
}

void SA_stats(MemoryContext context, MemoryStatsPrintFunc printfunc, void *passthru, MemoryContextCounters *totals, bool print_to_stderr){

    SlabAllocator *instance = (SlabAllocator *) context;
    Size nBlocks = 0;
    Size freeChunks = 0;
    Size totalSpace = 0;
    Size freeSpace = 0;
    Size slabCacheCount = 0;  // Counter for the number of slab caches

    pthread_mutex_lock(&instance->allocator_mutex);

    if(instance->headerForCacheList == NULL){
        pthread_mutex_unlock(&instance->allocator_mutex);
        return;
    }

    DLL* current = instance->headerForCacheList;
    while(current != NULL){
        SlabCache* cache = current->slabCacheInDLL;

        // Accumulate total blocks, free chunks, and total space
        nBlocks += nBlocksCountSlabCache(cache);
        freeChunks += freeChunksSlabCache(cache);
        totalSpace += totalSpaceSlabCache(cache);
        freeSpace += freeSpaceSlabCache(cache);
        
        // Increment slab cache count
        slabCacheCount++; 

        current = current->next;
    }

    // Ensure that freeSpace doesn't go negative
    if (freeSpace > totalSpace) {
        freeSpace = totalSpace;  // Set free space to total space if there is a mismatch
    }

    // Calculate used space correctly
    Size usedSpace = totalSpace - freeSpace;

    // Calculate average blocks per slab cache
    Size averageBlocksPerCache = (slabCacheCount > 0) ? nBlocks / slabCacheCount : 0;

    if(printfunc){

        // Updated format string with additional fields
        char stats_string[300];

        // Adjust the format string for alignment and make sure all values are printed
        snprintf(stats_string, sizeof(stats_string),
                "%-20s| %-12zu | %-14zu | %-10zu | %-10zu | %-14zu | %-20zu",
                "TopMemoryContext", totalSpace, nBlocks, freeSpace, usedSpace,
                slabCacheCount, averageBlocksPerCache);

        // Print the stats
        printfunc(context, passthru, stats_string, print_to_stderr);
    }

    if(totals){
        totals->nblocks += nBlocks;
        totals->freechunks += freeChunks;
        totals->totalspace += totalSpace;
        totals->freespace += freeSpace;
    }

    pthread_mutex_unlock(&instance->allocator_mutex);

}

#ifdef MEMORY_CONTEXT_CHECKING

void SA_Check(MemoryContext context){
    
}

#endif	/* MEMORY_CONTEXT_CHECKING */
