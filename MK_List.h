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

// T* Mk_List_Create(size_t elementSize)
uint8_t* Mk_List_Create(size_t elementSize);

// void Mk_List_Destroy(T* list)
void Mk_List_Destroy(uint8_t* list);

// size_t Mk_List_Count(T* list)
#define Mk_List_Count(list) GetHeader((uint8_t*)(list))->Count

// int Mk_List_Push(T** list, T element)
#define Mk_List_Push(list, element) (Mk_List_MaybeGrow((uint8_t**)(list), 1u) ? ((*(list))[GetHeader((uint8_t*)(*(list)))->Count++] = element, 1) : 0)

// int Mk_List_PushArray(T** list, T* array, size_t count)
int Mk_List_PushArray(uint8_t** list, uint8_t* array, size_t count);

// void Mk_List_Clear(T* list)
#define Mk_List_Clear(list) GetHeader((uint8_t*)(list))->Count = 0u

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
    size_t Count;
    size_t Capacity;
    size_t ElementSize;
} Mk_List_Header;

static inline Mk_List_Header* GetHeader(uint8_t* list) {
    return (Mk_List_Header*)(list - sizeof(Mk_List_Header));
}

static inline uint8_t* GetList(Mk_List_Header* header) {
    return (uint8_t*)header + sizeof(Mk_List_Header);
}

uint8_t* Mk_List_Create(size_t elementSize) {
    Mk_List_Header* header = (Mk_List_Header*)MK_ALLOC(sizeof(Mk_List_Header) + MK_LIST_GROW_SIZE * elementSize);
    if (!header) return NULL;
    header->Count = 0;
    header->Capacity = MK_LIST_GROW_SIZE;
    header->ElementSize = elementSize;
    return GetList(header);
}

void Mk_List_Destroy(uint8_t* list) {
    MK_FREE(GetHeader(list));
}

static int Mk_List_MaybeGrow(uint8_t** list, size_t additionalSpace) {
    Mk_List_Header* header = GetHeader(*list);
    if (header->Count + additionalSpace >= header->Capacity) {
        size_t newCapacity = header->Capacity + (additionalSpace > MK_LIST_GROW_SIZE ? additionalSpace : MK_LIST_GROW_SIZE);
        header = (Mk_List_Header*)MK_REALLOC(header, sizeof(Mk_List_Header) + newCapacity * header->ElementSize);
        if (!header) return 0;
        header->Capacity = newCapacity;
        *list = GetList(header);
    }
    return 1;
}

int Mk_List_PushArray(uint8_t** list, uint8_t* array, size_t count) {
    if (!Mk_List_MaybeGrow(list, count)) return 0;
    Mk_List_Header* header = GetHeader(*list);
    MK_MEMCOPY(&(*list)[header->Count * header->ElementSize], array, count * header->ElementSize);
    header->Count += count;
    return 1;
}

#endif