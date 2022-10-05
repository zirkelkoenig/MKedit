#ifndef MK_LIST_HEADER
#define MK_LIST_HEADER

#include <stdint.h>

// T* Mk_List_Create(sizeof(T))
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

#ifndef MK_ALLOC
#include <stdlib.h>
#include <string.h>

#define MK_ALLOC(size) malloc(size)
#define MK_REALLOC(pointer, size) realloc(pointer, size)
#define MK_FREE(pointer) free(pointer)
#define MK_MEMCOPY(destination, source, size) memcpy(destination, source, size)
#endif

#define MK_LIST_GROW_SIZE 32u

typedef struct {
    size_t Count;
    size_t Capacity;
    size_t ElementSize;
} Mk_List_Header;

static Mk_List_Header* GetHeader(uint8_t* list) {
    return (Mk_List_Header*)(list - sizeof(Mk_List_Header));
}

static uint8_t* GetList(Mk_List_Header* header) {
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

int Mk_List_MaybeGrow(uint8_t** list, size_t additionalSpace) {
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