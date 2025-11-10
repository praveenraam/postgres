#include "./headers/pointerstack.h"

PointerStack* StackInit()
{
    PointerStack* stack = (PointerStack*)malloc(sizeof(PointerStack));

    if(stack == NULL){
        return NULL;
    }

    stack->top = -1;

    return stack;
}

void StackDestroy(PointerStack* stack){
    if(stack != NULL){
        free(stack);
    }
}

bool StackPush(PointerStack* stack, void* item){
    if(stack->top >= STACK_CAPACITY - 1){
        return false;
    }
    stack->top++;
    stack->items[stack->top] = item;
    return true;
}

void* StackPop(PointerStack* stack){
    if(stack->top < 0){
        return NULL;
    }
    return stack->items[stack->top--];
}

bool StackIsEmpty(PointerStack* stack){
    return stack->top < 0;
}

bool StackIsFull(PointerStack* stack){
    return stack->top >= STACK_CAPACITY-1;
}

size_t countInsideStack(PointerStack* stack){
    return stack->top;
}