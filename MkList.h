#ifndef MK_LIST_HEADER
#define MK_LIST_HEADER

#include <stdint.h>

/* This library provides a dynamic array.
 * 
 * This is a single-file library, so to use it, at one place in your codebase you need to include it with MK_LIST_IMPLEMENTATION defined.
 * 
 * If the list needs to grow, it does so by the amount defined by the MK_LIST_GROW_SIZE macro, which you can set yourself if you need to.
 * 
 * This library by default uses the C standard headers to manage memory. You can override this by settings ALL of the following macros:
 *      MK_ALLOC(size)
 *      MK_REALLOC(pointer, size)
 *      MK_FREE(pointer)
 *      MK_MEMCOPY(destination, source, size)
 */

typedef size_t MkSize;
typedef uint8_t MkU8;

// T* Mk_ListCreate(size_t elementSize)
MkU8* MkList_Create(MkSize elementSize);

// void MkList_Destroy(T* list)
void MkList_Destroy(MkU8* list);

// MkSize MkList_Count(T* list)
#define MkList_Count(list) GetHeader((MkU8*)(list))->Count

// int MkList_Push(T** list, T element)
#define MkList_Push(list, element) (MaybeGrow((MkU8**)(list), 1u) ? ((*(list))[GetHeader((MkU8*)(*(list)))->Count++] = element, 1) : 0)

// int MkList_PushArray(T** list, T* array, size_t count)
int MkList_PushArray(MkU8** list, MkU8* array, MkSize count);

// void MkList_Clear(T* list)
#define MkList_Clear(list) GetHeader((MkU8*)(list))->Count = 0u

#endif


#ifdef MK_LIST_IMPLEMENTATION

#if !(defined(MK_ALLOC) && defined(MK_REALLOC) && defined(MK_FREE) && defined(MK_MEMCOPY))
#if defined(MK_ALLOC) || defined(MK_REALLOC) || defined(MK_FREE) || defined(MK_MEMCOPY)
#error "You need to either define all or none of the MK header memory macros!"
#endif

#include <stdlib.h>
#include <string.h>

#define MK_ALLOC(size) malloc(size)
#define MK_REALLOC(pointer, size) realloc(pointer, size)
#define MK_FREE(pointer) free(pointer)
#define MK_MEMCOPY(destination, source, size) memcpy(destination, source, size)
#endif

#ifndef MK_LIST_GROW_SIZE
#define MK_LIST_GROW_SIZE 32u
#endif

typedef struct {
    MkSize Count;
    MkSize Capacity;
    MkSize ElementSize;
} MkList_Header;

static inline MkList_Header* GetHeader(MkU8* list) {
    return (MkList_Header*)(list - sizeof(MkList_Header));
}

static inline MkU8* GetList(MkList_Header* header) {
    return (MkU8*)header + sizeof(MkList_Header);
}

MkU8* MkList_Create(MkSize elementSize) {
    MkList_Header* header = (MkList_Header*)MK_ALLOC(sizeof(MkList_Header) + MK_LIST_GROW_SIZE * elementSize);
    if (!header) return NULL;
    header->Count = 0u;
    header->Capacity = MK_LIST_GROW_SIZE;
    header->ElementSize = elementSize;
    return GetList(header);
}

void MkList_Destroy(MkU8* list) {
    MK_FREE(GetHeader(list));
}

static int MaybeGrow(MkU8** list, MkSize additionalSpace) {
    MkList_Header* header = GetHeader(*list);
    if (header->Count + additionalSpace >= header->Capacity) {
        MkSize newCapacity = header->Capacity + (additionalSpace > MK_LIST_GROW_SIZE ? additionalSpace : MK_LIST_GROW_SIZE);
        header = (MkList_Header*)MK_REALLOC(header, sizeof(MkList_Header) + newCapacity * header->ElementSize);
        if (!header) return 0;
        header->Capacity = newCapacity;
        *list = GetList(header);
    }
    return 1;
}

int MkList_PushArray(MkU8** list, MkU8* array, MkSize count) {
    if (!MaybeGrow(list, count)) return 0;
    MkList_Header* header = GetHeader(*list);
    MK_MEMCOPY(&(*list)[header->Count * header->ElementSize], array, count * header->ElementSize);
    header->Count += count;
    return 1;
}

#endif