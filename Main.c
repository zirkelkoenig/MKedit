#include <Windows.h>
#include <wchar.h>

void* Heap;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

typedef signed char i8;
typedef short i16;
typedef long i32;
typedef long long i64;

typedef struct {
    i16 X;
    i16 Y;
    i8 OnCurve;
} GlyphCoord;

u8* Font;
u32 LocaTableLocation;
i16 LocationOffsetType;
u32 EncodingTableLocation;
u32 GlyfTableLocation;

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

#if _DEBUG
    i16 xMin = BigBytesToI16(&Font[currentLocation + 2u]);
    i16 yMin = BigBytesToI16(&Font[currentLocation + 4u]);
    i16 xMax = BigBytesToI16(&Font[currentLocation + 6u]);
    i16 yMax = BigBytesToI16(&Font[currentLocation + 8u]);
#endif

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
        coord->X = xCoords[i];
        coord->Y = yCoords[i];
        coord->OnCurve = flags[i] & 0x01u;
    }

    HeapFree(Heap, 0, contourEndPointIndices);
    HeapFree(Heap, 0, flags);
    HeapFree(Heap, 0, xCoords);
    HeapFree(Heap, 0, yCoords);
    return coords;
}

int APIENTRY WinMain(
        HINSTANCE instance,
        HINSTANCE prevInstance,
        PSTR commandLine,
        int showCommands) {
    Heap = GetProcessHeap();
    InitFont();

    // test with letter 'u'
    u32 location;
    if (FindGlyph(0x0075u, &location)) {
        u16 coordCount;
        GlyphCoord* coords = GetGlyphData(location, &coordCount);
    }
}