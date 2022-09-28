#include <Windows.h>
#include <wchar.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

typedef signed char i8;
typedef short i16;
typedef long i32;
typedef long long i64;

void* Heap;
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

        u16 fontTableCount = BigBytesToU16(Font + 4);

        // 4 - tag
        // 4 - checksum
        // 4 - offset
        // 4 - length

        for (int i = 0; i != fontTableCount; i++) {
            u32 offset = 12 + i * 16;
            char* tag = (char*)(Font + offset);
            u32 location = BigBytesToU32(Font + offset + 8);

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

        LocationOffsetType = BigBytesToI16(Font + headTableLocation + 50);
    }

    //--------------------------------
    // Find Encoding Table Location
    {
        // 2 - version
        // 2 - encodingRecordCount

        u16 encodingCount = BigBytesToU16(Font + cmapTableLocation + 2);

        // 2 - platformId
        // 2 - encodingId
        // 4 - subtableOffset

        u32 baseOffset = cmapTableLocation + 4;
        EncodingTableLocation = 0xFFFFFFFF;
        for (int i = 0; i != encodingCount; i++) {
            u32 offset = baseOffset + i * 8;
            u16 platformId = BigBytesToU16(Font + offset);
            if (platformId == 0) // Unicode
            {
                u16 encodingId = BigBytesToU16(Font + offset + 2);
                if (encodingId == 3) // BMP
                {
                    EncodingTableLocation = cmapTableLocation + BigBytesToU32(Font + offset + 4);
                    break;
                }
            }
        }
    }
}

int FindGlyph(u16 codepoint, u32* location, u32* length) {
    //----------------------
    // Search Mapping Table
    u16 glyphId = 0;
    {
        // 2 - format
        // 2 - length
        // 2 - language
        // 2 - segCountX2
        // 2 - searchRange
        // 2 - entrySelector
        // 2 - rangeShift
        // = 14

        u16 segmentCount = BigBytesToU16(Font + EncodingTableLocation + 6) / 2;

        // segmentCount * 2 - endCodes
        // 2 - reservedPad
        // segmentCount * 2 - startCodes
        // segmentCount * 2 - idDelta
        // segmentCount * 2 - idRangeOffsets

        u32 codesLength = segmentCount * 2;
        u32 endCodesOffset = EncodingTableLocation + 14;
        u32 startCodesOffset = endCodesOffset + 2 + codesLength;
        u32 idDeltasOffset = startCodesOffset + codesLength;
        u32 idRangesOffset = idDeltasOffset + codesLength;

        for (int i = 0; i != segmentCount; i++) {
            u32 indexOffset = i * 2;
            u16 endCode = BigBytesToU16(Font + endCodesOffset + indexOffset);
            if (endCode >= codepoint) {
                u16 startCode = BigBytesToU16(Font + startCodesOffset + indexOffset);
                if (startCode <= codepoint) {
                    u16 idDelta = BigBytesToU16(Font + idDeltasOffset + indexOffset);
                    u8* idRangeOffsetPointer = Font + idRangesOffset + indexOffset;
                    u16 idRangeOffset = BigBytesToU16(idRangeOffsetPointer);
                    if (idRangeOffset != 0) {
                        u8* glyphIdPointer = idRangeOffset + 2u * (codepoint - startCode) + idRangeOffsetPointer;
                        u16 initGlyphId = BigBytesToU16(glyphIdPointer);
                        if (initGlyphId != 0) {
                            glyphId = (initGlyphId + idDelta) % 65536u;
                        }
                    } else {
                        glyphId = (codepoint + idDelta) & 65536u;
                    }
                }
                break;
            }
        }

        if (glyphId == 0) {
            return 0;
        }
    }

    u32 nextLocation;
    if (LocationOffsetType) {
        u32 index = LocaTableLocation + glyphId * 4;
        *location = BigBytesToU32(Font + index);
        nextLocation = BigBytesToU32(Font + index + 4);
    } else {
        u32 index = LocaTableLocation + glyphId * 2;
        *location = BigBytesToU16(Font + index);
        nextLocation = BigBytesToU16(Font + index + 2);
    }
    *length = nextLocation - *location;

    return 1;
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
    u32 length;
    FindGlyph(0x0075, &location, &length);

    return 0;
}