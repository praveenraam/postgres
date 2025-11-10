#pragma once
#include <stddef.h>
#include <stdbool.h>
#include<stdlib.h>

#define STACK_CAPACITY 10

typedef struct {
    void* items[STACK_CAPACITY];
    int top;
} PointerStack;

PointerStack* StackInit();
void StackDestroy(PointerStack* stack);
bool StackPush(PointerStack* stack, void* item);
void* StackPop(PointerStack* stack);
bool StackIsEmpty(PointerStack* stack);
bool StackIsFull(PointerStack* stack);
size_t countInsideStack(PointerStack* stack);