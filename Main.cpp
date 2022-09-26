#include <Windows.h>
#include <wchar.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

struct FontTableRecord
{
    u8 Tag[4u];
    u32 Offset;
};

u32 BigBytesToU32(u8* bytes)
{
    u32 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    result <<= 8u;
    result += bytes[2u];
    result <<= 8u;
    result += bytes[3u];
    return result;
}

u16 BigBytesToU16(u8* bytes)
{
    u16 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    return result;
}

i16 BigBytesToI16(u8* bytes)
{
    i16 result = bytes[0u];
    result <<= 8u;
    result += bytes[1u];
    return result;
}

HANDLE FontFile;
DWORD DiscardBytesRead;
HANDLE Heap = NULL;
u8* Buffer = NULL;
u32 BufferSize = 0u;

void CopyFontFileBytesToBuffer(u32 count)
{
    if (count == 0u)
    {
        return;
    }

    if (count > BufferSize)
    {
        BufferSize = count;
        if (Buffer == NULL)
        {
            Buffer = (u8*)HeapAlloc(Heap, 0u, BufferSize);
        }
        else
        {
            Buffer = (u8*)HeapReAlloc(Heap, 0u, Buffer, BufferSize);
        }
    }

    ReadFile(FontFile, Buffer, count, &DiscardBytesRead, NULL);
}

FontTableRecord* FontTableRecords;
u16 FontTableCount;

u32 GetFontTableLocation(const char* tag)
{
    for (u32 i = 0u; i != FontTableCount; i++)
    {
        FontTableRecord record = FontTableRecords[i];
        int tagsEqual =
            record.Tag[0u] == tag[0u] &&
            record.Tag[1u] == tag[1u] &&
            record.Tag[2u] == tag[2u] &&
            record.Tag[3u] == tag[3u];
        if (tagsEqual)
        {
            return record.Offset;
        }
    }
    return 0xFFFFFFFF;
}

int SetFontFilePointerToTable(const char* tag)
{
    u32 location = GetFontTableLocation(tag);
    if (location == 0xFFFFFFFF)
    {
        return 0;
    }
    SetFilePointer(FontFile, location, NULL, FILE_BEGIN);
    return 1;
}

int APIENTRY WinMain(
    HINSTANCE instance,
    HINSTANCE prevInstance,
    PSTR commandLine,
    int showCommands)
{
    FontFile = CreateFileW(
        L"C:\\Users\\Marvin\\source\\repos\\MkEdit\\consola.ttf",
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0u,
        NULL);

    Heap = GetProcessHeap();

    //--------------------
    // Parse Main Table
    {
        // u32 - sfntVersion
        // u16 - tableCount
        // u16 - searchRange
        // u16 - entrySelector
        // u16 - rangeShift
        SetFilePointer(FontFile, sizeof(u32), NULL, FILE_BEGIN);
        CopyFontFileBytesToBuffer(sizeof(u16));
        FontTableCount = BigBytesToU16(Buffer);
        SetFilePointer(FontFile, 3u * sizeof(u16), NULL, FILE_CURRENT);

        // 4 * u8 - tag
        // u32 - checksum
        // u32 - offset
        // u32 - length
        u32 tableRecordTagLength = 4u * sizeof(u8);
        u32 tableRecordLength = (tableRecordTagLength + 3u * sizeof(u32));
        CopyFontFileBytesToBuffer(FontTableCount * tableRecordLength);
        FontTableRecords = (FontTableRecord*)HeapAlloc(Heap, 0u, FontTableCount * sizeof(FontTableRecord));
        for (u32 i = 0u; i != FontTableCount; i++)
        {
            u32 offset = i * tableRecordLength;
            FontTableRecord* tableRecord = &FontTableRecords[i];
            CopyMemory(tableRecord->Tag, &Buffer[offset], tableRecordTagLength);
            tableRecord->Offset = BigBytesToU32(&Buffer[offset + tableRecordTagLength + sizeof(u32)]);
        }
    }

    //--------------------
    // Parse Head Table
    i16 locationOffsetType;
    {
        SetFontFilePointerToTable("head");

        // u16 - majorVersion
        // u16 - minorVersion
        // i32 - fontRevision
        // u32 - checksumAdjustment
        // u32 - magicNumber
        // u16 - flags
        // u16 - unitsPerEm
        // i64 - created
        // i64 - modified
        // i16 - xMin
        // i16 - yMin
        // i16 - xMax
        // i16 - yMax
        // u16 - macStyle
        // u16 - lowestRecPpem
        // i16 - fontDirectionHint
        // i16 - indexToLocFormat
        // i16 - glyphDataFormat
        u32 skipBytes = 11u * sizeof(u16) + 3u * sizeof(u32) + 2u * sizeof(i64);
        SetFilePointer(FontFile, skipBytes, NULL, FILE_CURRENT);
        CopyFontFileBytesToBuffer(sizeof(u16));
        locationOffsetType = BigBytesToI16(Buffer);
    }

    //---------------------------
    // Parse Max Profile Table
    u16 glyphCount;
    {
        SetFontFilePointerToTable("maxp");

        // u32 - version
        // u16 - numGlyphs
        SetFilePointer(FontFile, sizeof(u32), NULL, FILE_CURRENT);
        CopyFontFileBytesToBuffer(sizeof(u16));
        glyphCount = BigBytesToU16(Buffer);
    }

    //-----------------------
    // Parse Location Table
    u32* glyphOffsets;
    {
        SetFontFilePointerToTable("loca");

        u32 offsetSize = locationOffsetType ? sizeof(u32) : sizeof(u16);
        u32 tableLength = glyphCount * offsetSize;
        glyphOffsets = (u32*)HeapAlloc(Heap, 0u, tableLength);
        CopyFontFileBytesToBuffer(tableLength);

        if (locationOffsetType)
        {
            for (u32 i = 0u; i != glyphCount; i++)
            {
                u32 offset = i * offsetSize;
                glyphOffsets[i] = BigBytesToU32(&Buffer[offset]);
            }
        }
        else
        {
            for (u32 i = 0u; i != glyphCount; i++)
            {
                u32 offset = i * offsetSize;
                glyphOffsets[i] = BigBytesToU16(&Buffer[offset]);
            }
        }
    }

    //-------------------
    // Parse Mapping Table
    u16* glyphIds;
    {
        u32 tableLocation = GetFontTableLocation("cmap");
        SetFilePointer(FontFile, tableLocation, NULL, FILE_BEGIN);

        // u16 - version
        // u16 - encodingRecordCount
        SetFilePointer(FontFile, sizeof(u16), NULL, FILE_CURRENT);
        CopyFontFileBytesToBuffer(sizeof(u16));
        u16 encodingCount = BigBytesToU16(Buffer);

        // u16 - platformId
        // u16 - encodingId
        // u32 - subtableOffset
        u32 encodingRecordLength = 2u * sizeof(u16) + sizeof(u32);
        CopyFontFileBytesToBuffer(encodingCount * encodingRecordLength);
        u32 encodingOffset;
        for (u32 i = 0u; i != encodingCount; i++)
        {
            u32 offset = i * encodingRecordLength;
            u16 platformId = BigBytesToU16(&Buffer[offset]);
            if (platformId == 0u) // Unicode
            {
                u16 encodingId = BigBytesToU16(&Buffer[offset + sizeof(u16)]);
                if (encodingId == 3u) // BMP
                {
                    encodingOffset = BigBytesToU32(&Buffer[offset + 2u * sizeof(u16)]);
                    break;
                }
            }
        }
        SetFilePointer(FontFile, tableLocation + encodingOffset, NULL, FILE_BEGIN);

        // u16 - format
        // u16 - length
        // u16 - language
        // u16 - segCountX2
        // u16 - searchRange
        // u16 - entrySelector
        // u16 - rangeShift
        SetFilePointer(FontFile, sizeof(u16), NULL, FILE_CURRENT);
        CopyFontFileBytesToBuffer(sizeof(u16));
        u16 encodingLength = BigBytesToU16(Buffer);
        SetFilePointer(FontFile, sizeof(u16), NULL, FILE_CURRENT);
        CopyFontFileBytesToBuffer(sizeof(u16));
        u16 segmentCount = BigBytesToU16(Buffer) / 2u;
        SetFilePointer(FontFile, 3u * sizeof(u16), NULL, FILE_CURRENT);

        // segmentCount * u16 - endCodes
        // u16 - reservedPad
        // segmentCount * u16 - startCodes
        // segmentCount * u16 - idDelta
        // segmentCount * u16 - idRangeOffsets
        u32 descriptorLength = (1u + 4u * segmentCount) * sizeof(u16);
        CopyFontFileBytesToBuffer(descriptorLength);

        u16* endCodes = (u16*)HeapAlloc(Heap, 0u, segmentCount * sizeof(u16));
        for (u32 i = 0u; i != segmentCount; i++)
        {
            endCodes[i] = BigBytesToU16(&Buffer[i * sizeof(u16)]);
        }

        u32 offset = (segmentCount + 1u) * sizeof(u16);
        u16* startCodes = (u16*)HeapAlloc(Heap, 0u, segmentCount * sizeof(u16));
        for (u32 i = 0u; i != segmentCount; i++)
        {
            startCodes[i] = BigBytesToU16(&Buffer[offset + i * sizeof(u16)]);
        }

        offset += segmentCount * sizeof(u16);
        u16* idDeltas = (u16*)HeapAlloc(Heap, 0u, segmentCount * sizeof(u16));
        for (u32 i = 0u; i != segmentCount; i++)
        {
            idDeltas[i] = BigBytesToU16(&Buffer[offset + i * sizeof(u16)]);
        }

        offset += segmentCount * sizeof(u16);
        u16* idRangeOffsets = (u16*)HeapAlloc(Heap, 0u, segmentCount * sizeof(u16));
        for (u32 i = 0u; i != segmentCount; i++)
        {
            idRangeOffsets[i] = BigBytesToU16(&Buffer[offset + i * sizeof(u16)]) / sizeof(u16);
        }

        u32 glyphIndicesLength = encodingLength - (7u * sizeof(u16) + descriptorLength);
        CopyFontFileBytesToBuffer(glyphIndicesLength);
        u32 glyphIndicesCount = glyphIndicesLength / 2u;
        u16* glyphIndices = (u16*)HeapAlloc(Heap, 0u, glyphIndicesLength);
        for (u32 i = 0u; i != glyphIndicesCount; i++)
        {
            glyphIndices[i] = BigBytesToU16(&Buffer[i * sizeof(u16)]);
        }

        glyphIds = (u16*)HeapAlloc(Heap, HEAP_ZERO_MEMORY, 0xFFFF * sizeof(u16));
        for (u16 codepoint = 0x0000; codepoint != 0xFFFF; codepoint++)
        {
            u32 segment;
            for (u32 i = 0u; i != segmentCount; i++)
            {
                if (endCodes[i] >= codepoint)
                {
                    segment = i;
                    break;
                }
            }
            if (startCodes[segment] <= codepoint)
            {
                if (idRangeOffsets[segment] != 0u)
                {
                    u32 index = idRangeOffsets[segment] + (codepoint - startCodes[segment]) - (segmentCount - segment);
                    u32 glyphId = glyphIndices[index];
                    if (glyphId != 0)
                    {
                        glyphIds[codepoint] = glyphId + idDeltas[segment] % 65536u;
                    }
                }
                else
                {
                    glyphIds[codepoint] = (u32)codepoint + idDeltas[segment] % 65536u;
                }
            }
        }
    }

    return 0;
}