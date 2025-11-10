
#include "./headers/dll.h"

DLL* DLL_Init(SlabStorage* slab){
    DLL* returnPtr = (DLL*)malloc(sizeof(DLL));

    if(returnPtr == NULL) return NULL;
    
    returnPtr->slabInDLL = slab;
    returnPtr->slabCacheInDLL = NULL;
    returnPtr->next = NULL;
    returnPtr->prev = NULL; 

    return returnPtr;
}

DLL* DLL_Init_SA(SlabCache* slabCache){
    DLL* returnPtr = (DLL*)malloc(sizeof(DLL));

    if(returnPtr == NULL) return NULL;
    
    returnPtr->slabInDLL = NULL;
    returnPtr->slabCacheInDLL = slabCache;
    returnPtr->next = NULL;
    returnPtr->prev = NULL; 

    return returnPtr;
}


void DLL_Destroy(DLL* dll_to_destroy){
    if(dll_to_destroy == NULL) return;
    free(dll_to_destroy);
}

void DLL_DestroyAll(DLL* head_ptr){

    DLL* current = head_ptr;
    while(current != NULL){
        DLL* nextOne = current->next;

        DLL_Destroy(current);
        current = nextOne;
    }

}

DLL* DLL_InsertAtEnd(DLL* tail, SlabStorage* slabToAdd) {
    if (tail == NULL) return NULL;

    DLL* newOne = DLL_Init(slabToAdd);
    if (newOne == NULL) return tail;

    tail->next = newOne;
    newOne->prev = tail;
    return newOne; // Return new tail and update in call like -- tail = insertAtEnd(tail, slabToAdd);
}

DLL* DLL_InsertAtEnd_SA(DLL* tail,SlabCache* slabCacheToAdd){
    if (tail == NULL || slabCacheToAdd == NULL) return NULL;

    DLL* newOne = DLL_Init_SA(slabCacheToAdd);
    if (newOne == NULL) return tail;

    tail->next = newOne;
    newOne->prev = tail;
    return newOne; 
}

DLL* DLL_InsertAtEnd_asDLL(DLL* tail, DLL* DLL_toAdd) {
    if (DLL_toAdd == NULL) return tail;
    if (tail == NULL) {
        DLL_toAdd->prev = NULL;
        DLL_toAdd->next = NULL;
        return DLL_toAdd;
    }

    tail->next = DLL_toAdd;
    DLL_toAdd->prev = tail;
    DLL_toAdd->next = NULL;
    return DLL_toAdd;
}

void DLL_print(DLL* head_ptr) {
    DLL* current = head_ptr;
    while (current != NULL) {
        printf("%p \n", (void*)current); 
        current = current->next;
    }
}