#include "./headers/dll.h"
#include "./headers/slabcache.h"

#define MAXALIGN 8

SlabCache* SlabCacheInit(size_t c_object_size){

    SlabCache* cache = (SlabCache*)malloc(sizeof(SlabCache));

    if(cache == NULL){
        return NULL;
    }

    cache->object_size = c_object_size;
    size_t u = sizeof(SlabBlock) + sizeof(MemoryChunk) + c_object_size;
    cache->unit_size = (u + (MAXALIGN - 1)) & ~(MAXALIGN - 1);
    
    cache->headerForFull = NULL;
    cache->tailForFull = NULL;
    cache->headerForPartial = NULL;
    cache->tailForPartial = NULL;

    if (pthread_mutex_init(&cache->cache_mutex, NULL) != 0) {
        free(cache);
        return NULL;
    }

    return cache;
}

void SlabCacheDestroy(SlabCache* cache){
    if(cache != NULL){
        pthread_mutex_lock(&cache->cache_mutex);

        if(cache->headerForFull != NULL) DLL_DestroyAll(cache->headerForFull);
        if(cache->headerForPartial != NULL) DLL_DestroyAll(cache->headerForPartial);

        pthread_mutex_unlock(&cache->cache_mutex);

        pthread_mutex_destroy(&cache->cache_mutex);
        free(cache);
    }
}

void* SlabCacheAllocate(SlabCache* cache){
    
    if (cache == NULL) return NULL;
    pthread_mutex_lock(&cache->cache_mutex);

    if(isDDL_ForPartialEmpty(cache)){
        SlabStorage* newSlab = SlabStorageInit(cache->unit_size,10);
        if (newSlab == NULL) {
            pthread_mutex_unlock(&cache->cache_mutex);
            return NULL;
        }

        cache->headerForPartial = DLL_Init(newSlab);
        cache->tailForPartial = cache->headerForPartial;
    }

    void* allocatedPtr = SlabStorageAllocater(cache->headerForPartial->slabInDLL);

    if (allocatedPtr == NULL) {
        pthread_mutex_unlock(&cache->cache_mutex);
        return NULL;
    }

    if(getStatus(cache->headerForPartial->slabInDLL) == FULL){
        
        DLL* oldHeader = cache->headerForPartial;
        cache->headerForPartial = cache->headerForPartial->next;

        if(cache->headerForPartial != NULL){
            cache->headerForPartial->prev = NULL;
        }
        else{
            cache->tailForPartial = NULL;
        }

        oldHeader->next = NULL;
        oldHeader->prev = NULL;

        if(cache->headerForFull == NULL){
            cache->headerForFull = oldHeader;
            cache->tailForFull = oldHeader;
            oldHeader->next = NULL;
            oldHeader->prev = NULL;
        }
        else{
            cache->tailForFull = DLL_InsertAtEnd_asDLL(cache->tailForFull,oldHeader);
            // DLL_Destroy(oldHeader);
        }
    }

    pthread_mutex_unlock(&cache->cache_mutex);
    return allocatedPtr;

}

void SlabCacheDeallocator(SlabCache* cache,void* ptr){

    if (cache == NULL || ptr == NULL) return;
    pthread_mutex_lock(&cache->cache_mutex);

    DLL* current = cache->headerForPartial;
    while(current != NULL){
        if(SlabStorageContains(current->slabInDLL,ptr)){
            SlabStorageDeallocater(current->slabInDLL,ptr);
            pthread_mutex_unlock(&cache->cache_mutex);
            return;
        }
        current = current->next;
    }

    current = cache->headerForFull;
    while(current != NULL){
        if (SlabStorageContains(current->slabInDLL, ptr)) {
            SlabStorageDeallocater(current->slabInDLL, ptr);

            if (getStatus(current->slabInDLL) != FULL) {

                if (current->prev != NULL) { // if not head
                    current->prev->next = current->next;
                } else { // if head
                    cache->headerForFull = current->next; 
                }

                if (current->next != NULL) { // if not tail
                    current->next->prev = current->prev;
                } else { // if tail
                    cache->tailForFull = current->prev;
                }

                current->prev = cache->tailForPartial;
                current->next = NULL;
                if (cache->tailForPartial != NULL) {
                    cache->tailForPartial->next = current;
                }
                cache->tailForPartial = current;
                if (cache->headerForPartial == NULL) {
                    cache->headerForPartial = current;
                }
            }
            pthread_mutex_unlock(&cache->cache_mutex);
            return;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
}

bool isDDL_ForPartialEmpty(SlabCache* cache){
    return cache == NULL || cache->headerForPartial == NULL;
}

bool isPtrInSlabCache(SlabCache* cache, void* ptr){

    if (cache == NULL || ptr == NULL) return false;
    pthread_mutex_lock(&cache->cache_mutex);

    DLL* current = cache->headerForPartial;
    while(current != NULL){
        if(SlabStorageContains(current->slabInDLL,ptr)){
            pthread_mutex_unlock(&cache->cache_mutex);
            return true;
        }
        current = current->next;
    }

    current = cache->headerForFull;
    while(current != NULL){
        if (SlabStorageContains(current->slabInDLL, ptr)) {
            pthread_mutex_unlock(&cache->cache_mutex);
            return true;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
    return false;
}


size_t SlabCacheReturnFreeSpace(SlabCache* cache,void* ptr){

    if (cache == NULL || ptr == NULL) return 0;
    pthread_mutex_lock(&cache->cache_mutex);

    DLL* current = cache->headerForPartial;
    while(current != NULL){
        if(SlabStorageContains(current->slabInDLL,ptr)){
            size_t returnValue = current->slabInDLL->totalMemorySizeOfArray - current->slabInDLL->usedMemorySizeOfArray;
            pthread_mutex_unlock(&cache->cache_mutex);
            return returnValue;
        }
        current = current->next;
    }

    current = cache->headerForFull;
    while(current != NULL){
        if (SlabStorageContains(current->slabInDLL, ptr)) {
            size_t returnValue = current->slabInDLL->totalMemorySizeOfArray - current->slabInDLL->usedMemorySizeOfArray;
            pthread_mutex_unlock(&cache->cache_mutex);
            return returnValue;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
    return 0;
}


size_t nBlocksCountSlabCache(SlabCache *cache)
{

    if (cache == NULL) return 0;
    pthread_mutex_lock(&cache->cache_mutex);
    
    size_t returnValue = 0;
    
    DLL* current = cache->headerForPartial;
    while(current != NULL){
        SlabStorage* slabDetail = current->slabInDLL;
        returnValue += slabDetail->memoryArraySize;
        current = current->next;
    }

    current = cache->headerForFull;
    while(current != NULL){
        SlabStorage* slabDetail = current->slabInDLL;
        returnValue += slabDetail->memoryArraySize;
        current = current->next;
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
    return returnValue;

}

size_t freeChunksSlabCache(SlabCache* cache){

    if (cache == NULL) return 0;
    pthread_mutex_lock(&cache->cache_mutex);

    size_t returnValue = 0;
    
    DLL* current = cache->headerForPartial;
    while(current != NULL){
        SlabStorage* slabDetail = current->slabInDLL;
        returnValue += countInsideStack(slabDetail);
        current = current->next;
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
    return returnValue;

}

size_t totalSpaceSlabCache(SlabCache* cache){
    if (cache == NULL) return 0;
    pthread_mutex_lock(&cache->cache_mutex);

    size_t returnValue = 0;
    size_t slabCount = 0;

    DLL* current = cache->headerForPartial;
    while (current != NULL) {
        slabCount++;
        current = current->next;
    }

    current = cache->headerForFull;
    while (current != NULL) {
        slabCount++;
        current = current->next;
    }

    if (slabCount > 0) {
        SlabStorage* firstSlab = cache->headerForPartial ? cache->headerForPartial->slabInDLL : cache->headerForFull->slabInDLL;
        
        if (firstSlab != NULL) {
            returnValue = slabCount * firstSlab->totalMemorySizeOfArray;
        }
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
    return returnValue;
}

size_t freeSpaceSlabCache(SlabCache* cache){

    if (cache == NULL) return 0;
    pthread_mutex_lock(&cache->cache_mutex);

    size_t returnValue = 0;
    
    DLL* current = cache->headerForPartial;
    while(current != NULL){
        SlabStorage* slabDetail = current->slabInDLL;
        returnValue += slabDetail->totalMemorySizeOfArray - slabDetail->usedMemorySizeOfArray;
        current = current->next;
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
    return returnValue;

}