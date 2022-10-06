#include <Windows.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

void* Heap;
#define MK_ALLOC(size) HeapAlloc(Heap, 0u, size);
#define MK_REALLOC(pointer, size) HeapReAlloc(Heap, 0u, pointer, size);
#define MK_FREE(pointer) HeapFree(Heap, 0u, pointer)
#define MK_MEMCOPY(destination, source, size) CopyMemory(destination, source, size)

#define MK_LIST_IMPLEMENTATION
#include "MkList.h"
#undef MK_LIST_IMPLEMENTATION

#define MK_MATH_IMPLEMENTATION
#include "MkMath.h"
#undef MK_MATH_IMPLEMENTATION

#define MK_OPEN_TYPE_IMPLEMENTATION
#include "MkOpenType.h"
#undef MK_OPEN_TYPE_IMPLEMENTATION

typedef size_t uSize;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;

BITMAPINFO BitmapInfo;
u32* BitmapContent;
long BitmapWidth;
long BitmapHeight;
long BitmapHalfX;
long BitmapHalfY;

void PaintWindow(HDC deviceContext, RECT windowRect) {
    StretchDIBits(
            deviceContext,
            0,
            0,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            0,
            0,
            BitmapWidth,
            BitmapHeight,
            BitmapContent,
            &BitmapInfo,
            DIB_RGB_COLORS,
            SRCCOPY);
}

LRESULT WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paintStruct;
            HDC deviceContext = BeginPaint(window, &paintStruct);
            RECT rect;
            GetClientRect(window, &rect);
            PaintWindow(deviceContext, rect);
            EndPaint(window, &paintStruct);
            return 0;
        }

        default:
        {
            return DefWindowProcW(window, message, wParam, lParam);
        }
    }
}

void PaintFilledPoint(long x, long y) {
    long centerX = BitmapHalfX + x;
    long left = centerX - 2;
    long right = centerX + 2;
    long centerY = BitmapHalfY + y;
    long top = centerY - 2;
    long bottom = centerY + 2;
    for (long i = top; i <= bottom; i++) {
        for (long j = left; j <= right; j++) {
            BitmapContent[i * BitmapWidth + j] = 0x00000000u;
        }
    }
}

int APIENTRY WinMain(
        HINSTANCE instance,
        HINSTANCE prevInstance,
        PSTR commandLine,
        int showCommands) {
    // TODO: maybe switch to VirtualAlloc?
    Heap = GetProcessHeap();

    u8* fontData;
    {
        const wchar_t* fontFilePath = L"C:\\Users\\Marvin\\source\\repos\\MkEdit\\consola.ttf";
        void* fontFile = CreateFileW(
                fontFilePath,
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                0u,
                NULL);
        uSize fontFileSize = GetFileSize(fontFile, NULL);
        fontData = (u8*)HeapAlloc(Heap, 0, fontFileSize);
        u32 bytesRead;
        ReadFile(fontFile, fontData, fontFileSize, &bytesRead, NULL);
        CloseHandle(fontFile);
    }

    MkFontInfo fontInfo;
    MkFontInfo_Create(fontData, &fontInfo);

    // test with letter 'i'
    u16 contourCount;
    u16* glyphContourEnds;
    MkGlyphPoint* glyphPoints;
    MkGetUnicodeGlyphPoints(fontData, &fontInfo, 0x0069u, &contourCount, &glyphContourEnds, &glyphPoints);
    u16 glyphPointCount = glyphContourEnds[contourCount - 1] + 1u;

    MkVector2F* controlPoints = MkList_Create(sizeof(MkVector2F));
    uSize currentContour = 0u;
    uSize currentGlyphContourEnd;
    uSize currentVertexContourStart;
    MkVector2F* vertices = MkList_Create(sizeof(MkVector2F));
    MkVector2F* bezierVertices = (MkVector2F*)HeapAlloc(Heap, 0u, 8u * sizeof(MkVector2F));
    for (uSize i = 0u; i != glyphPointCount; i++) {
        MkVector2F vertex;
        vertex.X = glyphPoints[i].X;
        vertex.Y = glyphPoints[i].Y;
        MkList_Push(&controlPoints, vertex);
        
        if (MkList_Count(controlPoints) == 1u) {
            currentGlyphContourEnd = glyphContourEnds[currentContour];
            currentVertexContourStart = i;
        } else {
            int isContourEnd = i == currentGlyphContourEnd;
            if (glyphPoints[i].OnCurve) {
                uSize controlPointCount = MkList_Count(controlPoints);
                MkList_Push(&vertices, controlPoints[0u]);
                if (controlPointCount > 2u) {
                    MkGetBezierVertices(controlPoints, controlPointCount, bezierVertices, 8u);
                    MkList_PushArray(&vertices, bezierVertices, 8u);
                }
                if (isContourEnd) {
                    MkList_Push(&vertices, controlPoints[controlPointCount - 1u]);
                }

                MkList_Clear(controlPoints);
                if (!isContourEnd) {
                    MkList_Push(&controlPoints, vertex);
                }
            } else {
                if (isContourEnd) {
                    MkList_Push(&vertices, controlPoints[0u]);
                    MkList_Push(&controlPoints, vertices[currentVertexContourStart]);
                    MkGetBezierVertices(controlPoints, MkList_Count(controlPoints), bezierVertices, 8u);
                    MkList_PushArray(&vertices, bezierVertices, 8u);
                    MkList_Clear(controlPoints);
                }
            }

            if (isContourEnd) {
                currentContour++;
            }
        }
    }
    HeapFree(Heap, 0u, bezierVertices);
    MkList_Destroy(controlPoints);

    BitmapHalfX = MkMaxL(labs(fontInfo.MinX - 2), labs(fontInfo.MaxX + 2));
    BitmapHalfY = MkMaxL(labs(fontInfo.MinY - 2), labs(fontInfo.MaxY + 2));
    BitmapWidth = 2 * BitmapHalfX;
    BitmapHeight = 2 * BitmapHalfY;
    long bitmapLength = BitmapWidth * BitmapHeight;
    BitmapContent = (u32*)HeapAlloc(Heap, 0, bitmapLength * sizeof(u32));
    for (long i = 0; i != bitmapLength; i++) {
        BitmapContent[i] = 0x00FFFFFFu;
    }

    size_t vertexCount = MkList_Count(vertices);
    for (size_t i = 0; i != vertexCount; i++) {
        PaintFilledPoint(vertices[i].X, vertices[i].Y);
    }

    BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    BitmapInfo.bmiHeader.biWidth = BitmapWidth;
    BitmapInfo.bmiHeader.biHeight = BitmapHeight;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    WNDCLASSEXW windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"MKedit";
    RegisterClassExW(&windowClass);

    HWND window = CreateWindowExW(
            0,
            L"Mkedit",
            L"Mkedit",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            NULL,
            NULL,
            instance,
            NULL);
    ShowWindow(window, showCommands);

    MSG message;
    int messageRc;
    while ((messageRc = GetMessageW(&message, NULL, 0, 0)) != 0) {
        if (messageRc == -1) {
            return -1;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}