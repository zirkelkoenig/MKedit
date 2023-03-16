#include "Main.h"
#include "DynArray.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

enum class Utf8ParserState
{
    Start,
    Success,
    Error,
    Read1,
    Read2,
    Read3,
};

wchar_t ** ParseUtf8(u8 * input, size_t inputSize, int * containsErrors)
{
    *containsErrors = 0;
    wchar_t ** lines = DynArrayCreate(wchar_t *, 8);

    wchar_t * line = 0;
    u32 codepoint;
    int i = 0;
    Utf8ParserState state = Utf8ParserState::Start;
    while (i != inputSize || state == Utf8ParserState::Success)
    {
        switch (state)
        {
            case Utf8ParserState::Start:
            {
                if (!line)
                {
                    line = DynArrayCreate(wchar_t, 8);
                }
                codepoint = 0u;

                u8 byte = input[i];
                if ((byte & 0b10000000u) == 0b00000000u)
                {
                    codepoint = byte & 0b01111111u;
                    state = Utf8ParserState::Success;
                }
                else if ((byte & 0b11000000u) == 0b10000000u)
                {
                    state = Utf8ParserState::Error;
                }
                else if ((byte & 0b11100000u) == 0b11000000u)
                {
                    codepoint = (byte & 0b00011111u) << 6u;
                    state = inputSize - i >= 2 ? Utf8ParserState::Read1 : Utf8ParserState::Error;
                }
                else if ((byte & 0b11110000u) == 0b11100000u)
                {
                    codepoint = (byte & 0b00001111u) << 12u;
                    state = inputSize - i >= 3 ? Utf8ParserState::Read2 : Utf8ParserState::Error;
                }
                else if ((byte & 0b11111000u) == 0b11110000u)
                {
                    codepoint = (byte & 0b00000111u) << 18u;
                    state = inputSize - i >= 4 ? Utf8ParserState::Read3 : Utf8ParserState::Error;
                }
                else
                {
                    state = Utf8ParserState::Error;
                }
                i++;
                break;
            }

            case Utf8ParserState::Read1:
            {
                u8 byte = input[i++];
                if ((byte & 0b11000000u) == 0b10000000u)
                {
                    codepoint += byte & 0b00111111u;
                    state = Utf8ParserState::Success;
                }
                else
                {
                    state = Utf8ParserState::Error;
                }
                break;
            }

            case Utf8ParserState::Read2:
            {
                u8 byte = input[i++];
                if ((byte & 0b11000000u) == 0b10000000u)
                {
                    codepoint += (byte & 0b00111111u) << 6u;
                    state = Utf8ParserState::Read1;
                }
                else
                {
                    state = Utf8ParserState::Error;
                }
                break;
            }

            case Utf8ParserState::Read3:
            {
                u8 byte = input[i++];
                if ((byte & 0b11000000u) == 0b10000000u)
                {
                    codepoint += (byte & 0b00111111u) << 12u;
                    state = Utf8ParserState::Read2;
                }
                else
                {
                    state = Utf8ParserState::Error;
                }
                break;
            }

            case Utf8ParserState::Success:
            {
                if (codepoint > 0xffffu)
                {
                    codepoint = 0xfffdu;
                }

                if (codepoint == 0x000a) // LF
                {
                    size_t prevCol = DynArrayCount(line) - 1;
                    if (line[prevCol] == 0x000d) // CR
                    {
                        DynArraySetCount(&line, prevCol);
                    }
                    DynArrayAppend(&lines, line);
                    line = 0;
                }
                else
                {
                    DynArrayAppend(&line, (wchar_t)codepoint);
                }
                state = Utf8ParserState::Start;
                break;
            }

            case Utf8ParserState::Error:
            {
                DynArrayAppend(&line, (wchar_t)0xfffdu);
                state = Utf8ParserState::Start;
                *containsErrors = 1;
                break;
            }
        }
    }
    if (state != Utf8ParserState::Start)
    {
        *containsErrors = 1;
        DynArrayAppend(&line, (wchar_t)0xfffdu);
    }
    if (line)
    {
        DynArrayAppend(&lines, line);
    }
    return lines;
}

void Run()
{
    size_t utf8TextSize;
    int textContainsErrors;

    u8 * utf8Text = GetFileContent(L"testcrlf.txt", &utf8TextSize);
    wchar_t ** lines = ParseUtf8(utf8Text, utf8TextSize, &textContainsErrors);
    MemFree(utf8Text);
}
