#include "./headers/slabStorage.h"
#include <stdio.h>

#define MAXALIGN 8

SlabStorage* SlabStorageInit(size_t c_object_size ,size_t c_memoryArraySize) {
    SlabStorage* slab = (SlabStorage*)malloc(sizeof(SlabStorage));

    if(slab == NULL) {
        return NULL;
    }

    size_t slot_size = c_object_size; // here added new

    slab->memoryArraySize = c_memoryArraySize;
    slab->objectSize = slot_size;
    slab->totalMemorySizeOfArray = c_memoryArraySize*slot_size; // updated from object size to slot size
    slab->usedMemorySizeOfArray = 0;

    void *mem = NULL;
    int rc = posix_memalign(&mem, MAXALIGN, slab->totalMemorySizeOfArray);
    if (rc != 0 || mem == NULL) {
        free(slab);
        return NULL;
    }

    slab->MemoryArray = mem;
    slab->FreeSlabIterPointer = slab->MemoryArray;
    slab->status = EMPTY;
    slab->ptrStackInSlab = StackInit();

    return slab;
    
}

void SlabStorageDestroy(SlabStorage* slab)
{
    if(slab != NULL){
        free(slab->MemoryArray);
        StackDestroy(slab->ptrStackInSlab);
        free(slab);
    }
}

void* SlabStorageAllocater(SlabStorage* slab){
    if (slab == NULL) return NULL;
    if (slab->status == FULL) return NULL;

    void* returnPtr;

    if (StackIsEmpty(slab->ptrStackInSlab)) {
        char *iter = (char*)slab->FreeSlabIterPointer;
        char *end = (char*)slab->MemoryArray + slab->totalMemorySizeOfArray;

        if (iter >= end) {
            return NULL;
        }

        returnPtr = (void*)iter;
        slab->FreeSlabIterPointer = (void*)(iter + slab->objectSize);
    } else {
        returnPtr = StackPop(slab->ptrStackInSlab);
    }

    slab->usedMemorySizeOfArray += slab->objectSize;
    slab->status = (slab->usedMemorySizeOfArray >= slab->totalMemorySizeOfArray) ? FULL : PARTIAL;
    return returnPtr;
}

void SlabStorageDeallocater(SlabStorage* slab, void* ptr){
    ptr = ptr;
    int fromMemory = (char*)ptr - (char*)slab->MemoryArray;

    if(fromMemory >= 0 && fromMemory < slab->totalMemorySizeOfArray){

        // if(fromMemory % slab->objectSize != 0){
        //     return;
        // }

        StackPush(slab->ptrStackInSlab,ptr);

        slab->usedMemorySizeOfArray = slab->usedMemorySizeOfArray-slab->objectSize;

        slab->status = slab->usedMemorySizeOfArray == 0 ? EMPTY : PARTIAL;

        // call memory manager remove
        // printf("Memory Freed\n");
        return;

    }
    // printf("Ptr is out of bound\n");
}

bool SlabStorageContains(SlabStorage *slab, void *ptr)
{
    int fromMemory = (char*)ptr - (char*)slab->MemoryArray;
    return fromMemory >= 0 && fromMemory < slab->totalMemorySizeOfArray;
}

enum StatusOfSlotsAvailable getStatus(SlabStorage* slab){
    return slab->status;
}