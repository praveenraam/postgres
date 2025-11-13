#include "./headers/slabStorage.h"
#include <stdio.h>

SlabStorage* SlabStorageInit(size_t c_object_size ,size_t c_memoryArraySize) {
    SlabStorage* slab = (SlabStorage*)malloc(sizeof(SlabStorage));

    if(slab == NULL) {
        return NULL;
    }

    size_t slot_size = c_object_size + sizeof(MemoryChunk); // here added new

    slab->memoryArraySize = c_memoryArraySize;
    slab->objectSize = c_object_size;
    slab->totalMemorySizeOfArray = c_memoryArraySize*slot_size; // updated from object size to slot size
    slab->usedMemorySizeOfArray = 0;

    slab->MemoryArray = malloc(slab->totalMemorySizeOfArray);
    if(slab->MemoryArray == NULL){
        free(slab);
        return NULL;
    }

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

    if(slab->status != FULL){
        void* returnPtr;
        
        if(StackIsEmpty(slab->ptrStackInSlab)){
            if((char*)slab->FreeSlabIterPointer >= (char*)slab->MemoryArray + slab->totalMemorySizeOfArray){
                fprintf(stderr,"Storage is FULL : ");
                return NULL;
            }
            // printf("stack is empty\n");
            returnPtr = slab->FreeSlabIterPointer;
            slab->FreeSlabIterPointer = (char*)slab->FreeSlabIterPointer + slab->objectSize + sizeof(MemoryChunk);
        }
        else{
            // printf("Stack is Not empty\n");
            returnPtr = StackPop(slab->ptrStackInSlab);
        }

        slab->usedMemorySizeOfArray = slab->usedMemorySizeOfArray + slab->objectSize + sizeof(MemoryChunk);
        slab->status = slab->totalMemorySizeOfArray == slab->usedMemorySizeOfArray ? FULL : PARTIAL;

        // printf("Memory allocated\n");
        // call memory manager add

        return returnPtr;
    }
    return NULL;
}

void SlabStorageDeallocater(SlabStorage* slab, void* ptr){
    ptr = ptr - sizeof(MemoryChunk);
    int fromMemory = (char*)ptr - (char*)slab->MemoryArray;

    if(fromMemory >= 0 && fromMemory < slab->totalMemorySizeOfArray){

        // if(fromMemory % slab->objectSize != 0){
        //     return;
        // }

        StackPush(slab->ptrStackInSlab,ptr);

        slab->usedMemorySizeOfArray = slab->usedMemorySizeOfArray-slab->objectSize-sizeof(MemoryChunk);

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