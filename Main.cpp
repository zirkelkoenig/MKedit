#include <Windows.h>
#include <wchar.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

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

u16* GlyphIds;

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

    // u32 - sfntVersion
    // u16 - tableCount
    // u16 - searchRange
    // u16 - entrySelector
    // u16 - rangeShift
    SetFilePointer(FontFile, sizeof(u32), NULL, FILE_BEGIN);
    CopyFontFileBytesToBuffer(sizeof(u16));
    u16 tableCount = BigBytesToU16(Buffer);
    SetFilePointer(FontFile, 3u * sizeof(u16), NULL, FILE_CURRENT);

    // 4 * u8 - tag
    // u32 - checksum
    // u32 - offset
    // u32 - length
    u32 tableRecordTagLength = 4u * sizeof(u8);
    u32 tableRecordLength = (tableRecordTagLength + 3u * sizeof(u32));
    CopyFontFileBytesToBuffer(tableCount * tableRecordLength);
    FontTableRecord* tableRecords = (FontTableRecord*)HeapAlloc(Heap, 0u, tableCount * sizeof(FontTableRecord));
    for (u32 i = 0u; i != tableCount; i++)
    {
        u32 offset = i * tableRecordLength;
        FontTableRecord* tableRecord = &tableRecords[i];
        CopyMemory(tableRecord->Tag, &Buffer[offset], tableRecordTagLength);
        tableRecord->Offset = BigBytesToU32(&Buffer[offset + tableRecordTagLength + sizeof(u32)]);
    }

    u32 cmapLocation = 0u;
    for (u32 i = 0u; i != tableCount; i++)
    {
        FontTableRecord record = tableRecords[i];
        int tagsEqual =
            record.Tag[0u] == 'c' &&
            record.Tag[1u] == 'm' &&
            record.Tag[2u] == 'a' &&
            record.Tag[3u] == 'p';
        if (tagsEqual)
        {
            cmapLocation = record.Offset;
            break;
        }
    }
    SetFilePointer(FontFile, cmapLocation, NULL, FILE_BEGIN);

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
    SetFilePointer(FontFile, cmapLocation + encodingOffset, NULL, FILE_BEGIN);

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

    GlyphIds = (u16*)HeapAlloc(Heap, HEAP_ZERO_MEMORY, 0xFFFF * sizeof(u16));
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
                    GlyphIds[codepoint] = glyphId + idDeltas[segment] % 65536u;
                }
            }
            else
            {
                GlyphIds[codepoint] = (u32)codepoint + idDeltas[segment] % 65536u;
            }
        }
    }

    return 0;
}