#ifndef MK_DYN_ARRAY_H
#define MK_DYN_ARRAY_H

#include "Platform.h"

// T * DynArrayCreate(T, size_t initCapacity)
#define DynArrayCreate(T, initCapacity) (T *)DynArrayCreateImpl(sizeof(T), initCapacity)

// void DynArrayDestroy(T * array)
#define DynArrayDestroy(array) MemFree(DynArrayGetHeader((unsigned char *)(array)))

// int DynArrayAppend(T ** array, T elem)
#define DynArrayAppend(array, elem) (DynArrayIncrement((unsigned char **)(array)) ? ((*(array))[DynArrayCount(*(array)) - 1] = (elem), 1) : 0)

// size_t DynArrayCount(T * array)
#define DynArrayCount(array) DynArrayGetHeader((unsigned char *)(array))->count

// int DynArraySetCount(T ** array, size_t count)
#define DynArraySetCount(array, count) DynArraySetCountImpl((unsigned char **)(array), count)

struct DynArrayHeader
{
    size_t count;
    size_t capacity;
    size_t elemSize;
};

static inline unsigned char * DynArrayGetArray(DynArrayHeader * header)
{
    return (unsigned char *)header + sizeof(DynArrayHeader);
}

static inline DynArrayHeader * DynArrayGetHeader(unsigned char * array)
{
    return (DynArrayHeader *)(array - sizeof(DynArrayHeader));
}

unsigned char * DynArrayCreateImpl(size_t elemSize, size_t initCapacity);

int DynArrayIncrement(unsigned char ** array);

int DynArraySetCountImpl(unsigned char ** array, size_t count);

#endif
