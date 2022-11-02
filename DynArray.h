#ifndef DYNARRAY_H
#define DYNARRAY_H

//-------------------
// Public Interface

// T * DynArray_Create(T)
#define DynArray_Create(T) (T *)DynArray_CreateInternal(sizeof (T))

// int DynArray_Add(T ** array, T element)
#define DynArray_Add(array, element) (DynArray_MaybeGrow((unsigned char **)(array), 1u) ? ((*(array))[DynArray_GetHeader((unsigned char *)(*(array)))->Count++] = element, 1) : 0)

// size_t DynArray_Count(T * array)
#define DynArray_Count(array) DynArray_GetHeader((unsigned char *)array)->Count


//------------------------
// Implementation Details

struct DynArray_Header {
    size_t Count;
    size_t Capacity;
    size_t GrowCount;
    size_t ElementSize;
};

unsigned char * DynArray_CreateInternal(size_t elementSize);

static inline DynArray_Header * DynArray_GetHeader(unsigned char * array) {
    return (DynArray_Header *)(array - sizeof(DynArray_Header));
}

static inline unsigned char * DynArray_GetArray(DynArray_Header * header) {
    return (unsigned char *)header + sizeof(DynArray_Header);
}

int DynArray_MaybeGrow(unsigned char ** array, size_t additionalCount);

#endif