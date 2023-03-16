#ifndef MKEDIT_PLATFORM_H
#define MKEDIT_PLATFORM_H

// T * MemAlloc(T, size_t count)
#define MemAlloc(T, count) (T *)MemAllocImpl((count) * sizeof(T))

void * MemAllocImpl(size_t size);

// T * MemRealloc(T * pointer, T, size_t count)
#define MemRealloc(pointer, T, count) (T *)MemReallocImpl(pointer, (count) * sizeof(T))

void * MemReallocImpl(void * pointer, size_t size);

// void MemFree(T * pointer)
void MemFree(void * pointer);

// void MemSet(T * pointer, unsigned char byte, T, size_t count)
#define MemSet(pointer, byte, T, count) MemSetImpl(pointer, byte, (count) * sizeof(T))

void MemSetImpl(void * pointer, unsigned char byte, size_t count);

unsigned char * GetFileContent(const wchar_t * path, size_t * size);

#endif