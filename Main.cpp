#include "Main.h"
#include "DynArray.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

typedef signed char i8;
typedef short i16;
typedef long i32;
typedef long long i64;

PlatformCallbacks platform;

unsigned long * viewportBitmap;
unsigned viewportWidth;
unsigned viewportHeight;

static void ClearViewportBitmap() {
    for (auto row = 0u; row != viewportHeight; row++) {
        for (auto col = 0u; col != viewportWidth; col++) {
            viewportBitmap[row * viewportWidth + col] = 0x00FFFFFFu;
        }
    }
}

struct FontInfo {
    u32 TableLoca;
    u32 TableGlyf;
    u32 TableHead;
    u32 TableCmap;

    u32 EncodingEndCodes;
    u32 EncodingStartCodes;
    u32 EncodingIdDeltas;
    u32 EncodingIdRangeOffsets;

    u16 UnitsPerEm;
    i16 LocationFormat;
    u16 EncodingSegmentCount;

    i16 MinX;
    i16 MaxX;
    i16 MinY;
    i16 MaxY;
};

struct GlyphPoint {
    i16 X;
    i16 Y;
    u8 OnCurve;
};

static u16 BigBytesToU16(u8 * bytes) {
    u16 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    return result;
}

static u32 BigBytesToU32(u8 * bytes) {
    u32 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    result <<= 8u;
    result += bytes[2u];
    result <<= 8u;
    result += bytes[3u];
    return result;
}

static i16 BigBytesToI16(u8 * bytes) {
    i16 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    return result;
}

static int TagsEqual(const char * tagA, const char * tagB) {
    return
        tagA[0u] == tagB[0u] &&
        tagA[1u] == tagB[1u] &&
        tagA[2u] == tagB[2u] &&
        tagA[3u] == tagB[3u];
}

int FontInit(unsigned char * data, FontInfo * info) {
    //-------------------
    // Table Directory
    {
        // 4 - sfntVersion
        // 2 - numTables
        // 2 - searchRange
        // 2 - entrySelector
        // 2 - rangeShift
        // = 12

        u32 sfntVersion = BigBytesToU32(data);
        if (sfntVersion != 0x00010000u) return 0;

        u16 numTables = BigBytesToU16(data + 4u);

        // Table Record:
        // 4 - tag
        // 4 - checksum
        // 4 - offset
        // 4 - length

        info->TableLoca = 0u;
        info->TableGlyf = 0u;
        info->TableHead = 0u;
        info->TableCmap = 0u;
        for (u16 i = 0u; i != numTables; i++) {
            u32 iOffset = 12u + i * 16u;
            char * tag = (char *)(data + iOffset);
            u32 tableOffset = BigBytesToU32(data + iOffset + 8u);

            if (TagsEqual(tag, "head")) info->TableHead = tableOffset;
            else if (TagsEqual(tag, "glyf")) info->TableGlyf = tableOffset;
            else if (TagsEqual(tag, "loca")) info->TableLoca = tableOffset;
            else if (TagsEqual(tag, "cmap")) info->TableCmap = tableOffset;
        }
        int tablesFound =
            info->TableLoca &&
            info->TableGlyf &&
            info->TableHead &&
            info->TableCmap;
        if (!tablesFound) return 0;
    }

    //--------------
    // head Table
    //
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

    info->UnitsPerEm = BigBytesToU16(data + info->TableHead + 18u);
    info->MinX = BigBytesToI16(data + info->TableHead + 36u);
    info->MinY = BigBytesToI16(data + info->TableHead + 38u);
    info->MaxX = BigBytesToI16(data + info->TableHead + 40u);
    info->MaxY = BigBytesToI16(data + info->TableHead + 42u);
    info->LocationFormat = BigBytesToI16(data + info->TableHead + 50u);

    //------------
    // cmap Table
    {
        // 2 - version
        // 2 - numTables

        u16 numTables = BigBytesToU16(data + info->TableCmap + 2u);

        // Encoding Record:
        // 2 - platformId
        // 2 - encodingId
        // 4 - subtableOffset

        u32 encodingTableOffset = 0u;
        u32 encodingRecordsOffset = info->TableCmap + 4u;
        for (u16 i = 0u; i != numTables; i++) {
            u32 iOffset = encodingRecordsOffset + i * 8u;
            u16 platformId = BigBytesToU16(data + iOffset);
            if (platformId == 0u) { // Unicode
                u16 encodingId = BigBytesToU16(data + iOffset + 2u);
                if (encodingId == 3u) { // BMP
                    encodingTableOffset = info->TableCmap + BigBytesToU32(data + iOffset + 4u);
                    break;
                }
            }
        }
        if (!encodingTableOffset) return 0;

        // Encoding Table:
        // 2 - format
        // 2 - length
        // 2 - language
        // 2 - segCountX2
        // 2 - searchRange
        // 2 - entrySelector
        // 2 - rangeShift
        // = 14

        u16 format = BigBytesToU16(data + encodingTableOffset);
        if (format != 4u) return 0u;
        info->EncodingSegmentCount = BigBytesToU16(data + encodingTableOffset + 6u) / 2u;

        // segmentCount * 2 - endCodes
        // 2 - reservedPad
        // segmentCount * 2 - startCodes
        // segmentCount * 2 - idDeltas
        // segmentCount * 2 - idRangeOffsets

        u32 arrayLength = info->EncodingSegmentCount * 2u;
        info->EncodingEndCodes = encodingTableOffset + 14u;
        info->EncodingStartCodes = info->EncodingEndCodes + arrayLength + 2u;
        info->EncodingIdDeltas = info->EncodingStartCodes + arrayLength;
        info->EncodingIdRangeOffsets = info->EncodingIdDeltas + arrayLength;
    }

    return 1;
}

int GetUnicodeGlyph(
    u8 * data,
    FontInfo * info,
    u16 codepoint,
    size_t * contourCount,
    size_t ** contourEnds,
    GlyphPoint ** points)
{
    //-------------------
    // Retrieve Location
    u16 glyphId = 0u;
    for (u16 i = 0u; i != info->EncodingSegmentCount; i++) {
        u32 iOffset = i * 2u;
        u16 endCode = BigBytesToU16(data + info->EncodingEndCodes + iOffset);
        if (endCode >= codepoint) {
            u16 startCode = BigBytesToU16(data + info->EncodingStartCodes + iOffset);
            if (startCode <= codepoint) {
                u16 idDelta = BigBytesToU16(data + info->EncodingIdDeltas + iOffset);
                u8 * idRangeOffsetPointer = data + info->EncodingIdRangeOffsets + iOffset;
                u16 idRangeOffset = BigBytesToU16(idRangeOffsetPointer);
                if (idRangeOffset != 0u) {
                    u8 * glyphIdPointer = idRangeOffsetPointer + idRangeOffset + 2u * (codepoint - startCode);
                    glyphId = BigBytesToU16(glyphIdPointer);
                    if (glyphId != 0u) {
                        glyphId += idDelta;
                    }
                } else {
                    glyphId = codepoint + idDelta;
                }
                break;
            }
        }
    }

    u32 glyphOffset = info->LocationFormat ?
        BigBytesToU32(data + info->TableLoca + glyphId * 4u) :
        BigBytesToU16(data + info->TableLoca + glyphId * 2u);

    // ----------------
    // Read Glyph Data
    {
        u32 currentOffset = info->TableGlyf + glyphOffset;

        // 2 - numberOfContours
        // 2 - xMin
        // 2 - yMin
        // 2 - xMax
        // 2 - yMax
        // = 10

        i16 initContourCount = BigBytesToI16(data + currentOffset);
        if (initContourCount == -1) return 0;
        *contourCount = initContourCount;
        currentOffset += 10u;

        // contourCount * 2 - contourEndPointIndices

        *contourEnds = (size_t *)platform.MemAlloc(*contourCount * sizeof(size_t));
        for (size_t i = 0u; i != *contourCount; i++) {
            (*contourEnds)[i] = BigBytesToU16(data + currentOffset + i * 2u);
        }
        size_t pointCount = (*contourEnds)[*contourCount - 1u] + 1u;
        currentOffset += *contourCount * 2u;

        // 2 - instructionLength
        // instructionLength * 1 - instructions

        currentOffset += 2u + BigBytesToU16(data + currentOffset);

        u8 * flags = (u8 *)platform.MemAlloc(pointCount);
        size_t i = 0u;
        while (i != pointCount) {
            u8 flag = *(data + currentOffset++);
            if (flag & 0x08u) {
                u8 repeatFlag = flag - 0x08u;
                flags[i++] = repeatFlag;
                u8 repeatCount = *(data + currentOffset++);
                for (u8 j = 0u; j != repeatCount; j++) {
                    flags[i++] = repeatFlag;
                }
            } else {
                flags[i++] = flag;
            }
        }

        *points = (GlyphPoint *)platform.MemAlloc(pointCount * sizeof(GlyphPoint));
        if (pointCount == 0u) {
            platform.MemFree(flags);
            return 1;
        }

        i16 currentCoord = 0;
        for (i = 0u; i != pointCount; i++) {
            if (flags[i] & 0x02u) {
                u8 coord = *(data + currentOffset);
                currentOffset += 1u;
                if (flags[i] & 0x10u) {
                    currentCoord += coord;
                } else {
                    currentCoord += coord * -1;
                }
            } else if (!(flags[i] & 0x10u)) {
                currentCoord += BigBytesToI16(data + currentOffset);
                currentOffset += 2u;
            }
            (*points)[i].X = currentCoord;
            (*points)[i].OnCurve = flags[i] & 0x01u;
        }

        currentCoord = 0;
        for (i = 0u; i != pointCount; i++) {
            if (flags[i] & 0x04u) {
                u8 coord = *(data + currentOffset);
                currentOffset += 1u;
                if (flags[i] & 0x20u) {
                    currentCoord += coord;
                } else {
                    currentCoord += coord * -1;
                }
            } else if (!(flags[i] & 0x20u)) {
                currentCoord += BigBytesToI16(data + currentOffset);
                currentOffset += 2u;
            }
            (*points)[i].Y = currentCoord;
        }

        platform.MemFree(flags);
    }

    return 1;
}

// TODO(mk): this stuff is just for testing
u16 * TextLine;
FontInfo fontInfo;
unsigned char * FontData;

void Setup(PlatformCallbacks platformCallbacks) {
    platform = platformCallbacks;
    
    //-----------
    // TEST CODE
    auto textFilePath = L"C:\\Users\\Marvin\\source\\repos\\MkEdit\\testoneline.txt";
    size_t textFileSize;
    auto textFile = platform.GetFileContent(textFilePath, &textFileSize);
    
    TextLine = DynArray_Create(u16);
    auto i = 0u;
    while (i != textFileSize) {
        u16 codepoint;
        if (!(textFile[i] & 0x80u)) {
            codepoint = textFile[i++];
        } else if ((textFile[i] & 0xE0u) == 0xC0u) {
            codepoint = textFile[i++] & 0x1Fu;
            codepoint <<= 6u;
            codepoint += textFile[i++] & 0x3Fu;
        } else if ((textFile[i] & 0xF0u) == 0xE0u) {
            codepoint = textFile[i++] & 0x1Fu;
            codepoint <<= 6u;
            codepoint += textFile[i++] & 0x3Fu;
            codepoint <<= 6u;
            codepoint += textFile[i++] & 0x3Fu;
        } else {
            codepoint = 0xFFFDu;
            i += 4u;
        }
        DynArray_Add(&TextLine, codepoint);
    }
    
    platform.MemFree(textFile);
    
    auto fontFilePath = L"C:\\Users\\Marvin\\source\\repos\\MkEdit\\consola.ttf";
    size_t fontFileSize;
    FontData = platform.GetFileContent(fontFilePath, &fontFileSize);
    FontInit(FontData, &fontInfo);
}

// TODO(mk): obviously just test values
const float FontSize = 11.0f;
const float Dpi = 96.0f;

struct RasterEdge {
    float TopY;
    float BottomY;
    float XHit;
    float Delta;
};

void SetViewport(unsigned long * bitmap, unsigned width, unsigned height) {
    viewportBitmap = bitmap;
    viewportWidth = width;
    viewportHeight = height;
    ClearViewportBitmap();

    // Text rendering goes here

    platform.ViewportUpdated();
}

#define DYNARRAY_ALLOC(size) platform.MemAlloc(size)
#define DYNARRAY_REALLOC(pointer, size) platform.MemReAlloc(pointer, size)
#define DYNARRAY_FREE(pointer) platform.MemFree(pointer)
#include "DynArray.cpp"