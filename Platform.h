#ifndef PLATFORM_H
#define PLATFORM_H

struct PlatformCallbacks {
    // Memory
    void * (*MemAlloc)(size_t size);
    void * (*MemReAlloc)(void * pointer, size_t size);
    void (*MemFree)(void * pointer);
    void (*MemCopy)(void * destination, void * source, size_t size);
    void (*MemMove)(void * destination, void * source, size_t size);
    
    // Files
    unsigned char * (*GetFileContent)(const wchar_t * path, size_t * size);
    
    // Rendering
    void (*ViewportUpdated)();
};

#endif