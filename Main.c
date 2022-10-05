#include <Windows.h>
#include <math.h>
#include <stdlib.h>
#include <wchar.h>

void* Heap;
#define MK_ALLOC(size) HeapAlloc(Heap, 0, size);
#define MK_REALLOC(pointer, size) HeapReAlloc(Heap, 0, pointer, size);
#define MK_FREE(pointer) HeapFree(Heap, 0, pointer)
#define MK_MEMCOPY(destination, source, size) CopyMemory(destination, source, size)

#define MK_LIST_IMPLEMENTATION
#include "MK_List.h"
#undef MK_LIST_IMPLEMENTATION

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

typedef signed char i8;
typedef short i16;
typedef long i32;
typedef long long i64;

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
} GlyphCoord;

u8* Font;
u32 LocaTableLocation;
i16 LocationOffsetType;
u32 EncodingTableLocation;
u32 GlyfTableLocation;
GlyphCoord* GlyphCoords;
i16 GlyphMinX;
i16 GlyphMaxX;
i16 GlyphMinY;
i16 GlyphMaxY;

u32 BigBytesToU32(u8* bytes) {
    u32 result = bytes[0];
    result <<= 8;
    result += bytes[1];
    result <<= 8;
    result += bytes[2];
    result <<= 8;
    result += bytes[3];
    return result;
}

u16 BigBytesToU16(u8* bytes) {
    u16 result = bytes[0];
    result <<= 8;
    result += bytes[1];
    return result;
}

i16 BigBytesToI16(u8* bytes) {
    i16 result = bytes[0];
    result <<= 8;
    result += bytes[1];
    return result;
}

int tagsEqual(const char* tagA, const char* tagB) {
    return
        tagA[0] == tagB[0] &&
        tagA[1] == tagB[1] &&
        tagA[2] == tagB[2] &&
        tagA[3] == tagB[3];
}

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
                0,
                NULL);

        u32 fileSize = GetFileSize(file, NULL);
        Font = (u8*)HeapAlloc(Heap, 0, fileSize);
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

GlyphCoord* GetGlyphData(u32 location, u16* count) {
    u32 currentLocation = GlyfTableLocation + location;

    // 2 - numberOfContours
    // 2 - xMin
    // 2 - yMin
    // 2 - xMax
    // 2 - yMax
    // = 10

    i16 contourCount = BigBytesToI16(&Font[currentLocation]);

    GlyphMinX = BigBytesToI16(&Font[currentLocation + 2u]);
    GlyphMinY = BigBytesToI16(&Font[currentLocation + 4u]);
    GlyphMaxX = BigBytesToI16(&Font[currentLocation + 6u]);
    GlyphMaxY = BigBytesToI16(&Font[currentLocation + 8u]);

    currentLocation += 10u;

    // contourCount * 2 - contourEndPointIndices
    // 2 - instructionLength
    // instructionLength * 1 - instructions

    u16* contourEndPointIndices = (u16*)HeapAlloc(Heap, 0, contourCount * sizeof(u16));
    for (i16 i = 0u; i != contourCount; i++) {
        contourEndPointIndices[i] = BigBytesToU16(&Font[currentLocation]);
        currentLocation += 2u;
    }

    u16 instructionCount = BigBytesToU16(&Font[currentLocation]);
    currentLocation += 2u + instructionCount;

    *count = contourEndPointIndices[contourCount - 1] + 1u;
    u8* flags = (u8*)HeapAlloc(Heap, 0, *count);
    u16 index = 0u;
    while (index != *count) {
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

    i16* xCoords = GetGlyphCoords(flags, *count, &currentLocation, 0x02u, 0x10u);
    i16* yCoords = GetGlyphCoords(flags, *count, &currentLocation, 0x04u, 0x20u);

    GlyphCoord* coords = (GlyphCoord*)HeapAlloc(Heap, 0, *count * sizeof(GlyphCoord));
    for (u16 i = 0u; i != *count; i++) {
        GlyphCoord* coord = &coords[i];
        coord->Point.X = xCoords[i];
        coord->Point.Y = yCoords[i];
        coord->OnCurve = flags[i] & 0x01u;
    }

    HeapFree(Heap, 0, contourEndPointIndices);
    HeapFree(Heap, 0, flags);
    HeapFree(Heap, 0, xCoords);
    HeapFree(Heap, 0, yCoords);
    return coords;
}

Vector2F* GetBezierVertices(Vector2F* points, int pointCount, int vertexCount) {
    int n = pointCount - 1;
    float interval = 1.0f / (vertexCount + 1);

    float nFactorial = (float)FactorialI(n);
    float* combinations = (float*)HeapAlloc(Heap, 0, (n + 1) * sizeof(float));
    for (int i = 0; i <= n; i++) {
        combinations[i] = nFactorial / (float)(FactorialI(i) * FactorialI(n - i));
    }

    Vector2F* vertices = (Vector2F*)HeapAlloc(Heap, 0, vertexCount * sizeof(Vector2F));
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
    return vertices;
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

    // test with letter 'u'
    u32 location;
    FindGlyph(0x0075u, &location);
    u16 coordCount;
    GlyphCoords = GetGlyphData(location, &coordCount);

    Vector2F* vertices = (Vector2F*)Mk_List_Create(sizeof(Vector2F));
    Vector2F* controlPoints = (Vector2F*)Mk_List_Create(sizeof(Vector2F));
    int controlPointCount = 0;
    for (u16 i = 0u; i != coordCount; i++) {
        Vector2F coord;
        coord.X = GlyphCoords[i].Point.X;
        coord.Y = GlyphCoords[i].Point.Y;

        if (GlyphCoords[i].OnCurve) {
            if (controlPointCount != 0) {
                Mk_List_Push(&vertices, controlPoints[0]);

                if (controlPointCount > 1) {
                    Mk_List_Push(&controlPoints, coord);
                    controlPointCount++;

                    Vector2F* bezierVertices = GetBezierVertices(controlPoints, controlPointCount, 8);
                    Mk_List_PushArray(&vertices, bezierVertices, 8);
                    HeapFree(Heap, 0, bezierVertices);
                }

                Mk_List_Clear(controlPoints);
            }
            Mk_List_Push(&controlPoints, coord);
            controlPointCount = 1;
        } else {
            Mk_List_Push(&controlPoints, coord);
            controlPointCount++;
        }
    }
    Mk_List_Push(&vertices, controlPoints[0]);
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