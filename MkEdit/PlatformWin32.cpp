#include <windows.h>

#include "Platform.h"
#include "Main.h"

HANDLE heap;

void * MemAllocImpl(size_t size)
{
    return HeapAlloc(heap, 0, size);
}

void * MemReallocImpl(void * pointer, size_t size)
{
    return HeapReAlloc(heap, 0, pointer, size);
}

void MemFree(void * pointer)
{
    HeapFree(heap, 0, pointer);
}

void MemSetImpl(void * pointer, unsigned char byte, size_t count)
{
    FillMemory(pointer, count, byte);
}

unsigned char * GetFileContent(const wchar_t * path, size_t * size)
{
    HANDLE file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    *size = GetFileSize(file, NULL);

    unsigned char * content = MemAlloc(unsigned char, *size);
    DWORD discard;
    ReadFile(file, content, *size, &discard, NULL);

    CloseHandle(file);
    return content;
}

int WinMain(HINSTANCE instance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
    heap = GetProcessHeap();
    Run();
    return 0;
}
