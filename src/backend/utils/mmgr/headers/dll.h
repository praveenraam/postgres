#pragma once
#include "./slabStorage.h"
#include "./slabcache.h"
#include <stdio.h>

typedef struct DLL{
    SlabStorage* slabInDLL;
    SlabCache* slabCacheInDLL;
    struct DLL* next;
    struct DLL* prev;
} DLL;

DLL* DLL_Init(SlabStorage* slab);
DLL* DLL_Init_SA(SlabCache* slabCache);
void DLL_Destroy(DLL* dll_to_destroy);
void DLL_DestroyAll(DLL* head_ptr);
DLL* DLL_InsertAtEnd(DLL* tail, SlabStorage* slabToAdd);
DLL* DLL_InsertAtEnd_asDLL(DLL* tail,DLL* DLL_toAdd);
DLL* DLL_InsertAtEnd_SA(DLL* tail,SlabCache* slabCacheToAdd);
void DLL_print(DLL* head_ptr);