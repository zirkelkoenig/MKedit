#ifndef MK_OPEN_TYPE_HEADER
#define MK_OPEN_TYPE_HEADER

#include <stdint.h>

typedef size_t MkSize;

typedef uint8_t MkU8;
typedef uint16_t MkU16;
typedef uint32_t MkU32;

typedef int8_t MkI8;
typedef int16_t MkI16;
typedef int32_t MkI32;

typedef struct {
    MkU32 LocaTableOffset;
    MkU32 GlyphTableOffset;
    MkU32 HeadTableOffset;
    MkU32 CmapTableOffset;
    
    MkU32 EncodingEndCodesOffset;
    MkU32 EncodingStartCodesOffset;
    MkU32 EncodingIdDeltasOffset;
    MkU32 EncodingIdRangeOffsetsOffset;

    MkI16 LocationFormat;
    MkU16 EncodingSegmentCount;

    MkI16 MinX;
    MkI16 MaxX;
    MkI16 MinY;
    MkI16 MaxY;
} MkFontInfo;

typedef struct {
    MkI16 X;
    MkI16 Y;
    MkU8 OnCurve;
} MkGlyphPoint;

int MkFontInfo_Create(MkU8* fontData, MkFontInfo* fontInfo);
MkU16 MkGetUnicodeGlyphId(MkU8* fontData, MkFontInfo* fontInfo, MkU16 codepoint);
MkU32 MkGlyphIdToOffset(MkU8* fontData, MkFontInfo* fontInfo, MkU16 glyphId);
int MkGetGlyphPoints(MkU8* fontData, MkFontInfo* fontInfo, MkU32 offset, MkU16* contourCount, MkU16** contourEnds, MkGlyphPoint** points);

static inline int MkGetUnicodeGlyphPoints(
        MkU8* fontData,
        MkFontInfo* fontInfo,
        MkU16 codepoint,
        MkU16* contourCount,
        MkU16** contourEnds,
        MkGlyphPoint** points) {
    MkU16 glyphId = MkGetUnicodeGlyphId(fontData, fontInfo, codepoint);
    MkU32 offset = MkGlyphIdToOffset(fontData, fontInfo, glyphId);
    return MkGetGlyphPoints(fontData, fontInfo, offset, contourCount, contourEnds, points);
}

#endif

#ifdef MK_OPEN_TYPE_IMPLEMENTATION

#if !(defined(MK_ALLOC) && defined(MK_REALLOC) && defined(MK_FREE) && defined(MK_MEMCOPY))
#if defined(MK_ALLOC) || defined(MK_REALLOC) || defined(MK_FREE) || defined(MK_MEMCOPY)
#error "You need to either define all or none of the MK header memory macros!"
#endif

#include <stdlib.h>
#include <string.h>

#define MK_ALLOC(size) malloc(size)
#define MK_REALLOC(pointer, size) realloc(pointer, size)
#define MK_FREE(pointer) free(pointer)
#define MK_MEMCOPY(destination, source, size) memcpy(destination, source, size)
#define MK_MEMMOVE(destination, source, size) memmove(destination, source, size)
#endif

static MkU16 BigBytesToU16(MkU8* bytes) {
    MkU16 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    return result;
}

static MkU32 BigBytesToU32(MkU8* bytes) {
    MkU32 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    result <<= 8u;
    result += bytes[2u];
    result <<= 8u;
    result += bytes[3u];
    return result;
}

static MkI16 BigBytesToI16(MkU8* bytes) {
    MkI16 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    return result;
}

static int TagsEqual(const char* tagA, const char* tagB) {
    return
            tagA[0u] == tagB[0u] &&
            tagA[1u] == tagB[1u] &&
            tagA[2u] == tagB[2u] &&
            tagA[3u] == tagB[3u];
}

int MkFontInfo_Create(MkU8* fontData, MkFontInfo* fontInfo) {
    //-------------------
    // Table Directory
    {
        // 4 - sfntVersion
        // 2 - numTables
        // 2 - searchRange
        // 2 - entrySelector
        // 2 - rangeShift
        // = 12

        MkU32 sfntVersion = BigBytesToU32(fontData);
        if (sfntVersion != 0x00010000u) return 0;

        MkU16 numTables = BigBytesToU16(fontData + 4u);

        // Table Record:
        // 4 - tag
        // 4 - checksum
        // 4 - offset
        // 4 - length

        fontInfo->LocaTableOffset = 0u;
        fontInfo->GlyphTableOffset = 0u;
        fontInfo->HeadTableOffset = 0u;
        fontInfo->CmapTableOffset = 0u;
        for (MkU16 i = 0u; i != numTables; i++) {
            MkU32 iOffset = 12u + i * 16u;
            char* tag = (char*)(fontData + iOffset);
            MkU32 tableOffset = BigBytesToU32(fontData + iOffset + 8u);

            if (TagsEqual(tag, "head")) fontInfo->HeadTableOffset = tableOffset;
            else if (TagsEqual(tag, "glyf")) fontInfo->GlyphTableOffset = tableOffset;
            else if (TagsEqual(tag, "loca")) fontInfo->LocaTableOffset = tableOffset;
            else if (TagsEqual(tag, "cmap")) fontInfo->CmapTableOffset = tableOffset;
        }
        int tablesFound =
                fontInfo->LocaTableOffset &&
                fontInfo->GlyphTableOffset &&
                fontInfo->HeadTableOffset &&
                fontInfo->CmapTableOffset;
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

    fontInfo->MinX = BigBytesToI16(fontData + fontInfo->HeadTableOffset + 36u);
    fontInfo->MinY = BigBytesToI16(fontData + fontInfo->HeadTableOffset + 38u);
    fontInfo->MaxX = BigBytesToI16(fontData + fontInfo->HeadTableOffset + 40u);
    fontInfo->MaxY = BigBytesToI16(fontData + fontInfo->HeadTableOffset + 42u);
    fontInfo->LocationFormat = BigBytesToI16(fontData + fontInfo->HeadTableOffset + 50u);

    //------------
    // cmap Table
    {
        // 2 - version
        // 2 - numTables

        MkU16 numTables = BigBytesToU16(fontData + fontInfo->CmapTableOffset + 2u);

        // Encoding Record:
        // 2 - platformId
        // 2 - encodingId
        // 4 - subtableOffset

        MkU32 encodingTableOffset = 0u;
        MkU32 encodingRecordsOffset = fontInfo->CmapTableOffset + 4u;
        for (MkU16 i = 0u; i != numTables; i++) {
            MkU32 iOffset = encodingRecordsOffset + i * 8u;
            MkU16 platformId = BigBytesToU16(fontData + iOffset);
            if (platformId == 0u) { // Unicode
                MkU16 encodingId = BigBytesToU16(fontData + iOffset + 2u);
                if (encodingId == 3u) { // BMP
                    encodingTableOffset = fontInfo->CmapTableOffset + BigBytesToU32(fontData + iOffset + 4u);
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

        MkU16 format = BigBytesToU16(fontData + encodingTableOffset);
        if (format != 4u) return 0u;
        fontInfo->EncodingSegmentCount = BigBytesToU16(fontData + encodingTableOffset + 6u) / 2u;

        // segmentCount * 2 - endCodes
        // 2 - reservedPad
        // segmentCount * 2 - startCodes
        // segmentCount * 2 - idDeltas
        // segmentCount * 2 - idRangeOffsets

        MkU32 arrayLength = fontInfo->EncodingSegmentCount * 2u;
        fontInfo->EncodingEndCodesOffset = encodingTableOffset + 14u;
        fontInfo->EncodingStartCodesOffset = fontInfo->EncodingEndCodesOffset + arrayLength + 2u;
        fontInfo->EncodingIdDeltasOffset = fontInfo->EncodingStartCodesOffset + arrayLength;
        fontInfo->EncodingIdRangeOffsetsOffset = fontInfo->EncodingIdDeltasOffset + arrayLength;
    }

    return 1;
}

MkU16 MkGetUnicodeGlyphId(MkU8* fontData, MkFontInfo* fontInfo, MkU16 codepoint) {
    for (MkU16 i = 0u; i != fontInfo->EncodingSegmentCount; i++) {
        MkU32 offset = i * 2u;
        MkU16 endCode = BigBytesToU16(fontData + fontInfo->EncodingEndCodesOffset + offset);
        if (endCode >= codepoint) {
            MkU16 startCode = BigBytesToU16(fontData + fontInfo->EncodingStartCodesOffset + offset);
            if (startCode <= codepoint) {
                MkU16 idDelta = BigBytesToU16(fontData + fontInfo->EncodingIdDeltasOffset + offset);
                MkU8* idRangeOffsetPointer = fontData + fontInfo->EncodingIdRangeOffsetsOffset + offset;
                MkU16 idRangeOffset = BigBytesToU16(idRangeOffsetPointer);
                if (idRangeOffset != 0u) {
                    MkU8* glyphIdPointer = idRangeOffsetPointer + idRangeOffset + 2u * (codepoint - startCode);
                    MkU16 glyphId = BigBytesToU16(glyphIdPointer);
                    if (glyphId != 0u) {
                        return glyphId + idDelta;
                    }
                } else {
                    return codepoint + idDelta;
                }
            }
        }
    }
    return 0u;
}

MkU32 MkGlyphIdToOffset(MkU8* fontData, MkFontInfo* fontInfo, MkU16 glyphId) {
    switch (fontInfo->LocationFormat) {
        case 0: return BigBytesToU16(fontData + fontInfo->LocaTableOffset + glyphId * 2u);
        case 1: return BigBytesToU32(fontData + fontInfo->LocaTableOffset + glyphId * 4u);
        default: return 0u;
    }
}

int MkGetGlyphPoints(MkU8* fontData, MkFontInfo* fontInfo, MkU32 offset, MkU16* contourCount, MkU16** contourEnds, MkGlyphPoint** points) {
    MkU32 currentOffset = fontInfo->GlyphTableOffset + offset;

    // 2 - numberOfContours
    // 2 - xMin
    // 2 - yMin
    // 2 - xMax
    // 2 - yMax
    // = 10

    MkI16 contourCountInternal = BigBytesToI16(fontData + currentOffset);
    if (contourCountInternal == -1) return 0;
    *contourCount = contourCountInternal;
    currentOffset += 10u;

    // contourCount * 2 - contourEndPointIndices

    *contourEnds = (MkU16*)MK_ALLOC(*contourCount * sizeof(MkU16));
    for (MkU16 i = 0u; i != *contourCount; i++) {
        (*contourEnds)[i] = BigBytesToU16(fontData + currentOffset + i * 2u);
    }
    MkU16 pointCount = (*contourEnds)[*contourCount - 1u] + 1u;
    currentOffset += *contourCount * 2u;

    // 2 - instructionLength
    // instructionLength * 1 - instructions

    currentOffset += 2u + BigBytesToU16(fontData + currentOffset);

    MkU8* flags = (MkU8*)MK_ALLOC(pointCount);
    MkU16 index = 0u;
    while (index != pointCount) {
        MkU8 flag = *(fontData + currentOffset++);
        if (flag & 0x08u) {
            MkU8 repeatFlag = flag - 0x08u;
            flags[index++] = repeatFlag;
            MkU8 repeatCount = *(fontData + currentOffset++);
            for (MkU8 j = 0u; j != repeatCount; j++) {
                flags[index++] = repeatFlag;
            }
        } else {
            flags[index++] = flag;
        }
    }

    *points = (MkGlyphPoint*)MK_ALLOC(pointCount * sizeof(MkGlyphPoint));
    if (pointCount == 0u) {
        MK_FREE(flags);
        return 1;
    }

    MkI16 current = 0;
    for (MkU16 i = 0u; i != pointCount; i++) {
        if (flags[i] & 0x02u) {
            MkU8 coord = *(fontData + currentOffset);
            currentOffset += 1u;
            if (flags[i] & 0x10u) {
                current += coord;
            } else {
                current += coord * -1;
            }
        } else if (!(flags[i] & 0x10u)) {
            current += BigBytesToI16(fontData + currentOffset);
            currentOffset += 2u;
        }
        (*points)[i].X = current;
        (*points)[i].OnCurve = flags[i] & 0x01u;
    }

    current = 0;
    for (MkU16 i = 0u; i != pointCount; i++) {
        if (flags[i] & 0x04u) {
            MkU8 coord = *(fontData + currentOffset);
            currentOffset += 1u;
            if (flags[i] & 0x20u) {
                current += coord;
            } else {
                current += coord * -1;
            }
        } else if (!(flags[i] & 0x20u)) {
            current += BigBytesToI16(fontData + currentOffset);
            currentOffset += 2u;
        }
        (*points)[i].Y = current;
    }

    MK_FREE(flags);
    return 1;
}

#endif