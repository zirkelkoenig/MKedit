#include <Windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <wctype.h>

typedef int bool;
typedef unsigned char byte;
typedef unsigned short ushort;
typedef unsigned long ulong;

const ulong maxRows = ULONG_MAX;
const ushort maxRowLength = USHRT_MAX;

const ulong textColor = RGB(220, 220, 220);
const ulong backgroundColor = RGB(30, 30, 30);
const ulong cursorBackgroundColor = RGB(125, 125, 125);
const ulong statusBackgroundColor = RGB(46, 46, 46);
const ulong fileBackgroundColor = RGB(61, 61, 61);
const ulong promptTextColor = RGB(255, 255, 255);
const ulong promptBackgroundColor = RGB(134, 27, 45);

const int fontSize = 10;
const wchar_t fontName[] = L"Consolas";

#define INIT_ROW_CAPACITY 4
#define TEXTBUFFER_GROW_COUNT 16

HBRUSH textBrush;
HBRUSH backgroundBrush;
HBRUSH cursorBackgroundBrush;
HBRUSH statusBackgroundBrush;
HBRUSH fileBackgroundBrush;
HBRUSH promptTextBrush;
HBRUSH promptBackgroundBrush;

unsigned tabWidth = 4;
bool expandTabs = TRUE;
wchar_t * tabSpaces = L"        ";

bool fontSet;
long lineHeight;
long avgCharWidth;

typedef struct TextRow {
    ushort length;
    ushort capacity;
    wchar_t * text;
} TextRow;

ulong textBufferCount;
ulong textBufferCapacity;
TextRow * textBuffer;

ulong cursorRowIndex;
ushort cursorColIndex;
ulong cursorColLastDrawIndex;

ulong topDrawRowIndex;
ulong lastDrawRowCount;

typedef enum Mode {
    MODE_INSERT,
    MODE_OPEN_FILE,
    MODE_SAVE_FILE,
} Mode;

wchar_t workingFolderPath[MAX_PATH];
wchar_t filePath[MAX_PATH];
bool modified;
Mode currentMode;
ushort statusCursorCol;
ushort statusInputLength;
wchar_t statusInput[2 * MAX_PATH];
bool paintStatusMessage;

void TrySaveFile() {
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
        statusInputLength = wcslen(statusInput);
        paintStatusMessage = TRUE;
        currentMode = MODE_INSERT;
        return;
    }

    wcscpy_s(filePath, MAX_PATH, statusInput);
    modified = FALSE;
    currentMode = MODE_INSERT;

    TextRow * rowPtr = &textBuffer[0];
    byte output[4];
    ushort outputCount;
    wchar_t c;
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

    for (ulong i = 1; i != textBufferCount; i++) {
        rowPtr = &textBuffer[i];
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

void AppendChar(TextRow * rowPtr, wchar_t c) {
    if (rowPtr->length == rowPtr->capacity) {
        rowPtr->capacity *= 2;
        rowPtr->text = realloc(rowPtr->text, rowPtr->capacity);
    }
    rowPtr->text[rowPtr->length++] = c;
}

typedef enum Utf8State {
    UTF8_START,
    UTF8_END_OK,
    UTF8_END_ERROR,
    UTF8_READ_1,
    UTF8_READ_2,
    UTF8_READ_3,
    UTF8_READ_ERROR,
} Utf8State;

void TryLoadFile() {
    statusInput[statusInputLength] = L'\0';
    HANDLE file = CreateFileW(
        statusInput,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        wcscpy_s(statusInput, 2 * MAX_PATH, L"File could not be opened!");
        statusInputLength = wcslen(statusInput);
        paintStatusMessage = TRUE;
        currentMode = MODE_INSERT;
        return;
    }

    wcscpy_s(filePath, MAX_PATH, statusInput);

    for (ulong i = 0; i != textBufferCount; i++) {
        free(textBuffer[i].text);
    }
    textBufferCount = 0;

    cursorRowIndex = 0;
    cursorColIndex = 0;
    cursorColLastDrawIndex = 0;
    topDrawRowIndex = 0;
    modified = FALSE;
    currentMode = MODE_INSERT;

    textBufferCount = 1;
    TextRow * rowPtr = &textBuffer[0];
    rowPtr->length = 0;
    rowPtr->capacity = INIT_ROW_CAPACITY;
    rowPtr->text = malloc(INIT_ROW_CAPACITY * sizeof(wchar_t));

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
                    if (textBufferCount == textBufferCapacity) {
                        textBufferCapacity += TEXTBUFFER_GROW_COUNT;
                        textBuffer = realloc(textBuffer, textBufferCapacity * sizeof(TextRow));
                    }
                    rowPtr = &textBuffer[textBufferCount++];
                    rowPtr->length = 0;
                    rowPtr->capacity = INIT_ROW_CAPACITY;
                    rowPtr->text = malloc(INIT_ROW_CAPACITY * sizeof(wchar_t));
                    lastWasNewline = c == L'\r';
                } else {
                    AppendChar(rowPtr, c);
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

    CloseHandle(file);
}

void InsertChars(wchar_t * wcs, ushort count) {
    TextRow * rowPtr = &textBuffer[cursorRowIndex];
    
    rowPtr->length += count;
    bool grow = FALSE;
    while (rowPtr->length > rowPtr->capacity) {
        rowPtr->capacity *= 2;
        grow = TRUE;
    }
    if (grow) {
        rowPtr->text = realloc(rowPtr->text, rowPtr->capacity * sizeof(wchar_t));
    }

    ushort newCursorColIndex = cursorColIndex + count;
    for (ushort i = rowPtr->length - 1; i >= newCursorColIndex; i--) {
        rowPtr->text[i] = rowPtr->text[i - count];
    }
    for (ushort i = 0; i != count; i++) {
        rowPtr->text[cursorColIndex + i] = wcs[i];
    }
    cursorColIndex = newCursorColIndex;
}

void UpdateColDrawIndex() {
    wchar_t * text = textBuffer[cursorRowIndex].text;
    cursorColLastDrawIndex = 0;
    for (ushort i = 0; i != cursorColIndex; i++) {
        if (text[i] == L'\t') {
            cursorColLastDrawIndex += tabWidth;
        } else {
            cursorColLastDrawIndex++;
        }
    }
}

void ApplyColDrawIndex() {
    TextRow * rowPtr = &textBuffer[cursorRowIndex];
    ulong cursorColDrawIndex = 0;
    for (cursorColIndex = 0; cursorColIndex != rowPtr->length; cursorColIndex++) {
        if (cursorColDrawIndex >= cursorColLastDrawIndex) {
            if (cursorColDrawIndex > cursorColLastDrawIndex) {
                cursorColIndex--;
            }
            break;
        }

        if (rowPtr->text[cursorColIndex] == L'\t') {
            cursorColDrawIndex += tabWidth;
        } else {
            cursorColDrawIndex++;
        }
    }
}

wchar_t * wcschr_s(wchar_t * wcs, ushort length, wchar_t c) {
    wchar_t * end = wcs + length;
    while (wcs != end && wcs != L'\0') {
        if (*wcs == c) {
            return wcs;
        }
        wcs++;
    }
    return NULL;
}

void PaintCursorRow(
    HDC deviceContext,
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
        ushort nextTabOffset = nextTabPos - currentText;
        if (cursorOffset < 0 || cursorOffset >= nextTabOffset) {
            GetTextExtentPoint32W(
                deviceContext,
                currentText,
                nextTabOffset,
                &extent);
            DrawTextExW(
                deviceContext,
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
                cursorRect.right = textRectPtr->left + avgCharWidth;
                cursorRect.top = textRectPtr->top;
                cursorRect.bottom = textRectPtr->top + lineHeight;
                FillRect(deviceContext, &cursorRect, cursorBackgroundBrush);
            }

            GetTextExtentPoint32W(
                deviceContext,
                tabSpaces,
                tabWidth,
                &extent);
            DrawTextExW(
                deviceContext,
                tabSpaces,
                tabWidth,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
            textRectPtr->left += extent.cx;
            if (textRectPtr->left >= textRectPtr->right) {
                break;
            }
        } else {
            GetTextExtentPoint32W(
                deviceContext,
                currentText,
                cursorOffset,
                &extent);
            DrawTextExW(
                deviceContext,
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
                deviceContext,
                &currentText[cursorOffset],
                1,
                &extent);
            RECT cursorRect;
            cursorRect.left = textRectPtr->left;
            cursorRect.right = textRectPtr->left + extent.cx;
            cursorRect.top = textRectPtr->top;
            cursorRect.bottom = textRectPtr->top + lineHeight;
            FillRect(deviceContext, &cursorRect, cursorBackgroundBrush);

            GetTextExtentPoint32W(
                deviceContext,
                currentText + cursorOffset,
                nextTabOffset - cursorOffset,
                &extent);
            DrawTextExW(
                deviceContext,
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
                deviceContext,
                tabSpaces,
                tabWidth,
                &extent);
            DrawTextExW(
                deviceContext,
                tabSpaces,
                tabWidth,
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
                deviceContext,
                currentText,
                currentLength,
                &extent);
            DrawTextExW(
                deviceContext,
                currentText,
                currentLength,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
            textRectPtr->left += extent.cx;
            if (textRectPtr->left < textRectPtr->right) {
                RECT cursorRect;
                cursorRect.left = textRectPtr->left;
                cursorRect.right = textRectPtr->left + avgCharWidth;
                cursorRect.top = textRectPtr->top;
                cursorRect.bottom = textRectPtr->top + lineHeight;
                FillRect(
                    deviceContext,
                    &cursorRect,
                    cursorBackgroundBrush);
            }
        } else if (cursorOffset >= 0) {
            GetTextExtentPoint32W(
                deviceContext,
                currentText,
                cursorOffset,
                &extent);
            DrawTextExW(
                deviceContext,
                currentText,
                cursorOffset,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
            textRectPtr->left += extent.cx;
            if (textRectPtr->left < textRectPtr->right) {
                GetTextExtentPoint32W(
                    deviceContext,
                    currentText + cursorOffset,
                    1,
                    &extent);
                RECT cursorRect;
                cursorRect.left = textRectPtr->left;
                cursorRect.right = textRectPtr->left + extent.cx;
                cursorRect.top = textRectPtr->top;
                cursorRect.bottom = textRectPtr->top + lineHeight;
                FillRect(
                    deviceContext,
                    &cursorRect,
                    cursorBackgroundBrush);

                DrawTextExW(
                    deviceContext,
                    currentText + cursorOffset,
                    currentLength - cursorOffset,
                    textRectPtr,
                    DT_NOCLIP | DT_NOPREFIX,
                    NULL);
            }
        } else {
            DrawTextExW(
                deviceContext,
                currentText,
                currentLength,
                textRectPtr,
                DT_NOCLIP | DT_NOPREFIX,
                NULL);
        }
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_PAINT:
        {
            RECT clientRect;

            GetClientRect(window, &clientRect);
            long clientWidth = clientRect.right - clientRect.left;
            long clientHeight = clientRect.bottom - clientRect.top;
            if (clientHeight <= 0 || clientWidth <= 0) {
                lastDrawRowCount = 0;
                return 0;
            }

            PAINTSTRUCT paintStruct;
            HDC deviceContext = BeginPaint(window, &paintStruct);
            FillRect(deviceContext, &clientRect, backgroundBrush);

            if (!fontSet) {
                int ppi = GetDeviceCaps(deviceContext, LOGPIXELSY);
                int lfHeight = 0 - ((fontSize * ppi) / 72);
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
                    fontName);
                HFONT oldFont = SelectObject(deviceContext, font);
                if (oldFont) {
                    DeleteObject(oldFont);
                }

                TEXTMETRICW fontMetrics;
                GetTextMetricsW(deviceContext, &fontMetrics);
                lineHeight = fontMetrics.tmHeight;
                avgCharWidth = fontMetrics.tmAveCharWidth;

                fontSet = TRUE;
            }

            SetTextColor(deviceContext, textColor);
            SetBkMode(deviceContext, TRANSPARENT);

            ulong drawRowCount = clientHeight / lineHeight;
            if (drawRowCount == 0) {
                FillRect(deviceContext, &clientRect, backgroundBrush);
                EndPaint(window, &paintStruct);

                lastDrawRowCount = 0;
                return 0;
            }
            drawRowCount--;

            if (drawRowCount != 0) {
                wchar_t workingFolderPathLine[MAX_PATH + 32];
                swprintf_s(workingFolderPathLine, MAX_PATH + 32, L"Working Folder: %s", workingFolderPath);

                RECT workingFolderRect;
                workingFolderRect.left = clientRect.left;
                workingFolderRect.right = clientRect.right;
                workingFolderRect.top = clientRect.top;
                workingFolderRect.bottom = clientRect.top + lineHeight;

                DrawTextExW(
                    deviceContext,
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
                fileRect.right = clientRect.right;
                fileRect.top = clientRect.top + lineHeight;
                fileRect.bottom = fileRect.top + lineHeight;
                FillRect(deviceContext, &fileRect, fileBackgroundBrush);

                wchar_t filePathLine[MAX_PATH + 32];
                if (filePath[0] == L'\0') {
                    if (modified) {
                        wcscpy_s(filePathLine, MAX_PATH + 32, L"<UNNAMED> (modified)");
                    } else {
                        wcscpy_s(filePathLine, MAX_PATH + 32, L"<UNNAMED>");
                    }
                } else {
                    if (modified) {
                        swprintf_s(filePathLine, MAX_PATH + 32, L"%s (modified)", filePath);
                    } else {
                        wcscpy_s(filePathLine, MAX_PATH + 32, filePath);
                    }
                }
                DrawTextExW(
                    deviceContext,
                    filePathLine,
                    -1,
                    &fileRect,
                    DT_NOCLIP | DT_NOPREFIX,
                    NULL);

                drawRowCount--;
            }

            RECT textRect;
            textRect.left = clientRect.left;
            textRect.right = clientRect.right;
            textRect.top = clientRect.top + 2 * lineHeight;
            textRect.bottom = clientRect.bottom - lineHeight;

            if (cursorRowIndex < topDrawRowIndex) {
                topDrawRowIndex = cursorRowIndex;
            }
            ulong bottomDrawRowIndex = topDrawRowIndex + drawRowCount;
            if (cursorRowIndex >= bottomDrawRowIndex) {
                topDrawRowIndex += cursorRowIndex - bottomDrawRowIndex + 1;
                bottomDrawRowIndex = cursorRowIndex + 1;
            } else if (topDrawRowIndex != 0 && bottomDrawRowIndex > textBufferCount) {
                ulong diff = bottomDrawRowIndex - textBufferCount;
                if (diff > topDrawRowIndex) {
                    topDrawRowIndex = 0;
                } else {
                    topDrawRowIndex -= diff;
                }
            }

            for (ulong i = topDrawRowIndex; i != textBufferCount; i++) {
                if (textRect.bottom - textRect.top < lineHeight) {
                    break;
                }

                TextRow * rowPtr = &textBuffer[i];
                if (i == cursorRowIndex && currentMode == MODE_INSERT) {
                    PaintCursorRow(deviceContext, &textRect, rowPtr->text, rowPtr->length, cursorColIndex);
                } else {
                    wchar_t * currentText = rowPtr->text;
                    ushort currentLength = rowPtr->length;
                    wchar_t * nextTabPos = wcschr_s(currentText, currentLength, L'\t');
                    while (nextTabPos) {
                        SIZE extent;
                        ushort nextTabOffset = nextTabPos - currentText;

                        GetTextExtentPoint32W(
                            deviceContext,
                            currentText,
                            nextTabOffset,
                            &extent);
                        DrawTextExW(
                            deviceContext,
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
                            deviceContext,
                            tabSpaces,
                            tabWidth,
                            &extent);
                        DrawTextExW(
                            deviceContext,
                            tabSpaces,
                            tabWidth,
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
                            deviceContext,
                            currentText,
                            currentLength,
                            &textRect,
                            DT_NOCLIP | DT_NOPREFIX,
                            NULL);
                    }
                }

                textRect.left = 0;
                textRect.top += lineHeight;
                lastDrawRowCount++;
            }

            RECT statusRect;
            statusRect.left = clientRect.left;
            statusRect.right = clientRect.right;
            statusRect.top = clientRect.bottom - lineHeight;
            statusRect.bottom = clientRect.bottom;
            if (paintStatusMessage) {
                FillRect(deviceContext, &statusRect, promptBackgroundBrush);
                SetTextColor(deviceContext, promptTextColor);
                DrawTextExW(
                    deviceContext,
                    statusInput,
                    -1,
                    &statusRect,
                    DT_NOCLIP | DT_NOPREFIX,
                    NULL);
            } else if (currentMode == MODE_INSERT) {
                FillRect(deviceContext, &statusRect, statusBackgroundBrush);

                ulong rowPercent;
                if (textBufferCount > 1) {
                    rowPercent = (100 * cursorRowIndex) / (textBufferCount - 1);
                } else {
                    rowPercent = 0;
                }

                TextRow * cursorRowPtr = &textBuffer[cursorRowIndex];
                ulong cursorColDrawIndex = 0;
                for (ushort i = 0; i != cursorColIndex; i++) {
                    if (cursorRowPtr->text[i] == L'\t') {
                        cursorColDrawIndex += tabWidth;
                    } else {
                        cursorColDrawIndex++;
                    }
                }
                ulong cursorRowDrawLength = 0;
                for (ushort i = 0; i != cursorRowPtr->length; i++) {
                    if (cursorRowPtr->text[i] == L'\t') {
                        cursorRowDrawLength += tabWidth;
                    } else {
                        cursorRowDrawLength++;
                    }
                }

                wchar_t statusLine[128];
                swprintf_s(
                    statusLine,
                    128,
                    L"Row: %lu/%lu (%lu %%)    Col: %hu/%hu (%lu/%lu)",
                    cursorRowIndex + 1,
                    textBufferCount,
                    rowPercent,
                    cursorColIndex + 1,
                    cursorRowPtr->length,
                    cursorColDrawIndex + 1,
                    cursorRowDrawLength);

                DrawTextExW(
                    deviceContext,
                    statusLine,
                    -1,
                    &statusRect,
                    DT_NOCLIP | DT_NOPREFIX,
                    NULL);
            } else if (currentMode == MODE_OPEN_FILE) {
                FillRect(deviceContext, &statusRect, promptBackgroundBrush);
                SetTextColor(deviceContext, promptTextColor);

                wchar_t statusLine[2 * MAX_PATH + 32] = L"Open: ";
                ushort prefixOffset = wcslen(statusLine);
                wcsncpy_s(statusLine + prefixOffset, 2 * MAX_PATH, statusInput, statusInputLength);
                ushort actualCursorCol = statusCursorCol + prefixOffset;
                PaintCursorRow(deviceContext, &statusRect, statusLine, wcslen(statusLine), actualCursorCol);
            } else if (currentMode == MODE_SAVE_FILE) {
                FillRect(deviceContext, &statusRect, promptBackgroundBrush);
                SetTextColor(deviceContext, promptTextColor);

                wchar_t statusLine[2 * MAX_PATH + 32] = L"Save: ";
                ushort prefixOffset = wcslen(statusLine);
                wcsncpy_s(statusLine + prefixOffset, 2 * MAX_PATH, statusInput, statusInputLength);
                ushort actualCursorCol = statusCursorCol + prefixOffset;
                PaintCursorRow(deviceContext, &statusRect, statusLine, wcslen(statusLine), actualCursorCol);
            }

            EndPaint(window, &paintStruct);
            return 0;
        }

        case WM_KEYDOWN:
        {
            switch (wparam) {
                case VK_ESCAPE:
                {
                    currentMode = MODE_INSERT;
                    paintStatusMessage = FALSE;
                    InvalidateRect(window, NULL, TRUE);
                    break;
                }

                case VK_LEFT:
                {
                    if (currentMode == MODE_INSERT) {
                        if (cursorColIndex == 0) {
                            if (cursorRowIndex != 0) {
                                cursorRowIndex--;
                                cursorColIndex = textBuffer[cursorRowIndex].length;
                            }
                        } else {
                            cursorColIndex--;
                        }
                        UpdateColDrawIndex();
                        paintStatusMessage = FALSE;
                    } else {
                        if (statusCursorCol != 0) {
                            statusCursorCol--;
                        }
                    }
                    InvalidateRect(window, NULL, TRUE);
                    break;
                }

                case VK_RIGHT:
                {
                    if (currentMode == MODE_INSERT) {
                        if (cursorColIndex == textBuffer[cursorRowIndex].length) {
                            if (cursorRowIndex != textBufferCount - 1) {
                                cursorRowIndex++;
                                cursorColIndex = 0;
                            }
                        } else {
                            cursorColIndex++;
                        }
                        UpdateColDrawIndex();
                        paintStatusMessage = FALSE;
                    } else {
                        if (statusCursorCol != statusInputLength) {
                            statusCursorCol++;
                        }
                    }
                    InvalidateRect(window, NULL, TRUE);
                    break;
                }

                case VK_UP:
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    if (cursorRowIndex != 0) {
                        cursorRowIndex--;
                        ApplyColDrawIndex();
                    }
                    paintStatusMessage = FALSE;
                    InvalidateRect(window, NULL, TRUE);
                    break;
                }

                case VK_DOWN:
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    if (cursorRowIndex != textBufferCount - 1) {
                        cursorRowIndex++;
                        ApplyColDrawIndex();
                    }
                    paintStatusMessage = FALSE;
                    InvalidateRect(window, NULL, TRUE);
                    break;
                }

                case VK_HOME:
                {
                    if (currentMode == MODE_INSERT) {
                        if (GetAsyncKeyState(VK_CONTROL) & SHRT_MIN) {
                            cursorRowIndex = 0;
                        }
                        cursorColIndex = 0;
                        cursorColLastDrawIndex = 0;
                        paintStatusMessage = FALSE;
                    } else {
                        statusCursorCol = 0;
                    }
                    InvalidateRect(window, NULL, TRUE);
                    break;
                }

                case VK_END:
                {
                    if (currentMode == MODE_INSERT) {
                        if (GetAsyncKeyState(VK_CONTROL) & SHRT_MIN) {
                            cursorRowIndex = textBufferCount - 1;
                        }
                        cursorColIndex = textBuffer[cursorRowIndex].length;
                        UpdateColDrawIndex();
                        paintStatusMessage = FALSE;
                    } else {
                        statusCursorCol = statusInputLength - 1;
                    }
                    InvalidateRect(window, NULL, TRUE);
                    break;
                }

                case VK_PRIOR:
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    if (lastDrawRowCount >= cursorRowIndex) {
                        cursorRowIndex = 0;
                        topDrawRowIndex = 0;
                    } else {
                        cursorRowIndex -= lastDrawRowCount;
                        if (lastDrawRowCount >= topDrawRowIndex) {
                            topDrawRowIndex = 0;
                        } else {
                            topDrawRowIndex -= lastDrawRowCount;
                        }
                    }
                    ApplyColDrawIndex();
                    paintStatusMessage = FALSE;
                    InvalidateRect(window, NULL, TRUE);
                    break;
                }

                case VK_NEXT:
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    cursorRowIndex += lastDrawRowCount;
                    if (cursorRowIndex < textBufferCount) {
                        topDrawRowIndex += lastDrawRowCount;
                    } else {
                        cursorRowIndex = textBufferCount - 1;
                        topDrawRowIndex = 0;
                    }
                    ApplyColDrawIndex();
                    paintStatusMessage = FALSE;
                    InvalidateRect(window, NULL, TRUE);
                    break;
                }

                case VK_DELETE:
                {
                    if (currentMode == MODE_INSERT) {
                        TextRow * rowPtr = &textBuffer[cursorRowIndex];
                        if (cursorColIndex == rowPtr->length) {
                            if (cursorRowIndex != textBufferCount - 1) {
                                TextRow * nextRowPtr = &textBuffer[cursorRowIndex + 1];
                                ushort oldLength = rowPtr->length;
                                rowPtr->length += nextRowPtr->length;
                                if (rowPtr->length > rowPtr->capacity) {
                                    rowPtr->capacity += nextRowPtr->capacity;
                                    rowPtr->text = realloc(rowPtr->text, rowPtr->capacity * sizeof(wchar_t));
                                }

                                for (ushort i = 0; i != nextRowPtr->length; i++) {
                                    rowPtr->text[oldLength + i] = nextRowPtr->text[i];
                                }
                                free(nextRowPtr->text);

                                for (ulong i = cursorRowIndex + 1; i != textBufferCount - 1; i++) {
                                    textBuffer[i] = textBuffer[i + 1];
                                }
                                textBufferCount--;

                                modified = TRUE;
                            }
                        } else {
                            for (ushort i = cursorColIndex; i != rowPtr->length - 1; i++) {
                                rowPtr->text[i] = rowPtr->text[i + 1];
                            }
                            rowPtr->length--;

                            modified = TRUE;
                        }
                        UpdateColDrawIndex();
                        paintStatusMessage = FALSE;
                    } else {
                        if (statusCursorCol != statusInputLength) {
                            for (ushort i = statusCursorCol; i != statusInputLength - 1; i++) {
                                statusInput[i] = statusInput[i + 1];
                            }
                            statusInputLength--;
                        }
                    }
                    InvalidateRect(window, NULL, TRUE);
                    break;
                }
            }
            return 0;
        }

        case WM_CHAR:
        {
            wchar_t c = (wchar_t)wparam;
            switch (c) {
                case L'\t':
                {
                    if (currentMode != MODE_INSERT) {
                        break;
                    }

                    if (expandTabs) {
                        InsertChars(tabSpaces, tabWidth);
                    } else {
                        InsertChars(&c, 1);
                    }
                    modified = TRUE;
                    UpdateColDrawIndex();
                    paintStatusMessage = FALSE;
                    break;
                }

                case L'\r':
                {
                    if (currentMode == MODE_INSERT) {
                        if (textBufferCount == textBufferCapacity) {
                            textBufferCapacity += TEXTBUFFER_GROW_COUNT;
                            textBuffer = realloc(textBuffer, textBufferCapacity * sizeof(TextRow));
                        }
                        textBufferCount++;

                        for (ulong i = textBufferCount - 1; i != cursorRowIndex + 1; i--) {
                            textBuffer[i] = textBuffer[i - 1];
                        }

                        TextRow * rowPtr = &textBuffer[cursorRowIndex];
                        TextRow * newRowPtr = &textBuffer[cursorRowIndex + 1];
                        newRowPtr->length = rowPtr->length - cursorColIndex;
                        newRowPtr->capacity = INIT_ROW_CAPACITY;
                        while (newRowPtr->length > newRowPtr->capacity) {
                            newRowPtr->capacity *= 2;
                        }
                        newRowPtr->text = malloc(newRowPtr->capacity * sizeof(wchar_t));

                        for (ushort i = 0; i != newRowPtr->length; i++) {
                            newRowPtr->text[i] = rowPtr->text[cursorColIndex + i];
                        }

                        rowPtr->length = cursorColIndex;
                        cursorColIndex = 0;
                        cursorColLastDrawIndex = 0;
                        cursorRowIndex++;

                        modified = TRUE;
                        paintStatusMessage = FALSE;
                    } else if (currentMode == MODE_OPEN_FILE) {
                        TryLoadFile();
                    } else if (currentMode == MODE_SAVE_FILE) {
                        TrySaveFile();
                    }
                    break;
                }

                case L'\b':
                {
                    if (currentMode == MODE_INSERT) {
                        if (cursorColIndex == 0) {
                            if (cursorRowIndex != 0) {
                                TextRow * rowPtr = &textBuffer[cursorRowIndex];
                                TextRow * prevRowPtr = &textBuffer[cursorRowIndex - 1];
                                ushort prevOldLength = prevRowPtr->length;
                                prevRowPtr->length += rowPtr->length;
                                if (prevRowPtr->length > prevRowPtr->capacity) {
                                    prevRowPtr->capacity += rowPtr->capacity;
                                    prevRowPtr->text = realloc(prevRowPtr->text, prevRowPtr->capacity * sizeof(wchar_t));
                                }

                                for (ushort i = 0; i != rowPtr->length; i++) {
                                    prevRowPtr->text[prevOldLength + i] = rowPtr->text[i];
                                }
                                free(rowPtr->text);

                                for (ulong i = cursorRowIndex; i != textBufferCount - 1; i++) {
                                    textBuffer[i] = textBuffer[i + 1];
                                }
                                textBufferCount--;

                                cursorRowIndex--;
                                cursorColIndex = prevRowPtr->length;

                                modified = TRUE;
                            }
                        } else {
                            TextRow * rowPtr = &textBuffer[cursorRowIndex];
                            for (ushort i = cursorColIndex; i != rowPtr->length; i++) {
                                rowPtr->text[i - 1] = rowPtr->text[i];
                            }
                            cursorColIndex--;
                            rowPtr->length--;

                            modified = TRUE;
                        }
                        UpdateColDrawIndex();
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
                    statusInputLength = wcslen(statusInput);
                    statusCursorCol = statusInputLength;
                    break;
                }

                default:
                {
                    if (iswcntrl(c)) {
                        break;
                    }

                    if (currentMode == MODE_INSERT) {
                        InsertChars(&c, 1);
                        modified = TRUE;
                        UpdateColDrawIndex();
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

            InvalidateRect(window, NULL, TRUE);
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, wchar_t * commandLine, int showCommand) {
    textBrush = CreateSolidBrush(textColor);
    backgroundBrush = CreateSolidBrush(backgroundColor);
    cursorBackgroundBrush = CreateSolidBrush(cursorBackgroundColor);
    statusBackgroundBrush = CreateSolidBrush(statusBackgroundColor);
    fileBackgroundBrush = CreateSolidBrush(fileBackgroundColor);
    promptTextBrush = CreateSolidBrush(promptTextColor);
    promptBackgroundBrush = CreateSolidBrush(promptBackgroundColor);

    textBuffer = malloc(TEXTBUFFER_GROW_COUNT * sizeof(TextRow));
    textBufferCount = 1;
    textBufferCapacity = TEXTBUFFER_GROW_COUNT;

    TextRow * row = &textBuffer[0];
    row->text = malloc(INIT_ROW_CAPACITY * sizeof(wchar_t));
    row->length = 0;
    row->capacity = INIT_ROW_CAPACITY;

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