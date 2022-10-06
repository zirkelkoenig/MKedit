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
#include "Mk_List.h"
#undef MK_LIST_IMPLEMENTATION

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t uMax;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef struct {
    float X;
    float Y;
} Vector2F;

Vector2F Vector2F_Add(Vector2F a, Vector2F b) {
    Vector2F result;
    result.X = a.X + b.X;
    result.Y = a.Y + b.Y;
    return result;
}

Vector2F Vector2F_Scale(Vector2F v, float alpha) {
    Vector2F result;
    result.X = v.X * alpha;
    result.Y = v.Y * alpha;
    return result;
}

int FactorialI(int n) {
    if (n == 0) {
        return 1;
    }

    int result = 1;
    for (int i = 1; i <= n; i++) {
        result *= i;
    }
    return result;
}

typedef struct {
    i16 X;
    i16 Y;
} Vector2I;

typedef struct {
    Vector2I Point;
    i8 OnCurve;
} GlyphPoint;

u8* Font;
u32 LocaTableLocation;
i16 LocationOffsetType;
u32 EncodingTableLocation;
u32 GlyfTableLocation;
i16 GlyphMinX;
i16 GlyphMaxX;
i16 GlyphMinY;
i16 GlyphMaxY;

u32 BigBytesToU32(u8* bytes) {
    u32 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    result <<= 8u;
    result += bytes[2u];
    result <<= 8u;
    result += bytes[3u];
    return result;
}

u16 BigBytesToU16(u8* bytes) {
    u16 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    return result;
}

i16 BigBytesToI16(u8* bytes) {
    i16 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    return result;
}

int tagsEqual(const char* tagA, const char* tagB) {
    return
            tagA[0u] == tagB[0u] &&
            tagA[1u] == tagB[1u] &&
            tagA[2u] == tagB[2u] &&
            tagA[3u] == tagB[3u];
}

// @TODO: reorganize the whole font file thing into its own file
void InitFont() {
    //-----------------------------
    // Load Font File into Memory
    {
        const wchar_t* filePath = L"C:\\Users\\Marvin\\source\\repos\\MkEdit\\consola.ttf";
        void* file = CreateFileW(
                filePath,
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                0u,
                NULL);

        u32 fileSize = GetFileSize(file, NULL);
        Font = (u8*)HeapAlloc(Heap, 0u, fileSize);
        u32 bytesRead;
        ReadFile(file, Font, fileSize, &bytesRead, NULL);
        CloseHandle(file);
    }

    //-------------------
    // Parse Table Directory
    u32 headTableLocation;
    u32 cmapTableLocation;
    {
        // 4 - sfntVersion
        // 2 - tableCount
        // 2 - searchRange
        // 2 - entrySelector
        // 2 - rangeShift
        // = 12

        u16 fontTableCount = BigBytesToU16(&Font[4u]);

        // 4 - tag
        // 4 - checksum
        // 4 - offset
        // 4 - length

        for (u32 i = 0u; i != fontTableCount; i++) {
            u32 offset = 12u + i * 16u;
            char* tag = (char*)(&Font[offset]);
            u32 location = BigBytesToU32(&Font[offset + 8u]);

            if (tagsEqual(tag, "head")) headTableLocation = location;
            else if (tagsEqual(tag, "loca")) LocaTableLocation = location;
            else if (tagsEqual(tag, "cmap")) cmapTableLocation = location;
            else if (tagsEqual(tag, "glyf")) GlyfTableLocation = location;
        }
    }

    //---------------------
    // Parse Head Table
    {
        // 2 - majorVersion
        // 2 - minorVersion
        // 4 - fontRevision
        // 4 - checksumAdjustment
        // 4 - magicNumber
        // 2 - flags
        // 2 - unitsPerEm
        // 8 - created
        // 8 - modified
        // 2 - xMin
        // 2 - yMin
        // 2 - xMax
        // 2 - yMax
        // 2 - macStyle
        // 2 - lowestRecPpem
        // 2 - fontDirectionHint
        // 2 - indexToLocFormat
        // 2 - glyphDataFormat
        // = 54

        LocationOffsetType = BigBytesToI16(&Font[headTableLocation + 50u]);
    }

    //--------------------------------
    // Find Encoding Table Location
    {
        // 2 - version
        // 2 - encodingRecordCount

        u16 encodingCount = BigBytesToU16(&Font[cmapTableLocation + 2u]);

        // 2 - platformId
        // 2 - encodingId
        // 4 - subtableOffset

        u32 baseOffset = cmapTableLocation + 4u;
        EncodingTableLocation = 0xFFFFFFFFu;
        for (u32 i = 0u; i != encodingCount; i++) {
            u32 offset = baseOffset + i * 8u;
            u16 platformId = BigBytesToU16(&Font[offset]);
            if (platformId == 0u) // Unicode
            {
                u16 encodingId = BigBytesToU16(&Font[offset + 2u]);
                if (encodingId == 3u) // BMP
                {
                    EncodingTableLocation = cmapTableLocation + BigBytesToU32(&Font[offset + 4u]);
                    break;
                }
            }
        }
    }
}

int FindGlyph(u16 codepoint, u32* location) {
    // 2 - format
    // 2 - length
    // 2 - language
    // 2 - segCountX2
    // 2 - searchRange
    // 2 - entrySelector
    // 2 - rangeShift
    // = 14

    u32 segmentCount = BigBytesToU16(&Font[EncodingTableLocation + 6u]) / 2u;

    // segmentCount * 2 - endCodes
    // 2 - reservedPad
    // segmentCount * 2 - startCodes
    // segmentCount * 2 - idDelta
    // segmentCount * 2 - idRangeOffsets

    u32 arrayLength = segmentCount * 2u;
    u32 endCodesOffset = EncodingTableLocation + 14u;
    u32 startCodesOffset = endCodesOffset + 2u + arrayLength;
    u32 idDeltasOffset = startCodesOffset + arrayLength;
    u32 idRangesOffset = idDeltasOffset + arrayLength;

    u32 glyphId = 0u;
    for (u32 i = 0u; i != segmentCount; i++) {
        u32 offset = i * 2u;
        u16 endCode = BigBytesToU16(&Font[endCodesOffset + offset]);
        if (endCode >= codepoint) {
            u16 startCode = BigBytesToU16(&Font[startCodesOffset + offset]);
            if (startCode <= codepoint) {
                u16 idDelta = BigBytesToU16(&Font[idDeltasOffset + offset]);
                u8* idRangeOffsetPointer = &Font[idRangesOffset + offset];
                u16 idRangeOffset = BigBytesToU16(idRangeOffsetPointer);
                if (idRangeOffset != 0u) {
                    u8* glyphIdPointer = idRangeOffset + 2u * (codepoint - startCode) + idRangeOffsetPointer;
                    u16 initGlyphId = BigBytesToU16(glyphIdPointer);
                    if (initGlyphId != 0u) {
                        glyphId = (initGlyphId + idDelta) % 65536u;
                    }
                } else {
                    glyphId = (codepoint + idDelta) & 65536u;
                }
            }
            break;
        }
    }
    if (glyphId == 0u) {
        return 0;
    }

    if (LocationOffsetType) {
        u32 index = LocaTableLocation + glyphId * 4u;
        *location = BigBytesToU32(&Font[index]);
    } else {
        u32 index = LocaTableLocation + glyphId * 2u;
        *location = BigBytesToU16(&Font[index]);
    }
    return 1;
}

i16* GetGlyphCoords(u8* flags, u16 count, u32* location, u8 shortFlag, u8 alternateFlag) {
    i16* coords = (i16*)HeapAlloc(Heap, 0, count * sizeof(i16));
    i16 previous = 0;
    for (u16 i = 0u; i != count; i++) {
        if (flags[i] & shortFlag) {
            u8 coord = Font[*location];
            *location += 1u;
            if (flags[i] & alternateFlag) {
                coords[i] = previous + coord;
            } else {
                coords[i] = previous + (i16)coord * -1;
            }
        } else if (flags[i] & alternateFlag) {
            coords[i] = previous;
        } else {
            coords[i] = previous + BigBytesToI16(&Font[*location]);
            *location += 2u;
        }
        previous = coords[i];
    }
    return coords;
}

void GetGlyphPoints(u32 location, i16* contourCount, u16** contourEnds, GlyphPoint** points) {
    u32 currentLocation = GlyfTableLocation + location;

    // 2 - numberOfContours
    // 2 - xMin
    // 2 - yMin
    // 2 - xMax
    // 2 - yMax
    // = 10

    *contourCount = BigBytesToI16(&Font[currentLocation]);

    GlyphMinX = BigBytesToI16(&Font[currentLocation + 2u]);
    GlyphMinY = BigBytesToI16(&Font[currentLocation + 4u]);
    GlyphMaxX = BigBytesToI16(&Font[currentLocation + 6u]);
    GlyphMaxY = BigBytesToI16(&Font[currentLocation + 8u]);

    currentLocation += 10u;

    // contourCount * 2 - contourEndPointIndices
    // 2 - instructionLength
    // instructionLength * 1 - instructions

    *contourEnds = (u16*)HeapAlloc(Heap, 0, *contourCount * sizeof(u16));
    for (i16 i = 0u; i != *contourCount; i++) {
        (*contourEnds)[i] = BigBytesToU16(&Font[currentLocation]);
        currentLocation += 2u;
    }

    u16 instructionCount = BigBytesToU16(&Font[currentLocation]);
    currentLocation += 2u + instructionCount;

    u16 pointCount = (*contourEnds)[*contourCount - 1] + 1u;
    u8* flags = (u8*)HeapAlloc(Heap, 0, pointCount);
    u16 index = 0u;
    while (index != pointCount) {
        u8 flag = Font[currentLocation++];
        if (flag & 0x08u) {
            u8 repeatFlag = flag - 0x08u;
            flags[index++] = repeatFlag;
            u8 repeatCount = Font[currentLocation++];
            for (u8 j = 0u; j != repeatCount; j++) {
                flags[index++] = repeatFlag;
            }
        } else {
            flags[index++] = flag;
        }
    }

    i16* xCoords = GetGlyphCoords(flags, pointCount, &currentLocation, 0x02u, 0x10u);
    i16* yCoords = GetGlyphCoords(flags, pointCount, &currentLocation, 0x04u, 0x20u);

    *points = (GlyphPoint*)HeapAlloc(Heap, 0, pointCount * sizeof(GlyphPoint));
    for (u16 i = 0u; i != pointCount; i++) {
        GlyphPoint point;
        point.Point.X = xCoords[i];
        point.Point.Y = yCoords[i];
        point.OnCurve = flags[i] & 0x01u;
        (*points)[i] = point;
    }

    HeapFree(Heap, 0, flags);
    HeapFree(Heap, 0, xCoords);
    HeapFree(Heap, 0, yCoords);
}

void GetBezierVertices(Vector2F* points, int pointCount, Vector2F* vertices, int vertexCount) {
    int n = pointCount - 1;
    float interval = 1.0f / (vertexCount + 1);

    float nFactorial = (float)FactorialI(n);
    float* combinations = (float*)HeapAlloc(Heap, 0, (n + 1) * sizeof(float));
    for (int i = 0; i <= n; i++) {
        combinations[i] = nFactorial / (float)(FactorialI(i) * FactorialI(n - i));
    }

    for (int j = 0; j != vertexCount; j++) {
        float t = (j + 1) * interval;
        float tComplement = 1.0f - t;

        Vector2F vertex = { 0 };
        for (int i = 0; i <= n; i++) {
            float factor = powf(t, i) * powf(tComplement, n - i);
            vertex = Vector2F_Add(vertex, Vector2F_Scale(points[i], combinations[i] * factor));
        }
        vertices[j] = vertex;
    }

    HeapFree(Heap, 0, combinations);
}

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

long Mk_Max(long a, long b) {
    return a > b ? a : b;
}

void PaintFilledPoint(long x, long y) {
    long centerX = BitmapHalfX + x;
    long left = centerX - 1;
    long right = centerX + 1;
    long centerY = BitmapHalfY + y;
    long top = centerY - 1;
    long bottom = centerY + 1;
    for (long i = top; i <= bottom; i++) {
        for (long j = left; j <= right; j++) {
            BitmapContent[i * BitmapWidth + j] = 0x00000000u;
        }
    }
}

void PaintHollowPoint(long x, long y) {
    long centerX = BitmapHalfX + x;
    long left = centerX - 2;
    long right = centerX + 2;
    long centerY = BitmapHalfY + y;
    long top = centerY - 2;
    long bottom = centerY + 2;
    for (long i = top; i <= bottom; i++) {
        for (long j = left; j <= right; j++) {
            if (i == top || i == bottom || j == left || j == right) {
                BitmapContent[i * BitmapWidth + j] = 0x00000000u;
            }
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
    InitFont();

    // test with letter 'A'
    u32 location;
    FindGlyph(0x0041u, &location);

    i16 contourCount;
    u16* glyphContourEnds;
    GlyphPoint* glyphPoints;
    GetGlyphPoints(location, &contourCount, &glyphContourEnds, &glyphPoints);
    u16 glyphPointCount = glyphContourEnds[contourCount - 1] + 1u;

    Vector2F* controlPoints = Mk_List_Create(sizeof(Vector2F));
    u16 currentContour = 0u;
    u16 currentGlyphContourEnd;
    uMax currentVertexContourStart;
    Vector2F* vertices = Mk_List_Create(sizeof(Vector2F));
    Vector2F* bezierVertices = (Vector2F*)HeapAlloc(Heap, 0u, 8u * sizeof(Vector2F));
    for (u16 i = 0u; i != glyphPointCount; i++) {
        Vector2F vertex;
        vertex.X = glyphPoints[i].Point.X;
        vertex.Y = glyphPoints[i].Point.Y;
        Mk_List_Push(&controlPoints, vertex);
        
        if (Mk_List_Count(controlPoints) == 1u) {
            currentGlyphContourEnd = glyphContourEnds[currentContour];
            currentVertexContourStart = i;
        } else {
            int isContourEnd = i == currentGlyphContourEnd;
            if (glyphPoints[i].OnCurve) {
                uMax controlPointCount = Mk_List_Count(controlPoints);
                Mk_List_Push(&vertices, controlPoints[0u]);
                if (controlPointCount > 2u) {
                    GetBezierVertices(controlPoints, controlPointCount, bezierVertices, 8u);
                    Mk_List_PushArray(&vertices, bezierVertices, 8u);
                }
                if (isContourEnd) {
                    Mk_List_Push(&vertices, controlPoints[controlPointCount - 1u]);
                }

                Mk_List_Clear(controlPoints);
                if (!isContourEnd) {
                    Mk_List_Push(&controlPoints, vertex);
                }
            } else {
                if (isContourEnd) {
                    Mk_List_Push(&vertices, controlPoints[0u]);
                    Mk_List_Push(&controlPoints, vertices[currentVertexContourStart]);
                    GetBezierVertices(controlPoints, Mk_List_Count(controlPoints), bezierVertices, 8u);
                    Mk_List_PushArray(&vertices, bezierVertices, 8u);
                    Mk_List_Clear(controlPoints);
                }
            }

            if (isContourEnd) {
                currentContour++;
            }
        }
    }
    HeapFree(Heap, 0u, bezierVertices);
    Mk_List_Destroy(controlPoints);

    BitmapHalfX = Mk_Max(labs(GlyphMinX - 2), labs(GlyphMaxX + 2));
    BitmapHalfY = Mk_Max(labs(GlyphMinY - 2), labs(GlyphMaxY + 2));
    BitmapWidth = 2 * BitmapHalfX;
    BitmapHeight = 2 * BitmapHalfY;
    long bitmapLength = BitmapWidth * BitmapHeight;
    BitmapContent = (u32*)HeapAlloc(Heap, 0, bitmapLength * sizeof(u32));
    for (long i = 0; i != bitmapLength; i++) {
        BitmapContent[i] = 0x00FFFFFFu;
    }

    size_t vertexCount = Mk_List_Count(vertices);
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