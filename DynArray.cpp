#include "DynArray.h"

#if !(defined(DYNARRAY_ALLOC) && defined(DYNARRAY_REALLOC) && defined(DYNARRAY_FREE))
#error "You need to define the memory macros!"
#endif

#ifndef DYNARRAY_GROW_COUNT
#define DYNARRAY_GROW_COUNT 8u
#endif

unsigned char * DynArray_CreateInternal(size_t elementSize) {
    DynArray_Header * header = (DynArray_Header *)DYNARRAY_ALLOC(sizeof(DynArray_Header) + DYNARRAY_GROW_COUNT * elementSize);
    if (!header) return 0u;

    header->Count = 0u;
    header->Capacity = DYNARRAY_GROW_COUNT;
    header->GrowCount = DYNARRAY_GROW_COUNT;
    header->ElementSize = elementSize;

    return DynArray_GetArray(header);
}

int DynArray_MaybeGrow(unsigned char ** array, size_t additionalCount) {
    DynArray_Header * header = DynArray_GetHeader(*array);
    size_t desiredCapacity = header->Count + additionalCount;
    if (desiredCapacity > header->Capacity) {
        size_t minGrowCount = desiredCapacity - header->Capacity;
        size_t newCapacity = minGrowCount > header->GrowCount ? desiredCapacity : header->Capacity + header->GrowCount;
        header = (DynArray_Header *)DYNARRAY_REALLOC(header, sizeof(DynArray_Header) + newCapacity * header->ElementSize);
        if (!header) return 0;
        header->Capacity = newCapacity;
        *array = DynArray_GetArray(header);
    }
    return 1;
}