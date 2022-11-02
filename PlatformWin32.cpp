#include "Platform.h"

typedef void (*Main_Setup)(PlatformCallbacks platform);

typedef void (*Main_SetViewport)(unsigned long * bitmap, unsigned width, unsigned height);
static Main_SetViewport SetViewport;

#include <Windows.h>

void * Heap;

void * MemAlloc(size_t size) {
    return HeapAlloc(Heap, 0u, size);
}

void * MemReAlloc(void * pointer, size_t size) {
    return HeapReAlloc(Heap, 0u, pointer, size);
}

void MemFree(void * pointer) {
    HeapFree(Heap, 0u, pointer);
}

void MemCopy(void * destination, void * source, size_t size) {
    CopyMemory(destination, source, size);
}

void MemMove(void * destination, void * source, size_t size) {
    MoveMemory(destination, source, size);
}

unsigned char * GetFileContent(const wchar_t * path, size_t * size) {
    auto file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0u,
        NULL);
    if (!file) return NULL;
    
    *size = GetFileSize(file, NULL);
    auto content = (unsigned char *)MemAlloc(*size);
    if (content) {
        DWORD readCount;
        if (!ReadFile(file, content, *size, &readCount, NULL)) {
            MemFree(content);
            content = NULL;
            *size = 0u;
        }
    }
    
    CloseHandle(file);
    return content;
}

HWND Window;

void ViewportUpdated() {
    UpdateWindow(Window);
}

unsigned long * ViewportBitmap;
unsigned ViewportWidth;
unsigned ViewportHeight;
BITMAPINFO ViewportBitmapInfo;

LRESULT WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SIZE:
        {
            if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
                ViewportWidth = LOWORD(lParam);
                ViewportHeight = HIWORD(lParam);
                if (ViewportBitmap) {
                    ViewportBitmap = (unsigned long *)MemReAlloc(ViewportBitmap, ViewportWidth * ViewportHeight * sizeof(unsigned long));
                } else {
                    ViewportBitmap = (unsigned long *)MemAlloc(ViewportWidth * ViewportHeight * sizeof(unsigned long));
                }
                
                ViewportBitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                ViewportBitmapInfo.bmiHeader.biWidth = ViewportWidth;
                ViewportBitmapInfo.bmiHeader.biHeight = ViewportHeight;
                ViewportBitmapInfo.bmiHeader.biPlanes = 1;
                ViewportBitmapInfo.bmiHeader.biBitCount = 32;
                ViewportBitmapInfo.bmiHeader.biCompression = BI_RGB;
                
                SetViewport(ViewportBitmap, ViewportWidth, ViewportHeight);
            }
            return 0;
        }
        
        case WM_PAINT:
        {
            PAINTSTRUCT paintStruct;
            auto deviceContext = BeginPaint(window, &paintStruct);
            StretchDIBits(
                deviceContext,
                0, 0,
                ViewportWidth, ViewportHeight,
                0, 0,
                ViewportWidth, ViewportHeight,
                ViewportBitmap,
                &ViewportBitmapInfo,
                DIB_RGB_COLORS,
                SRCCOPY);
            EndPaint(window, &paintStruct);
            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
        
        default:
        {
            return DefWindowProc(window, message, wParam, lParam);
        }
    }
}

int APIENTRY WinMain(
    HINSTANCE instance,
    HINSTANCE prevInstance,
    PSTR commandLine,
    int showCommand)
{
    Heap = GetProcessHeap();
    
    auto main = LoadLibraryExW(L"Main.dll", NULL, 0u);
    SetViewport = (Main_SetViewport)GetProcAddress(main, "SetViewport");
    
    PlatformCallbacks platform;
    platform.MemAlloc = MemAlloc;
    platform.MemReAlloc = MemReAlloc;
    platform.MemFree = MemFree;
    platform.MemCopy = MemCopy;
    platform.MemMove = MemMove;
    platform.GetFileContent = GetFileContent;
    platform.ViewportUpdated = ViewportUpdated;
    
    auto setup = (Main_Setup)GetProcAddress(main, "Setup");
    setup(platform);
    
    WNDCLASSEXW windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"MKedit";
    RegisterClassExW(&windowClass);
    
    Window = CreateWindowExW(
        0u,
        L"MKedit",
        L"MKedit",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        instance,
        NULL);
    ShowWindow(Window, showCommand);
    
    MSG message;
    int rcMessage;
    while ((rcMessage = GetMessageW(&message, NULL, 0u, 0u)) != 0) {
        if (rcMessage == -1) return -1;
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return 0;
}