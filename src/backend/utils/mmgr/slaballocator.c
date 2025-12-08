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
            gInstance->headerForCacheList = NULL;
            gInstance->tailForCacheList = NULL;

            pthread_mutex_init(&gInstance->allocator_mutex, NULL);
        }
    }
    return gInstance;
}

void* SA_Allocater(MemoryContext context, Size object_size, int flags){
    
    SlabAllocator *instance = (SlabAllocator *) context;

    if(instance == NULL){
        instance = getInstanceOfSA();
    }

    static int count = 0;
    count++;

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

    Size total_size = sizeof(MemoryChunk) + object_size;
    void *raw_ptr = SlabCacheAllocate(cache);
    // fprintf(stderr,"Allocating %i : ",count);
    if (raw_ptr == NULL){
        // fprintf(stderr,"raw_ptr is null : ");
        return NULL;
    }

    MemoryChunk *chunk = (MemoryChunk *) raw_ptr;

    #ifdef MEMORY_CONTEXT_CHECKING
        chunk->requested_size = object_size;
    #endif

    MemoryChunkSetHdrMask(chunk, (void*)chunk, object_size, MCTX_MY_SLAB_ALLOCATER_ID);

    void *user_ptr = (void *)((char *)chunk + sizeof(MemoryChunk));
    // fprintf(stderr,"Allocator %p %i\n",user_ptr,object_size);

    return user_ptr;
}

void SA_Deallocater(void* ptr){

    SlabAllocator* instance = getInstanceOfSA();

    pthread_mutex_lock(&instance->allocator_mutex);

    if(instance->headerForCacheList == NULL){
        pthread_mutex_unlock(&instance->allocator_mutex);
        return;
    }   

    DLL* current = instance->headerForCacheList;
    while(current != NULL){

        SlabCache* cache = current->slabCacheInDLL;
        SlabCacheDeallocator(cache, ptr);

        current = current->next;
    }
    pthread_mutex_unlock(&instance->allocator_mutex);
}

void* SA_Reallocater(void* from_address, Size new_required_size, int flags)
{
    SlabAllocator* instance = getInstanceOfSA();

    if(new_required_size == 0){
        SA_Deallocater(from_address);
        return NULL;
    }

    if(from_address == NULL){
        return SA_Allocater((MemoryContext)NULL, new_required_size,0);
    }

    void* new_address = SA_Allocater((MemoryContext)NULL, new_required_size,0);

    if(new_address == NULL) return NULL;

    Size current_size = SA_GetSizeOfObject(from_address);
    if(current_size == 0){
        return new_address;
    }

    size_t copy_size = new_required_size < current_size ? new_required_size : current_size;
    memcpy(new_address,from_address,copy_size);

    SA_Deallocater(from_address);
    return new_address;
}

size_t SA_GetSizeOfObject(void* ptr){

    SlabAllocator* instance = getInstanceOfSA();

    pthread_mutex_lock(&instance->allocator_mutex);

    if(instance->headerForCacheList == NULL){
        pthread_mutex_unlock(&instance->allocator_mutex);
        return 0;
    }

    DLL* current = instance->headerForCacheList;
    while(current != NULL){

        if(isPtrInSlabCache(current->slabCacheInDLL,ptr)){
            size_t object_size = current->slabCacheInDLL->object_size;
            pthread_mutex_unlock(&instance->allocator_mutex);
            return object_size;
        }

        current = current->next;
    }

    pthread_mutex_unlock(&instance->allocator_mutex);
    return 0;
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
    SlabAllocator *sb = getInstanceOfSA();
    MemSetAligned(sb, 0, sizeof(SlabAllocator));

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

    SlabAllocator* instance = getInstanceOfSA();

    pthread_mutex_lock(&instance->allocator_mutex);

    if(instance->headerForCacheList == NULL){
        pthread_mutex_unlock(&instance->allocator_mutex);
        return NULL;
    }

    DLL* current = instance->headerForCacheList;
    while(current != NULL){

        if(isPtrInSlabCache(current->slabCacheInDLL,ptr)){
            pthread_mutex_unlock(&instance->allocator_mutex);
            return &instance->header;
        }

        current = current->next;
    }

    pthread_mutex_unlock(&instance->allocator_mutex);
    return NULL;

}

Size SA_get_chunk_space(void* ptr){

    SlabAllocator* instance = getInstanceOfSA();

    pthread_mutex_lock(&instance->allocator_mutex);

    if(instance->headerForCacheList == NULL){
        pthread_mutex_unlock(&instance->allocator_mutex);
        return 0;
    }

    DLL* current = instance->headerForCacheList;
    while(current != NULL){

        if(isPtrInSlabCache(current->slabCacheInDLL,ptr)){
            pthread_mutex_unlock(&instance->allocator_mutex);
            return SlabCacheReturnFreeSpace(current->slabCacheInDLL,ptr);
        }

        current = current->next;
    }

    pthread_mutex_unlock(&instance->allocator_mutex);
    return 0;

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
