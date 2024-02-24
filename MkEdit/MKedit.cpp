#include <Windows.h>

#include <KnownFolders.h>
#include <objbase.h>
#include <ShlObj.h>

#include <stdio.h>
#include <stdlib.h>
#include <wctype.h>

#include "Generated/ConfigGen.h"

typedef unsigned char byte;
typedef unsigned short ushort;
typedef unsigned long ulong;

const ulong maxRows = ULONG_MAX;
const ushort maxRowLength = USHRT_MAX;

Config config;

//---------------------
// RUNTIME CONSTANTS

HBRUSH textBrush;
HBRUSH backgroundBrush;
HBRUSH cursorBrush;
HBRUSH statusBackgroundBrush;
HBRUSH docTitleBackgroundBrush;
HBRUSH promptTextBrush;
HBRUSH promptBackgroundBrush;

wchar_t * tabSpaces;

//--------------------------
// BASIC DATA STRUCTURES

#define INIT_ROW_CAPACITY 4
#define TEXTBUFFER_GROW_COUNT 16

struct DocLine {
    ushort length;
    ushort capacity;
    wchar_t * text;
};

struct DocLines {
    ulong textBufferCount;
    ulong textBufferCapacity;
    DocLine * textBuffer;
};

struct Doc {
    DocLines lines;
    ulong cursorRowIndex;
    ushort cursorColIndex;
    ulong cursorColLastDrawIndex;
    ulong topDrawRowIndex;
    ulong lastDrawRowCount;
    bool modified;
};

Doc * CreateEmptyDoc() {
    Doc * docPtr = (Doc *)malloc(sizeof(Doc));
    memset(docPtr, 0, sizeof(Doc));

    docPtr->lines.textBufferCount = 1;
    docPtr->lines.textBufferCapacity = TEXTBUFFER_GROW_COUNT;
    docPtr->lines.textBuffer = (DocLine *)malloc(TEXTBUFFER_GROW_COUNT * sizeof(DocLine));

    DocLine * linePtr = &docPtr->lines.textBuffer[0];
    linePtr->length = 0;
    linePtr->capacity = INIT_ROW_CAPACITY;
    linePtr->text = (wchar_t *)malloc(INIT_ROW_CAPACITY * sizeof(wchar_t));

    return docPtr;
}

void DestroyDoc(Doc * docPtr) {
    for (ulong i = 0; i != docPtr->lines.textBufferCount; i++) {
        free(docPtr->lines.textBuffer[i].text);
    }
    free(docPtr->lines.textBuffer);
    free(docPtr);
}

Doc * onlyDoc;

bool fontSet;
long lineHeight;
long avgCharWidth;

enum VimMode {
    MODE_VIM_NORMAL,
    MODE_VIM_INSERT,
};

VimMode currentVimMode = MODE_VIM_NORMAL;

bool paintStatusMessage;

void UpdateColDrawIndex(Doc * doc) {
    wchar_t * text = doc->lines.textBuffer[doc->cursorRowIndex].text;
    doc->cursorColLastDrawIndex = 0;
    for (ushort i = 0; i != doc->cursorColIndex; i++) {
        if (text[i] == L'\t') {
            doc->cursorColLastDrawIndex += config.tabWidth;
        } else {
            doc->cursorColLastDrawIndex++;
        }
    }
}

void ApplyColDrawIndex(Doc * doc) {
    DocLine * rowPtr = &doc->lines.textBuffer[doc->cursorRowIndex];
    ulong cursorColDrawIndex = 0;
    for (doc->cursorColIndex = 0; doc->cursorColIndex != rowPtr->length; doc->cursorColIndex++) {
        if (cursorColDrawIndex >= doc->cursorColLastDrawIndex) {
            if (cursorColDrawIndex > doc->cursorColLastDrawIndex) {
                doc->cursorColIndex--;
            }
            break;
        }

        if (rowPtr->text[doc->cursorColIndex] == L'\t') {
            cursorColDrawIndex += config.tabWidth;
        } else {
            cursorColDrawIndex++;
        }
    }
}

bool ProcessKeydownVimNormal(WPARAM wparam) {
    switch (wparam) {
        default:
        {
            return FALSE;
        }
    }
}

bool ProcessKeydownVimInsert(WPARAM wparam) {
    switch (wparam) {
        default:
        {
            return FALSE;
        }
    }
}

bool ProcessKeydownVim(WPARAM wparam) {
    switch (currentVimMode) {
        case MODE_VIM_NORMAL:
            return ProcessKeydownVimNormal(wparam);

        default:
            return FALSE;
    }
}

bool ProcessCharVimNormal(Doc * doc, wchar_t wc) {
    switch (wc) {
        case L'h':
        {
            if (doc->cursorColIndex != 0) {
                doc->cursorColIndex--;
            }
            UpdateColDrawIndex(doc);
            paintStatusMessage = FALSE;
            return TRUE;
        }

        case L'l':
        {
            ushort rowLength = doc->lines.textBuffer[doc->cursorRowIndex].length;
            if (rowLength != 0 && doc->cursorColIndex != rowLength - 1) {
                doc->cursorColIndex++;
            }
            UpdateColDrawIndex(doc);
            paintStatusMessage = FALSE;
            return TRUE;
        }

        case L'k':
        {
            if (doc->cursorRowIndex != 0) {
                doc->cursorRowIndex--;
                ApplyColDrawIndex(doc);
            }
            paintStatusMessage = FALSE;
            return TRUE;
        }

        case L'j':
        {
            if (doc->cursorRowIndex != doc->lines.textBufferCount - 1) {
                doc->cursorRowIndex++;
                ApplyColDrawIndex(doc);
            }
            paintStatusMessage = FALSE;
            return TRUE;
        }

        case L'0':
        {
            doc->cursorColIndex = 0;
            doc->cursorColLastDrawIndex = 0;
            paintStatusMessage = FALSE;
            return TRUE;
        }

        case L'$':
        {
            ushort rowLength = doc->lines.textBuffer[doc->cursorRowIndex].length;
            if (rowLength != 0) {
                doc->cursorColIndex = rowLength - 1;
            }
            UpdateColDrawIndex(doc);
            paintStatusMessage = FALSE;
            return TRUE;
        }

        case L'i':
        {
            currentVimMode = MODE_VIM_INSERT;
            paintStatusMessage = FALSE;
            return TRUE;
        }

        default:
        {
            return FALSE;
        }
    }
}

void InsertChars(Doc * doc, wchar_t * wcs, ushort count) {
    DocLine * rowPtr = &doc->lines.textBuffer[doc->cursorRowIndex];
    rowPtr->length += count;

    bool grow = FALSE;
    while (rowPtr->length > rowPtr->capacity) {
        rowPtr->capacity *= 2;
        grow = TRUE;
    }
    if (grow) {
        rowPtr->text = (wchar_t *)realloc(rowPtr->text, rowPtr->capacity * sizeof(wchar_t));
    }

    for (ushort i = rowPtr->length - 1; i != doc->cursorColIndex + count - 1; i--) {
        rowPtr->text[i] = rowPtr->text[i - count];
    }
    for (ushort i = 0; i != count; i++) {
        rowPtr->text[doc->cursorColIndex++] = wcs[i];
    }
}

bool ProcessCharVimInsert(Doc * doc, wchar_t wc) {
    switch (wc) {
        case L'\t':
        {
            if (config.expandTabs) {
                InsertChars(doc, tabSpaces, config.tabWidth);
            } else {
                InsertChars(doc, &wc, 1);
            }
            UpdateColDrawIndex(doc);
            paintStatusMessage = FALSE;
            doc->modified = TRUE;
            return TRUE;
        }

        case L'\r':
        {
            if (doc->lines.textBufferCount == doc->lines.textBufferCapacity) {
                doc->lines.textBufferCapacity += TEXTBUFFER_GROW_COUNT;
                doc->lines.textBuffer = (DocLine *)realloc(doc->lines.textBuffer, doc->lines.textBufferCapacity * sizeof(DocLine));
            }
            doc->lines.textBufferCount++;
            for (ulong i = doc->lines.textBufferCount - 1; i != doc->cursorRowIndex + 1; i--) {
                doc->lines.textBuffer[i] = doc->lines.textBuffer[i - 1];
            }

            DocLine * curRowPtr = &doc->lines.textBuffer[doc->cursorRowIndex];
            DocLine * nextRowPtr = curRowPtr + 1;
            nextRowPtr->length = curRowPtr->length - doc->cursorColIndex;
            nextRowPtr->capacity = curRowPtr->capacity;
            nextRowPtr->text = (wchar_t *)malloc(nextRowPtr->capacity * sizeof(wchar_t));
            for (ushort i = 0; i != nextRowPtr->length; i++) {
                nextRowPtr->text[i] = curRowPtr->text[doc->cursorColIndex + i];
            }
            curRowPtr->length = doc->cursorColIndex;
            doc->cursorRowIndex++;
            doc->cursorColIndex = 0;
            doc->cursorColLastDrawIndex = 0;

            paintStatusMessage = FALSE;
            doc->modified = TRUE;
            return TRUE;
        }

        case '\b':
        {
            if (doc->cursorColIndex == 0) {
                if (doc->cursorRowIndex != 0) {
                    DocLine * curRowPtr = &doc->lines.textBuffer[doc->cursorRowIndex];
                    DocLine * prevRowPtr = curRowPtr - 1;
                    doc->cursorRowIndex--;
                    ushort prevLength = prevRowPtr->length;
                    doc->cursorColIndex = prevLength;
                    InsertChars(doc, curRowPtr->text, curRowPtr->length);
                    free(curRowPtr->text);
                    for (ulong i = doc->cursorRowIndex + 1; i != doc->lines.textBufferCount - 1; i++) {
                        doc->lines.textBuffer[i] = doc->lines.textBuffer[i + 1];
                    }
                    doc->lines.textBufferCount--;
                    doc->cursorColIndex = prevLength;
                    doc->modified = TRUE;
                }
            } else {
                doc->cursorColIndex--;
                DocLine * rowPtr = &doc->lines.textBuffer[doc->cursorRowIndex];
                for (ushort i = doc->cursorColIndex; i != rowPtr->length - 1; i++) {
                    rowPtr->text[i] = rowPtr->text[i + 1];
                }
                rowPtr->length--;
                doc->modified = TRUE;
            }

            UpdateColDrawIndex(doc);
            paintStatusMessage = FALSE;
            return TRUE;
        }

        case 0x1b: // Esc
        {
            ushort rowLength = doc->lines.textBuffer[doc->cursorRowIndex].length;
            if (rowLength != 0 && doc->cursorColIndex == rowLength) {
                doc->cursorColIndex--;
                if (doc->cursorColLastDrawIndex != 0) {
                    doc->cursorColLastDrawIndex--;
                }
            }
            currentVimMode = MODE_VIM_NORMAL;
            paintStatusMessage = FALSE;
            return TRUE;
        }

        default:
        {
            if (iswcntrl(wc)) {
                return FALSE;
            }

            InsertChars(doc, &wc, 1);
            UpdateColDrawIndex(doc);
            paintStatusMessage = FALSE;
            doc->modified = TRUE;
            return TRUE;
        }
    }
}

bool ProcessCharVim(Doc * doc, wchar_t wc) {
    switch (currentVimMode) {
        case MODE_VIM_NORMAL:
            return ProcessCharVimNormal(doc, wc);

        case MODE_VIM_INSERT:
            return ProcessCharVimInsert(doc, wc);

        default:
            return FALSE;
    }
}

enum Mode {
    MODE_INSERT,
    MODE_OPEN_FILE,
    MODE_SAVE_FILE,
};

wchar_t workingFolderPath[MAX_PATH];
wchar_t filePath[MAX_PATH];
Mode currentMode;
ushort statusCursorCol;
ushort statusInputLength;
wchar_t statusInput[2 * MAX_PATH];

void TrySaveFile(DocLines * docLinesPtr) {
    statusInput[statusInputLength] = L'\0';
    HANDLE file = CreateFileW(
        statusInput,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        wcscpy_s(statusInput, 2 * MAX_PATH, L"File could not be saved!");
        statusInputLength = (ushort)wcslen(statusInput);
        paintStatusMessage = TRUE;
        currentMode = MODE_INSERT;
        return;
    }

    wcscpy_s(filePath, MAX_PATH, statusInput);
    currentMode = MODE_INSERT;

    DocLine * rowPtr = &docLinesPtr->textBuffer[0];
    byte output[4];
    ushort outputCount;
    ulong c;
    ulong currentCount;

    byte newline[] = { 0x0d, 0x0a };

    for (ushort j = 0; j != rowPtr->length; j++) {
        outputCount = 0;
        c = rowPtr->text[j];

        if (c <= 0x007f) {
            output[0] = (byte)c;
            outputCount = 1;
        } else if (c <= 0x07ff) {
            output[0] = (byte)((c >> 6) + 0b11000000);
            output[1] = (byte)((c & 0b00111111) + 0b10000000);
            outputCount = 2;
        } else if (c <= 0xffff) {
            output[0] = (byte)((c >> 12) + 0b11100000);
            output[1] = (byte)((c >> 6 & 0b00111111) + 0b10000000);
            output[2] = (byte)((c & 0b00111111) + 0b10000000);
            outputCount = 3;
        } else if (c <= 0x0010ffff) {
            output[0] = (byte)((c >> 18) + 0b11110000);
            output[1] = (byte)((c >> 12 & 0b00111111) + 0b10000000);
            output[2] = (byte)((c >> 6 & 0b00111111) + 0b10000000);
            output[3] = (byte)((c & 0b00111111) + 0b10000000);
            outputCount = 4;
        } else {
            // 0xFFFD
            output[0] = 0b11101111;
            output[1] = 0b10111111;
            output[2] = 0b10111101;
            outputCount = 3;
        }

        WriteFile(file, output, outputCount, &currentCount, NULL);
    }

    for (ulong i = 1; i != docLinesPtr->textBufferCount; i++) {
        rowPtr = &docLinesPtr->textBuffer[i];
        WriteFile(file, newline, 2, &currentCount, NULL);

        for (ushort j = 0; j != rowPtr->length; j++) {
            outputCount = 0;
            c = rowPtr->text[j];

            if (c <= 0x007f) {
                output[0] = (byte)c;
                outputCount = 1;
            } else if (c <= 0x07ff) {
                output[0] = (byte)((c >> 6) + 0b11000000);
                output[1] = (byte)((c & 0b00111111) + 0b10000000);
                outputCount = 2;
            } else if (c <= 0xffff) {
                output[0] = (byte)((c >> 12) + 0b11100000);
                output[1] = (byte)((c >> 6 & 0b00111111) + 0b10000000);
                output[2] = (byte)((c & 0b00111111) + 0b10000000);
                outputCount = 3;
            } else if (c <= 0x0010ffff) {
                output[0] = (byte)((c >> 18) + 0b11110000);
                output[1] = (byte)((c >> 12 & 0b00111111) + 0b10000000);
                output[2] = (byte)((c >> 6 & 0b00111111) + 0b10000000);
                output[3] = (byte)((c & 0b00111111) + 0b10000000);
                outputCount = 4;
            } else {
                // 0xFFFD
                output[0] = 0b11101111;
                output[1] = 0b10111111;
                output[2] = 0b10111101;
                outputCount = 3;
            }

            WriteFile(file, output, outputCount, &currentCount, NULL);
        }
    }

    CloseHandle(file);
}

void AppendChar(DocLine * rowPtr, wchar_t c) {
    if (rowPtr->length == rowPtr->capacity) {
        rowPtr->capacity *= 2;
        rowPtr->text = (wchar_t *)realloc(rowPtr->text, rowPtr->capacity * sizeof(wchar_t));
    }
    rowPtr->text[rowPtr->length++] = c;
}

enum Utf8State {
    UTF8_START,
    UTF8_END_OK,
    UTF8_END_ERROR,
    UTF8_READ_1,
    UTF8_READ_2,
    UTF8_READ_3,
    UTF8_READ_ERROR,
};

Doc * TryLoadFilePath(wchar_t * filePath, bool appendNull);

Doc * TryLoadFile() {
    statusInput[statusInputLength] = L'\0';
    Doc * result = TryLoadFilePath(statusInput, FALSE);
    if (!result) {
        wcscpy_s(statusInput, 2 * MAX_PATH, L"File could not be opened!");
        statusInputLength = (ushort)wcslen(statusInput);
        paintStatusMessage = TRUE;
        currentMode = MODE_INSERT;
    }
    return result;
}

Doc * TryLoadFilePath(wchar_t * filePath, bool appendNull) {
    HANDLE file = CreateFileW(
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    wcscpy_s(filePath, MAX_PATH, statusInput);

    Doc * docPtr = CreateEmptyDoc();
    DocLines * docLines = &docPtr->lines;
    DocLine * rowPtr = &docLines->textBuffer[0];

    currentMode = MODE_INSERT;

    bool limited = sizeof(wchar_t) < 4;
    
    Utf8State state = UTF8_START;
    ulong c;
    byte currentByte;
    ulong currentCount;
    ReadFile(file, &currentByte, 1, &currentCount, NULL);
    bool lastWasNewline = FALSE;
    while (currentCount != 0 || state == UTF8_END_OK || state == UTF8_END_ERROR) {
        switch (state) {
            case UTF8_START:
            {
                if ((currentByte & 0b10000000) == 0b00000000) {
                    c = currentByte;
                    ReadFile(file, &currentByte, 1, &currentCount, NULL);
                    state = UTF8_END_OK;
                } else if ((currentByte & 0b11000000) == 0b10000000) {
                    ReadFile(file, &currentByte, 1, &currentCount, NULL);
                    state = UTF8_READ_ERROR;
                } else if ((currentByte & 0b11100000) == 0b11000000) {
                    c = (currentByte & 0b00011111) << 6;
                    ReadFile(file, &currentByte, 1, &currentCount, NULL);
                    state = UTF8_READ_1;
                } else if ((currentByte & 0b11110000) == 0b11100000) {
                    c = (currentByte & 0b00001111) << 12;
                    ReadFile(file, &currentByte, 1, &currentCount, NULL);
                    state = UTF8_READ_2;
                } else if ((currentByte & 0b11111000) == 0b11110000) {
                    c = (currentByte & 0b00000111) << 18;
                    ReadFile(file, &currentByte, 1, &currentCount, NULL);
                    state = UTF8_READ_3;
                } else {
                    ReadFile(file, &currentByte, 1, &currentCount, NULL);
                    state = UTF8_END_ERROR;
                }
                break;
            }

            case UTF8_END_OK:
            {
                if (lastWasNewline) {
                    lastWasNewline = FALSE;
                    if (c == L'\n') {
                        state = UTF8_START;
                        break;
                    }
                }

                if (c > 0xffff && limited) {
                    AppendChar(rowPtr, 0xfffd);
                } else if (c == L'\0') {
                    currentCount = 0;
                } else if (c == L'\n' || c == L'\r') {
                    if (appendNull) {
                        AppendChar(rowPtr, L'\0');
                        rowPtr->length--;
                    }

                    if (docLines->textBufferCount == docLines->textBufferCapacity) {
                        docLines->textBufferCapacity += TEXTBUFFER_GROW_COUNT;
                        docLines->textBuffer = (DocLine *)realloc(docLines->textBuffer, docLines->textBufferCapacity * sizeof(DocLine));
                    }
                    rowPtr = &docLines->textBuffer[docLines->textBufferCount++];

                    rowPtr->length = 0;
                    rowPtr->capacity = INIT_ROW_CAPACITY;
                    rowPtr->text = (wchar_t *)malloc(INIT_ROW_CAPACITY * sizeof(wchar_t));
                    lastWasNewline = c == L'\r';
                } else {
                    AppendChar(rowPtr, (wchar_t)c);
                }
                state = UTF8_START;
                break;
            }

            case UTF8_END_ERROR:
            {
                AppendChar(rowPtr, 0xfffd);
                state = UTF8_START;
                break;
            }

            case UTF8_READ_1:
            {
                if ((currentByte & 0b11000000) == 0b10000000) {
                    c += currentByte & 0b00111111;
                    state = UTF8_END_OK;
                } else {
                    state = UTF8_END_ERROR;
                }
                ReadFile(file, &currentByte, 1, &currentCount, NULL);
                break;
            }

            case UTF8_READ_2:
            {
                if ((currentByte & 0b11000000) == 0b10000000) {
                    c += (currentByte & 0b00111111) << 6;
                    state = UTF8_READ_1;
                } else {
                    state = UTF8_END_ERROR;
                }
                ReadFile(file, &currentByte, 1, &currentCount, NULL);
                break;
            }

            case UTF8_READ_3:
            {
                if ((currentByte & 0b11000000) == 0b10000000) {
                    c += (currentByte & 0b00111111) << 12;
                    state = UTF8_READ_2;
                } else {
                    state = UTF8_END_ERROR;
                }
                ReadFile(file, &currentByte, 1, &currentCount, NULL);
                break;
            }

            case UTF8_READ_ERROR:
            {
                if ((currentByte & 0b11000000) != 0b10000000) {
                    state = UTF8_END_ERROR;
                }
                ReadFile(file, &currentByte, 1, &currentCount, NULL);
                break;
            }
        }
    }
    if (state != UTF8_START) {
        AppendChar(rowPtr, 0xfffd);
    }
    if (appendNull) {
        AppendChar(rowPtr, L'\0');
        rowPtr->length--;
    }

    CloseHandle(file);
    return docPtr;
}

wchar_t * wcschr_s(wchar_t * wcs, ushort length, wchar_t c) {
    wchar_t * end = wcs + length;
    while (wcs != end && *wcs != L'\0') {
        if (*wcs == c) {
            return wcs;
        }
        wcs++;
    }
    return NULL;
}

HDC bitmapDeviceContext;
ushort bitmapWidth, bitmapHeight;

void PaintCursorRow(
    RECT * textRectPtr,
    wchar_t * text,
    ushort length,
    ushort colIndex)
{
    SIZE extent;

    wchar_t * currentText = text;
    ushort currentLength = length;
    wchar_t * nextTabPos = wcschr_s(currentText, currentLength, L'\t');
    long cursorOffset = colIndex;
    while (nextTabPos) {
        ushort nextTabOffset = (ushort)(nextTabPos - currentText);
        if (cursorOffset < 0 || cursorOffset >= nextTabOffset) {
            GetTextExtentPoint32W(
                bitmapDeviceContext,
                currentText,
                nextTabOffset,
                &extent);
            DrawTextExW(
                bitmapDeviceContext,
                currentText,
                nextTabOffset,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
            textRectPtr->left += extent.cx;
            if (textRectPtr->left >= textRectPtr->right) {
                break;
            }

            if (cursorOffset == nextTabOffset) {
                RECT cursorRect;
                cursorRect.left = textRectPtr->left;
                cursorRect.top = textRectPtr->top;
                cursorRect.right = textRectPtr->left + avgCharWidth;
                cursorRect.bottom = textRectPtr->top + lineHeight;
                FillRect(bitmapDeviceContext, &cursorRect, cursorBrush);
            }

            GetTextExtentPoint32W(
                bitmapDeviceContext,
                tabSpaces,
                config.tabWidth,
                &extent);
            DrawTextExW(
                bitmapDeviceContext,
                tabSpaces,
                config.tabWidth,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
            textRectPtr->left += extent.cx;
            if (textRectPtr->left >= textRectPtr->right) {
                break;
            }
        } else {
            GetTextExtentPoint32W(
                bitmapDeviceContext,
                currentText,
                cursorOffset,
                &extent);
            DrawTextExW(
                bitmapDeviceContext,
                currentText,
                cursorOffset,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
            textRectPtr->left += extent.cx;
            if (textRectPtr->left >= textRectPtr->right) {
                break;
            }

            GetTextExtentPoint32W(
                bitmapDeviceContext,
                &currentText[cursorOffset],
                1,
                &extent);
            RECT cursorRect;
            cursorRect.left = textRectPtr->left;
            cursorRect.top = textRectPtr->top;
            cursorRect.right = textRectPtr->left + extent.cx;
            cursorRect.bottom = textRectPtr->top + lineHeight;
            FillRect(bitmapDeviceContext, &cursorRect, cursorBrush);

            GetTextExtentPoint32W(
                bitmapDeviceContext,
                currentText + cursorOffset,
                nextTabOffset - cursorOffset,
                &extent);
            DrawTextExW(
                bitmapDeviceContext,
                currentText + cursorOffset,
                nextTabOffset - cursorOffset,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
            textRectPtr->left += extent.cx;
            if (textRectPtr->left >= textRectPtr->right) {
                break;
            }

            GetTextExtentPoint32W(
                bitmapDeviceContext,
                tabSpaces,
                config.tabWidth,
                &extent);
            DrawTextExW(
                bitmapDeviceContext,
                tabSpaces,
                config.tabWidth,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
            textRectPtr->left += extent.cx;
            if (textRectPtr->left >= textRectPtr->right) {
                break;
            }
        }

        currentText = nextTabPos + 1;
        nextTabOffset++;
        currentLength -= nextTabOffset;
        cursorOffset -= nextTabOffset;
        nextTabPos = wcschr_s(currentText, currentLength, L'\t');
    }

    if (!nextTabPos) {
        if (cursorOffset == currentLength) {
            GetTextExtentPoint32W(
                bitmapDeviceContext,
                currentText,
                currentLength,
                &extent);
            DrawTextExW(
                bitmapDeviceContext,
                currentText,
                currentLength,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
            textRectPtr->left += extent.cx;
            if (textRectPtr->left < textRectPtr->right) {
                RECT cursorRect;
                cursorRect.left = textRectPtr->left;
                cursorRect.top = textRectPtr->top;
                cursorRect.right = textRectPtr->left + avgCharWidth;
                cursorRect.bottom = textRectPtr->top + lineHeight;
                FillRect(bitmapDeviceContext, &cursorRect, cursorBrush);
            }
        } else if (cursorOffset >= 0) {
            GetTextExtentPoint32W(
                bitmapDeviceContext,
                currentText,
                cursorOffset,
                &extent);
            DrawTextExW(
                bitmapDeviceContext,
                currentText,
                cursorOffset,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
            textRectPtr->left += extent.cx;
            if (textRectPtr->left < textRectPtr->right) {
                GetTextExtentPoint32W(
                    bitmapDeviceContext,
                    currentText + cursorOffset,
                    1,
                    &extent);
                RECT cursorRect;
                cursorRect.left = textRectPtr->left;
                cursorRect.top = textRectPtr->top;
                cursorRect.right = textRectPtr->left + extent.cx;
                cursorRect.bottom = textRectPtr->top + lineHeight;
                FillRect(
                    bitmapDeviceContext,
                    &cursorRect,
                    cursorBrush);

                DrawTextExW(
                    bitmapDeviceContext,
                    currentText + cursorOffset,
                    currentLength - cursorOffset,
                    textRectPtr,
                    DT_NOCLIP | DT_NOPREFIX,
                    NULL);
            }
        } else {
            DrawTextExW(
                bitmapDeviceContext,
                currentText,
                currentLength,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
        }
    }
}

void Paint(Doc * docPtr) {
    if (bitmapWidth == 0 || bitmapHeight == 0) {
        docPtr->lastDrawRowCount = 0;
        return;
    }

    RECT clientRect;
    clientRect.left = 0;
    clientRect.top = 0;
    clientRect.right = bitmapWidth;
    clientRect.bottom = bitmapHeight;

    FillRect(bitmapDeviceContext, &clientRect, backgroundBrush);

    SetTextColor(bitmapDeviceContext, config.textColor);
    SetBkMode(bitmapDeviceContext, TRANSPARENT);

    ulong drawRowCount = bitmapHeight / lineHeight;
    if (drawRowCount == 0) {
        docPtr->lastDrawRowCount = 0;
        return;
    }
    drawRowCount--;

    if (drawRowCount != 0) {
        wchar_t workingFolderPathLine[MAX_PATH + 32];
        swprintf_s(workingFolderPathLine, MAX_PATH + 32, L"Working Folder: %s", workingFolderPath);

        RECT workingFolderRect;
        workingFolderRect.left = clientRect.left;
        workingFolderRect.top = clientRect.top;
        workingFolderRect.right = clientRect.right;
        workingFolderRect.bottom = clientRect.top + lineHeight;

        DrawTextExW(
            bitmapDeviceContext,
            workingFolderPathLine,
            -1,
            &workingFolderRect,
            DT_NOCLIP | DT_NOPREFIX,
            NULL);

        drawRowCount--;
    }

    if (drawRowCount != 0) {
        RECT fileRect;
        fileRect.left = clientRect.left;
        fileRect.top = clientRect.top + lineHeight;
        fileRect.right = clientRect.right;
        fileRect.bottom = fileRect.top + lineHeight;
        FillRect(bitmapDeviceContext, &fileRect, docTitleBackgroundBrush);

        wchar_t filePathLine[MAX_PATH + 32];
        if (filePath[0] == L'\0') {
            if (docPtr->modified) {
                wcscpy_s(filePathLine, MAX_PATH + 32, L"<UNNAMED> (modified)");
            } else {
                wcscpy_s(filePathLine, MAX_PATH + 32, L"<UNNAMED>");
            }
        } else {
            if (docPtr->modified) {
                swprintf_s(filePathLine, MAX_PATH + 32, L"%s (modified)", filePath);
            } else {
                wcscpy_s(filePathLine, MAX_PATH + 32, filePath);
            }
        }
        DrawTextExW(
            bitmapDeviceContext,
            filePathLine,
            -1,
            &fileRect,
            DT_NOCLIP | DT_NOPREFIX,
            NULL);

        drawRowCount--;
    }

    RECT textRect;
    textRect.left = clientRect.left;
    textRect.top = clientRect.top + 2 * lineHeight;
    textRect.right = clientRect.right;
    textRect.bottom = clientRect.bottom - lineHeight;

    if (docPtr->cursorRowIndex < docPtr->topDrawRowIndex) {
        docPtr->topDrawRowIndex = docPtr->cursorRowIndex;
    }
    ulong bottomDrawRowIndex = docPtr->topDrawRowIndex + drawRowCount;
    if (docPtr->cursorRowIndex >= bottomDrawRowIndex) {
        docPtr->topDrawRowIndex += docPtr->cursorRowIndex - bottomDrawRowIndex + 1;
        bottomDrawRowIndex = docPtr->cursorRowIndex + 1;
    } else if (docPtr->topDrawRowIndex != 0 && bottomDrawRowIndex > docPtr->lines.textBufferCount) {
        ulong diff = bottomDrawRowIndex - docPtr->lines.textBufferCount;
        if (diff > docPtr->topDrawRowIndex) {
            docPtr->topDrawRowIndex = 0;
        } else {
            docPtr->topDrawRowIndex -= diff;
        }
    }

    for (ulong i = docPtr->topDrawRowIndex; i != docPtr->lines.textBufferCount; i++) {
        if (textRect.bottom - textRect.top < lineHeight) {
            break;
        }

        DocLine * rowPtr = &docPtr->lines.textBuffer[i];
        if (i == docPtr->cursorRowIndex && currentMode == MODE_INSERT) {
            PaintCursorRow(&textRect, rowPtr->text, rowPtr->length, docPtr->cursorColIndex);
        } else {
            wchar_t * currentText = rowPtr->text;
            ushort currentLength = rowPtr->length;
            wchar_t * nextTabPos = wcschr_s(currentText, currentLength, L'\t');
            while (nextTabPos) {
                SIZE extent;
                ushort nextTabOffset = (ushort)(nextTabPos - currentText);

                GetTextExtentPoint32W(
                    bitmapDeviceContext,
                    currentText,
                    nextTabOffset,
                    &extent);
                DrawTextExW(
                    bitmapDeviceContext,
                    currentText,
                    nextTabOffset,
                    &textRect,
                    DT_NOCLIP | DT_NOPREFIX,
                    NULL);
                textRect.left += extent.cx;
                if (textRect.left >= textRect.right) {
                    break;
                }

                GetTextExtentPoint32W(
                    bitmapDeviceContext,
                    tabSpaces,
                    config.tabWidth,
                    &extent);
                DrawTextExW(
                    bitmapDeviceContext,
                    tabSpaces,
                    config.tabWidth,
                    &textRect,
                    DT_NOCLIP | DT_NOPREFIX,
                    NULL);
                textRect.left += extent.cx;
                if (textRect.left >= textRect.right) {
                    break;
                }

                nextTabOffset++;
                currentText = nextTabPos + 1;
                currentLength -= nextTabOffset;
                nextTabPos = wcschr_s(currentText, currentLength, L'\t');
            }

            if (!nextTabPos) {
                DrawTextExW(
                    bitmapDeviceContext,
                    currentText,
                    currentLength,
                    &textRect,
                    DT_NOCLIP | DT_NOPREFIX,
                    NULL);
            }
        }

        textRect.left = 0;
        textRect.top += lineHeight;
        docPtr->lastDrawRowCount++;
    }

    RECT statusRect;
    statusRect.left = clientRect.left;
    statusRect.top = clientRect.bottom - lineHeight;
    statusRect.right = clientRect.right;
    statusRect.bottom = clientRect.bottom;
    if (paintStatusMessage) {
        FillRect(bitmapDeviceContext, &statusRect, promptBackgroundBrush);
        SetTextColor(bitmapDeviceContext, config.promptTextColor);
        DrawTextExW(
            bitmapDeviceContext,
            statusInput,
            -1,
            &statusRect,
            DT_NOCLIP | DT_NOPREFIX,
            NULL);
    } else if (config.useVimMode) {
        FillRect(bitmapDeviceContext, &statusRect, statusBackgroundBrush);

        ulong rowPercent;
        if (docPtr->lines.textBufferCount > 1) {
            rowPercent = (100 * docPtr->cursorRowIndex) / (docPtr->lines.textBufferCount - 1);
        } else {
            rowPercent = 0;
        }

        DocLine * cursorRowPtr = &docPtr->lines.textBuffer[docPtr->cursorRowIndex];
        ulong cursorColDrawIndex = 0;
        for (ushort i = 0; i != docPtr->cursorColIndex; i++) {
            if (cursorRowPtr->text[i] == L'\t') {
                cursorColDrawIndex += config.tabWidth;
            } else {
                cursorColDrawIndex++;
            }
        }
        ulong cursorRowDrawLength = 0;
        for (ushort i = 0; i != cursorRowPtr->length; i++) {
            if (cursorRowPtr->text[i] == L'\t') {
                cursorRowDrawLength += config.tabWidth;
            } else {
                cursorRowDrawLength++;
            }
        }

        const wchar_t vimNormalText[] = L"[NORMAL]";
        const wchar_t vimInsertText[] = L"[INSERT]";
        const wchar_t * vimModeText;
        if (currentVimMode == MODE_VIM_NORMAL) {
            vimModeText = vimNormalText;
        } else {
            vimModeText = vimInsertText;
        }

        wchar_t statusLine[256];
        swprintf_s(
            statusLine,
            256,
            L"%s    Row: %lu/%lu (%lu %%)    Col: %hu/%hu (%lu/%lu)",
            vimModeText,
            docPtr->cursorRowIndex + 1,
            docPtr->lines.textBufferCount,
            rowPercent,
            docPtr->cursorColIndex + 1,
            cursorRowPtr->length,
            cursorColDrawIndex + 1,
            cursorRowDrawLength);

        DrawTextExW(
            bitmapDeviceContext,
            statusLine,
            -1,
            &statusRect,
            DT_NOCLIP | DT_NOPREFIX,
            NULL);
    } else if (currentMode == MODE_INSERT) {
        FillRect(bitmapDeviceContext, &statusRect, statusBackgroundBrush);

        ulong rowPercent;
        if (docPtr->lines.textBufferCount > 1) {
            rowPercent = (100 * docPtr->cursorRowIndex) / (docPtr->lines.textBufferCount - 1);
        } else {
            rowPercent = 0;
        }

        DocLine * cursorRowPtr = &docPtr->lines.textBuffer[docPtr->cursorRowIndex];
        ulong cursorColDrawIndex = 0;
        for (ushort i = 0; i != docPtr->cursorColIndex; i++) {
            if (cursorRowPtr->text[i] == L'\t') {
                cursorColDrawIndex += config.tabWidth;
            } else {
                cursorColDrawIndex++;
            }
        }
        ulong cursorRowDrawLength = 0;
        for (ushort i = 0; i != cursorRowPtr->length; i++) {
            if (cursorRowPtr->text[i] == L'\t') {
                cursorRowDrawLength += config.tabWidth;
            } else {
                cursorRowDrawLength++;
            }
        }

        wchar_t statusLine[128];
        swprintf_s(
            statusLine,
            128,
            L"Row: %lu/%lu (%lu %%)    Col: %hu/%hu (%lu/%lu)",
            docPtr->cursorRowIndex + 1,
            docPtr->lines.textBufferCount,
            rowPercent,
            docPtr->cursorColIndex + 1,
            cursorRowPtr->length,
            cursorColDrawIndex + 1,
            cursorRowDrawLength);

        DrawTextExW(
            bitmapDeviceContext,
            statusLine,
            -1,
            &statusRect,
            DT_NOCLIP | DT_NOPREFIX,
            NULL);
    } else if (currentMode == MODE_OPEN_FILE) {
        FillRect(bitmapDeviceContext, &statusRect, promptBackgroundBrush);
        SetTextColor(bitmapDeviceContext, config.promptTextColor);

        wchar_t statusLine[2 * MAX_PATH + 32] = L"Open: ";
        ushort prefixOffset = (ushort)wcslen(statusLine);
        wcsncpy_s(statusLine + prefixOffset, 2 * MAX_PATH, statusInput, statusInputLength);
        ushort actualCursorCol = statusCursorCol + prefixOffset;
        PaintCursorRow(&statusRect, statusLine, (ushort)wcslen(statusLine), actualCursorCol);
    } else if (currentMode == MODE_SAVE_FILE) {
        FillRect(bitmapDeviceContext, &statusRect, promptBackgroundBrush);
        SetTextColor(bitmapDeviceContext, config.promptTextColor);

        wchar_t statusLine[2 * MAX_PATH + 32] = L"Save: ";
        ushort prefixOffset = (ushort)wcslen(statusLine);
        wcsncpy_s(statusLine + prefixOffset, 2 * MAX_PATH, statusInput, statusInputLength);
        ushort actualCursorCol = statusCursorCol + prefixOffset;
        PaintCursorRow(&statusRect, statusLine, (ushort)wcslen(statusLine), actualCursorCol);
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_SIZE:
        {
            HDC deviceContext = GetDC(window);

            if (!bitmapDeviceContext) {
                bitmapDeviceContext = CreateCompatibleDC(deviceContext);

                int ppi = GetDeviceCaps(bitmapDeviceContext, LOGPIXELSY);
                int lfHeight = 0 - ((config.fontSize * ppi) / 72);
                HFONT font = CreateFontW(
                    lfHeight,
                    0,
                    0,
                    0,
                    FW_DONTCARE,
                    0,
                    0,
                    0,
                    ANSI_CHARSET,
                    OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE,
                    config.fontName);
                HFONT oldFont = (HFONT)SelectObject(bitmapDeviceContext, font);
                if (oldFont) {
                    DeleteObject(oldFont);
                }

                TEXTMETRICW fontMetrics;
                GetTextMetricsW(bitmapDeviceContext, &fontMetrics);
                lineHeight = fontMetrics.tmHeight;
                avgCharWidth = fontMetrics.tmAveCharWidth;
            }

            bitmapWidth = LOWORD(lparam);
            bitmapHeight = HIWORD(lparam);
            HBITMAP bitmap = CreateCompatibleBitmap(deviceContext, bitmapWidth, bitmapHeight);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(bitmapDeviceContext, bitmap);
            if (oldBitmap) {
                DeleteObject(oldBitmap);
            }

            Paint(onlyDoc);
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paintStruct;
            HDC deviceContext = BeginPaint(window, &paintStruct);
            long x = paintStruct.rcPaint.left;
            long y = paintStruct.rcPaint.top;
            long width = paintStruct.rcPaint.right - paintStruct.rcPaint.left;
            long height = paintStruct.rcPaint.bottom - paintStruct.rcPaint.top;

            bool result = BitBlt(
                deviceContext,
                x,
                y,
                width,
                height,
                bitmapDeviceContext,
                0,
                0,
                SRCCOPY);

            EndPaint(window, &paintStruct);
            return 0;
        }

        case WM_KEYDOWN:
        {
            if (config.useVimMode) {
                if (ProcessKeydownVim(wparam)) {
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                }
                return 0;
            }

            switch (wparam) {
                case VK_ESCAPE:
                {
                    currentMode = MODE_INSERT;
                    paintStatusMessage = FALSE;
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                    break;
                }

                case VK_LEFT:
                {
                    if (currentMode == MODE_INSERT) {
                        if (onlyDoc->cursorColIndex == 0) {
                            if (onlyDoc->cursorRowIndex != 0) {
                                onlyDoc->cursorRowIndex--;
                                onlyDoc->cursorColIndex = onlyDoc->lines.textBuffer[onlyDoc->cursorRowIndex].length;
                            }
                        } else {
                            onlyDoc->cursorColIndex--;
                        }
                        UpdateColDrawIndex(onlyDoc);
                        paintStatusMessage = FALSE;
                    } else {
                        if (statusCursorCol != 0) {
                            statusCursorCol--;
                        }
                    }
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                    break;
                }

                case VK_RIGHT:
                {
                    if (currentMode == MODE_INSERT) {
                        if (onlyDoc->cursorColIndex == onlyDoc->lines.textBuffer[onlyDoc->cursorRowIndex].length) {
                            if (onlyDoc->cursorRowIndex != onlyDoc->lines.textBufferCount - 1) {
                                onlyDoc->cursorRowIndex++;
                                onlyDoc->cursorColIndex = 0;
                            }
                        } else {
                            onlyDoc->cursorColIndex++;
                        }
                        UpdateColDrawIndex(onlyDoc);
                        paintStatusMessage = FALSE;
                    } else {
                        if (statusCursorCol != statusInputLength) {
                            statusCursorCol++;
                        }
                    }
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                    break;
                }

                case VK_UP:
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    if (onlyDoc->cursorRowIndex != 0) {
                        onlyDoc->cursorRowIndex--;
                        ApplyColDrawIndex(onlyDoc);
                    }
                    paintStatusMessage = FALSE;
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                    break;
                }

                case VK_DOWN:
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    if (onlyDoc->cursorRowIndex != onlyDoc->lines.textBufferCount - 1) {
                        onlyDoc->cursorRowIndex++;
                        ApplyColDrawIndex(onlyDoc);
                    }
                    paintStatusMessage = FALSE;
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                    break;
                }

                case VK_HOME:
                {
                    if (currentMode == MODE_INSERT) {
                        if (GetAsyncKeyState(VK_CONTROL) & SHRT_MIN) {
                            onlyDoc->cursorRowIndex = 0;
                        }
                        onlyDoc->cursorColIndex = 0;
                        onlyDoc->cursorColLastDrawIndex = 0;
                        paintStatusMessage = FALSE;
                    } else {
                        statusCursorCol = 0;
                    }
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                    break;
                }

                case VK_END:
                {
                    if (currentMode == MODE_INSERT) {
                        if (GetAsyncKeyState(VK_CONTROL) & SHRT_MIN) {
                            onlyDoc->cursorRowIndex = onlyDoc->lines.textBufferCount - 1;
                        }
                        onlyDoc->cursorColIndex = onlyDoc->lines.textBuffer[onlyDoc->cursorRowIndex].length;
                        UpdateColDrawIndex(onlyDoc);
                        paintStatusMessage = FALSE;
                    } else {
                        statusCursorCol = statusInputLength - 1;
                    }
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                    break;
                }

                case VK_PRIOR:
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    if (onlyDoc->lastDrawRowCount >= onlyDoc->cursorRowIndex) {
                        onlyDoc->cursorRowIndex = 0;
                        onlyDoc->topDrawRowIndex = 0;
                    } else {
                        onlyDoc->cursorRowIndex -= onlyDoc->lastDrawRowCount;
                        if (onlyDoc->lastDrawRowCount >= onlyDoc->topDrawRowIndex) {
                            onlyDoc->topDrawRowIndex = 0;
                        } else {
                            onlyDoc->topDrawRowIndex -= onlyDoc->lastDrawRowCount;
                        }
                    }
                    ApplyColDrawIndex(onlyDoc);
                    paintStatusMessage = FALSE;
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                    break;
                }

                case VK_NEXT:
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    onlyDoc->cursorRowIndex += onlyDoc->lastDrawRowCount;
                    if (onlyDoc->cursorRowIndex < onlyDoc->lines.textBufferCount) {
                        onlyDoc->topDrawRowIndex += onlyDoc->lastDrawRowCount;
                    } else {
                        onlyDoc->cursorRowIndex = onlyDoc->lines.textBufferCount - 1;
                        onlyDoc->topDrawRowIndex = 0;
                    }
                    ApplyColDrawIndex(onlyDoc);
                    paintStatusMessage = FALSE;
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                    break;
                }

                case VK_DELETE:
                {
                    if (currentMode == MODE_INSERT) {
                        DocLine * rowPtr = &onlyDoc->lines.textBuffer[onlyDoc->cursorRowIndex];
                        if (onlyDoc->cursorColIndex == rowPtr->length) {
                            if (onlyDoc->cursorRowIndex != onlyDoc->lines.textBufferCount - 1) {
                                DocLine * nextRowPtr = &onlyDoc->lines.textBuffer[onlyDoc->cursorRowIndex + 1];
                                ushort oldLength = rowPtr->length;
                                rowPtr->length += nextRowPtr->length;
                                if (rowPtr->length > rowPtr->capacity) {
                                    rowPtr->capacity += nextRowPtr->capacity;
                                    rowPtr->text = (wchar_t *)realloc(rowPtr->text, rowPtr->capacity * sizeof(wchar_t));
                                }

                                for (ushort i = 0; i != nextRowPtr->length; i++) {
                                    rowPtr->text[oldLength + i] = nextRowPtr->text[i];
                                }
                                free(nextRowPtr->text);

                                for (ulong i = onlyDoc->cursorRowIndex + 1; i != onlyDoc->lines.textBufferCount - 1; i++) {
                                    onlyDoc->lines.textBuffer[i] = onlyDoc->lines.textBuffer[i + 1];
                                }
                                onlyDoc->lines.textBufferCount--;

                                onlyDoc->modified = TRUE;
                            }
                        } else {
                            for (ushort i = onlyDoc->cursorColIndex; i != rowPtr->length - 1; i++) {
                                rowPtr->text[i] = rowPtr->text[i + 1];
                            }
                            rowPtr->length--;

                            onlyDoc->modified = TRUE;
                        }
                        UpdateColDrawIndex(onlyDoc);
                        paintStatusMessage = FALSE;
                    } else {
                        if (statusCursorCol != statusInputLength) {
                            for (ushort i = statusCursorCol; i != statusInputLength - 1; i++) {
                                statusInput[i] = statusInput[i + 1];
                            }
                            statusInputLength--;
                        }
                    }
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                    break;
                }
            }
            return 0;
        }

        case WM_CHAR:
        {
            wchar_t c = (wchar_t)wparam;

            if (config.useVimMode) {
                if (ProcessCharVim(onlyDoc, c)) {
                    Paint(onlyDoc);
                    InvalidateRect(window, NULL, FALSE);
                }
                return 0;
            }

            switch (c) {
                case L'\t':
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    if (config.expandTabs) {
                        InsertChars(onlyDoc, tabSpaces, config.tabWidth);
                    } else {
                        InsertChars(onlyDoc, &c, 1);
                    }
                    onlyDoc->modified = TRUE;
                    UpdateColDrawIndex(onlyDoc);
                    paintStatusMessage = FALSE;
                    break;
                }

                case L'\r':
                {
                    if (currentMode == MODE_INSERT) {
                        if (onlyDoc->lines.textBufferCount == onlyDoc->lines.textBufferCapacity) {
                            onlyDoc->lines.textBufferCapacity += TEXTBUFFER_GROW_COUNT;
                            onlyDoc->lines.textBuffer = (DocLine *)realloc(onlyDoc->lines.textBuffer, onlyDoc->lines.textBufferCapacity * sizeof(DocLine));
                        }
                        onlyDoc->lines.textBufferCount++;

                        for (ulong i = onlyDoc->lines.textBufferCount - 1; i != onlyDoc->cursorRowIndex + 1; i--) {
                            onlyDoc->lines.textBuffer[i] = onlyDoc->lines.textBuffer[i - 1];
                        }

                        DocLine * rowPtr = &onlyDoc->lines.textBuffer[onlyDoc->cursorRowIndex];
                        DocLine * newRowPtr = &onlyDoc->lines.textBuffer[onlyDoc->cursorRowIndex + 1];
                        newRowPtr->length = rowPtr->length - onlyDoc->cursorColIndex;
                        newRowPtr->capacity = INIT_ROW_CAPACITY;
                        while (newRowPtr->length > newRowPtr->capacity) {
                            newRowPtr->capacity *= 2;
                        }
                        newRowPtr->text = (wchar_t *)malloc(newRowPtr->capacity * sizeof(wchar_t));

                        for (ushort i = 0; i != newRowPtr->length; i++) {
                            newRowPtr->text[i] = rowPtr->text[onlyDoc->cursorColIndex + i];
                        }

                        rowPtr->length = onlyDoc->cursorColIndex;
                        onlyDoc->cursorColIndex = 0;
                        onlyDoc->cursorColLastDrawIndex = 0;
                        onlyDoc->cursorRowIndex++;

                        onlyDoc->modified = TRUE;
                        paintStatusMessage = FALSE;
                    } else if (currentMode == MODE_OPEN_FILE) {
                        Doc * newDoc = TryLoadFile();
                        if (newDoc) {
                            DestroyDoc(onlyDoc);
                            onlyDoc = newDoc;
                        }
                    } else if (currentMode == MODE_SAVE_FILE) {
                        TrySaveFile(&onlyDoc->lines);
                    }
                    break;
                }

                case L'\b':
                {
                    if (currentMode == MODE_INSERT) {
                        if (onlyDoc->cursorColIndex == 0) {
                            if (onlyDoc->cursorRowIndex != 0) {
                                DocLine * rowPtr = &onlyDoc->lines.textBuffer[onlyDoc->cursorRowIndex];
                                DocLine * prevRowPtr = &onlyDoc->lines.textBuffer[onlyDoc->cursorRowIndex - 1];
                                ushort prevOldLength = prevRowPtr->length;
                                prevRowPtr->length += rowPtr->length;
                                if (prevRowPtr->length > prevRowPtr->capacity) {
                                    prevRowPtr->capacity += rowPtr->capacity;
                                    prevRowPtr->text = (wchar_t *)realloc(prevRowPtr->text, prevRowPtr->capacity * sizeof(wchar_t));
                                }

                                for (ushort i = 0; i != rowPtr->length; i++) {
                                    prevRowPtr->text[prevOldLength + i] = rowPtr->text[i];
                                }
                                free(rowPtr->text);

                                for (ulong i = onlyDoc->cursorRowIndex; i != onlyDoc->lines.textBufferCount - 1; i++) {
                                    onlyDoc->lines.textBuffer[i] = onlyDoc->lines.textBuffer[i + 1];
                                }
                                onlyDoc->lines.textBufferCount--;

                                onlyDoc->cursorRowIndex--;
                                onlyDoc->cursorColIndex = prevRowPtr->length;

                                onlyDoc->modified = TRUE;
                            }
                        } else {
                            DocLine * rowPtr = &onlyDoc->lines.textBuffer[onlyDoc->cursorRowIndex];
                            for (ushort i = onlyDoc->cursorColIndex; i != rowPtr->length; i++) {
                                rowPtr->text[i - 1] = rowPtr->text[i];
                            }
                            onlyDoc->cursorColIndex--;
                            rowPtr->length--;

                            onlyDoc->modified = TRUE;
                        }
                        UpdateColDrawIndex(onlyDoc);
                        paintStatusMessage = FALSE;
                    } else {
                        if (statusCursorCol != 0) {
                            for (ushort i = statusCursorCol; i != statusInputLength; i++) {
                                statusInput[i - 1] = statusInput[i];
                            }
                            statusCursorCol--;
                            statusInputLength--;
                        }
                    }
                    break;
                }

                case 0xf: // Ctrl + O
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    paintStatusMessage = FALSE;
                    currentMode = MODE_OPEN_FILE;
                    statusInputLength = 0;
                    statusCursorCol = 0;
                    break;
                }

                case 0x13: // Ctrl + S
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    paintStatusMessage = FALSE;
                    currentMode = MODE_SAVE_FILE;
                    wcscpy_s(statusInput, MAX_PATH, filePath);
                    statusInputLength = (ushort)wcslen(statusInput);
                    statusCursorCol = statusInputLength;
                    break;
                }

                default:
                {
                    if (iswcntrl(c)) {
                        break;
                    }

                    if (currentMode == MODE_INSERT) {
                        InsertChars(onlyDoc, &c, 1);
                        onlyDoc->modified = TRUE;
                        UpdateColDrawIndex(onlyDoc);
                        paintStatusMessage = FALSE;
                    } else {
                        if (statusInputLength != 2 * MAX_PATH) {
                            for (ushort i = statusInputLength; i != statusCursorCol; i--) {
                                statusInput[i] = statusInput[i - 1];
                            }
                            statusInputLength++;
                            statusInput[statusCursorCol++] = c;
                        }
                    }
                    break;
                }
            }

            Paint(onlyDoc);
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }

        case WM_CLOSE:
        {
            DestroyWindow(window);
            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }

        default:
        {
            return DefWindowProcW(window, message, wparam, lparam);
        }
    }
}

static bool DocStreamCallback(void * rawDoc, wchar_t * wc, void ** stopStatus) {
    Doc * doc = (Doc *)rawDoc;
    if (doc->cursorColIndex == doc->lines.textBuffer[doc->cursorRowIndex].length) {
        if (doc->cursorRowIndex == doc->lines.textBufferCount - 1) {
            return false;
        }

        *wc = L'\n';
        doc->cursorRowIndex++;
        doc->cursorColIndex = 0;
    } else {
        *wc = doc->lines.textBuffer[doc->cursorRowIndex].text[doc->cursorColIndex];
        doc->cursorColIndex++;
    }
    return true;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, wchar_t * commandLine, int showCommand) {
    ConfigInit(&config);

    wchar_t * appDataFolderPath;
    SHGetKnownFolderPath(
        FOLDERID_RoamingAppData,
        KF_FLAG_DEFAULT,
        NULL,
        &appDataFolderPath);
    wchar_t configFilePath[MAX_PATH];
    swprintf_s(configFilePath, MAX_PATH, L"%s\\MKedit\\Config.cfg", appDataFolderPath);
    CoTaskMemFree(appDataFolderPath);

    Doc * configDoc = TryLoadFilePath(configFilePath, true);
    if (configDoc) {
        bool loadConfStatus;
        MkConfGenLoadError * loadConfErrors;
        ulong loadConfErrorCount;
        ConfigLoad(
            &config,
            configDoc,
            DocStreamCallback,
            (void **)&loadConfStatus,
            &loadConfErrors,
            &loadConfErrorCount);
        if (loadConfErrorCount != 0) {
            free(loadConfErrors);
        }
        DestroyDoc(configDoc);
    }

    tabSpaces = (wchar_t *)malloc(config.tabWidth * sizeof(wchar_t));
    for (unsigned i = 0; i != config.tabWidth; i++) {
        tabSpaces[i] = L' ';
    }

    textBrush = CreateSolidBrush(config.textColor);
    backgroundBrush = CreateSolidBrush(config.backgroundColor);
    cursorBrush = CreateSolidBrush(config.cursorColor);
    statusBackgroundBrush = CreateSolidBrush(config.statusBackgroundColor);
    docTitleBackgroundBrush = CreateSolidBrush(config.docTitleBackgroundColor);
    promptTextBrush = CreateSolidBrush(config.promptTextColor);
    promptBackgroundBrush = CreateSolidBrush(config.promptBackgroundColor);

    onlyDoc = CreateEmptyDoc();

    GetCurrentDirectoryW(MAX_PATH, workingFolderPath);

    const wchar_t windowClassName[] = L"MKedit";

    WNDCLASSEXW windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = windowClassName;
    RegisterClassExW(&windowClass);

    HWND window = CreateWindowExW(
        0,
        windowClassName,
        windowClassName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        instance,
        NULL);
    ShowWindow(window, showCommand);

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}