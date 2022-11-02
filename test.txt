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
#define MK_MEMMOVE(destination, source, size) MoveMemory(destination, source, size)

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

typedef struct {
    long TopY;
    long BottomY;
    float XHit;
    float Delta;
} RasterEdge;

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
            BitmapContent[i * BitmapWidth + j] = 0x00DCDCDCu;
        }
    }
}

RasterEdge MakeRasterEdge(MkVector2F a, MkVector2F b) {
    long ax = a.X;
    long ay = a.Y;
    long bx = b.X;
    long by = b.Y;

    RasterEdge edge;
    long yDelta = by - ay;
    long xDelta;
    if (yDelta < 0) {
        edge.TopY = ay;
        edge.BottomY = by;
        edge.XHit = ax;
        xDelta = bx - ax;
    } else if (yDelta > 0) {
        edge.TopY = by;
        edge.BottomY = ay;
        edge.XHit = bx;
        xDelta = ax - bx;
    } else {
        edge.TopY = ay;
        edge.BottomY = by;
        xDelta = ax - bx;
        edge.XHit = xDelta <= 0 ? ax : bx;
        return edge;
    }
    edge.Delta = (float)xDelta / (float)(edge.BottomY - edge.TopY) * -1.0f;
    return edge;
}

int CompareRasterEdges(RasterEdge* a, RasterEdge* b) {
    if (a->TopY < b->TopY) {
        return 1;
    } else if (a->TopY > b->TopY) {
        return -1;
    } else {
        float xHitDelta = a->XHit - b->XHit;
        if (xHitDelta < -0.001f) {
            return -1;
        } else if (xHitDelta > 0.001f) {
            return 1;
        } else {
            return 0;
        }
    }
}

int CompareRasterEdgesByXHit(RasterEdge* a, RasterEdge* b) {
    float xHitDelta = a->XHit - b->XHit;
    if (xHitDelta < -0.001f) {
        return -1;
    } else if (xHitDelta > 0.001f) {
        return 1;
    } else {
        return 0;
    }
}

// test with letter 'R'
void RenderTest(u8* fontData, MkFontInfo* fontInfo) {
    u16 contourCount;
    u16* glyphContourEnds;
    MkGlyphPoint* glyphPoints;
    MkGetUnicodeGlyphPoints(fontData, fontInfo, 0x0052u, &contourCount, &glyphContourEnds, &glyphPoints);
    if (contourCount == 0u) return;
    u16 glyphPointCount = glyphContourEnds[contourCount - 1] + 1u;

    RasterEdge* edges = MkList_Create(sizeof(RasterEdge));
    MkVector2F* curve = MkList_Create(sizeof(MkVector2F));
    MkVector2F* bezierCurve = (MkVector2F*)HeapAlloc(Heap, 0u, 8u * sizeof(MkVector2F));
    uSize currentContour = 0u;
    int contourStart = 1;
    MkVector2F currentContourStartVertex;
    uSize currentContourEnd;
    for (uSize i = 0u; i != glyphPointCount; i++) {
        MkVector2F vertex;
        vertex.X = glyphPoints[i].X;
        vertex.Y = glyphPoints[i].Y;
        MkList_Push(&curve, vertex);

        if (contourStart) {
            currentContourStartVertex = vertex;
            currentContourEnd = glyphContourEnds[currentContour];
            contourStart = 0;
        } else if (MkList_Count(curve) > 1u) {
            int contourEnd = i == currentContourEnd;

            if (glyphPoints[i].OnCurve) {
                uSize vertexCount = MkList_Count(curve);
                if (vertexCount > 2u) {
                    MkGetBezierVertices(curve, vertexCount, bezierCurve, 8u);
                    MkList_Push(&edges, MakeRasterEdge(curve[0u], bezierCurve[0u]));
                    for (uSize j = 0u; j != 7u; j++) {
                        MkList_Push(&edges, MakeRasterEdge(bezierCurve[j], bezierCurve[j + 1u]));
                    }
                    MkList_Push(&edges, MakeRasterEdge(bezierCurve[7u], curve[vertexCount - 1u]));
                } else {
                    MkList_Push(&edges, MakeRasterEdge(curve[0u], curve[1u]));
                }

                if (contourEnd) {
                    MkList_Push(&edges, MakeRasterEdge(curve[vertexCount - 1u], currentContourStartVertex));
                    MkList_Clear(curve);
                } else {
                    MkList_Clear(curve);
                    MkList_Push(&curve, vertex);
                }
            } else if (contourEnd) {
                MkList_Push(&curve, currentContourStartVertex);
                MkGetBezierVertices(curve, MkList_Count(curve), bezierCurve, 8u);
                MkList_Push(&edges, MakeRasterEdge(curve[0u], bezierCurve[0u]));
                for (uSize j = 0u; j != 7u; j++) {
                    MkList_Push(&edges, MakeRasterEdge(bezierCurve[j], bezierCurve[j + 1u]));
                }
                MkList_Push(&edges, MakeRasterEdge(bezierCurve[7u], currentContourStartVertex));
                MkList_Clear(curve);
            }

            if (contourEnd) {
                currentContour++;
                contourStart = 1;
            }
        }
    }
    HeapFree(Heap, 0u, bezierCurve);
    MkList_Destroy(curve);
    HeapFree(Heap, 0u, glyphPoints);
    HeapFree(Heap, 0u, glyphContourEnds);

    uSize edgeCount = MkList_Count(edges);
    qsort(edges, edgeCount, sizeof(RasterEdge), CompareRasterEdges);

    uSize nextEdge = 0u;
    RasterEdge* activeEdges = MkList_Create(sizeof(RasterEdge));
    long y = edges[0u].TopY;
    while (nextEdge != edgeCount || MkList_Count(activeEdges) != 0u) {
        while (nextEdge != edgeCount && edges[nextEdge].TopY == y) {
            if (edges[nextEdge].TopY != edges[nextEdge].BottomY) {
                MkList_Push(&activeEdges, edges[nextEdge]);
            }
            nextEdge++;
        }

        uSize activeEdgeCount = MkList_Count(activeEdges);
        if (activeEdgeCount != 0u) {
            uSize i;

            qsort(activeEdges, activeEdgeCount, sizeof(RasterEdge), CompareRasterEdgesByXHit);

            long x = activeEdges[0u].XHit;
            long lastX = activeEdges[activeEdgeCount - 1u].XHit;
            int draw = 0;
            long yIndex = BitmapHalfY - y;

            i = 0u;
            while (x <= lastX) {
                if ((long)activeEdges[i].XHit == x) {
                    i++;
                    if (i != activeEdgeCount && (long)activeEdges[i].XHit == x) {
                        draw ^= activeEdges[i - 1u].BottomY == y;
                        draw ^= activeEdges[i].BottomY == y;
                        i++;
                    } else {
                        draw ^= -1;
                    }
                }

                if (draw) {
                    BitmapContent[yIndex * BitmapWidth + BitmapHalfX + x] = 0x00DCDCDCu;
                }

                x++;
            }

            i = 0u;
            while (i != MkList_Count(activeEdges)) {
                if (activeEdges[i].BottomY == y) {
                    MkList_Remove(activeEdges, i);
                } else {
                    activeEdges[i].XHit += activeEdges[i].Delta;
                    i++;
                }
            }
        }

        y--;
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

    BitmapHalfX = MkMaxL(labs(fontInfo.MinX - 2), labs(fontInfo.MaxX + 2));
    BitmapHalfY = MkMaxL(labs(fontInfo.MinY - 2), labs(fontInfo.MaxY + 2));
    BitmapWidth = 2 * BitmapHalfX;
    BitmapHeight = 2 * BitmapHalfY;
    long bitmapLength = BitmapWidth * BitmapHeight;
    BitmapContent = (u32*)HeapAlloc(Heap, 0, bitmapLength * sizeof(u32));
    for (long i = 0; i != bitmapLength; i++) {
        BitmapContent[i] = 0x001E1E1Eu;
    }

    RenderTest(fontData, &fontInfo);

    BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    BitmapInfo.bmiHeader.biWidth = BitmapWidth;
    BitmapInfo.bmiHeader.biHeight = -BitmapHeight;
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