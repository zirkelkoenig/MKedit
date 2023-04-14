#include <windows.h>
#include "../../MKlib/MkLib.h"
#include "../../MKlib/MkString.h"
#include "../../MKlib/MkDynArray.h"

HANDLE heap;

void * MemAlloc(size_t size) {
    return HeapAlloc(heap, 0, size);
}

void * MemRealloc(void * pointer, size_t size) {
    return HeapReAlloc(heap, 0, pointer, size);
}

void MemFree(void * pointer) {
    HeapFree(heap, 0, pointer);
}

void MemCopy(void * dest, const void * source, size_t size) {
    CopyMemory(dest, source, size);
}

void MemMove(void * dest, const void * source, size_t size) {
    MoveMemory(dest, source, size);
}

MkLib_MemAlloc_Cb MkLib_MemAlloc = MemAlloc;
MkLib_MemRealloc_Cb MkLib_MemRealloc = MemRealloc;
MkLib_MemFree_Cb MkLib_MemFree = MemFree;
MkLib_MemCopy_Cb MkLib_MemCopy = MemCopy;
MkLib_MemMove_Cb MkLib_MemMove = MemMove;

wchar_t ** textBuffer;
size_t cursorRow;
size_t cursorCol;
size_t cursorColLast;

size_t lineHeight;
unsigned long backgroundColor;
unsigned long textColor;
unsigned long cursorBackgroundColor;
unsigned long statusTextColor;
unsigned long statusBackgroundColor;
unsigned long statusAlertBackgroundColor;

size_t clientWidth;
size_t clientHeight;
size_t topVisibleLine;
size_t visibleLineCount;

int ctrlKeyDown;
int askExit;

LRESULT WindowProc(HWND window, unsigned int msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_SIZE:
        {
            clientWidth = LOWORD(lparam);
            clientHeight = HIWORD(lparam);

            visibleLineCount = (clientHeight / lineHeight) - 1;
            size_t bottomVisibleLine = topVisibleLine + (visibleLineCount - 1);
            if (cursorRow > bottomVisibleLine) {
                topVisibleLine = cursorRow - (visibleLineCount - 1);
            } else {
                size_t textBufferLineCount = MkLib_DynArray_Count(textBuffer);
                if (bottomVisibleLine >= textBufferLineCount) {
                    if (textBufferLineCount <= visibleLineCount) {
                        topVisibleLine = 0;
                    } else {
                        topVisibleLine = textBufferLineCount - visibleLineCount;
                    }
                }
            }
            break;
        }

        case WM_CHAR:
        {
            switch (wparam) {
                case L'\n':
                {
                    OutputDebugStringW(L"Line Feed\n");
                    break;
                }

                case L'\r':
                {
                    if (ctrlKeyDown || askExit) {
                        break;
                    }

                    size_t currentLineLength = MkLib_DynArray_Count(textBuffer[cursorRow]);
                    size_t newLineLength = currentLineLength - cursorCol;
                    wchar_t * newLine = MkLib_DynArray_Create(wchar_t, newLineLength + 8, 8);
                    MkLib_DynArray_SetCount(&newLine, newLineLength);
                    MemCopy(newLine, textBuffer[cursorRow] + cursorCol, newLineLength * sizeof(wchar_t));
                    MkLib_DynArray_Insert(&textBuffer, cursorRow + 1, newLine);
                    MkLib_DynArray_SetCount(&textBuffer[cursorRow], cursorCol);
                    
                    cursorRow++;
                    cursorCol = 0;
                    cursorColLast = 0;
                    if (cursorRow >= topVisibleLine + visibleLineCount) {
                        topVisibleLine++;
                    }
                    InvalidateRect(window, NULL, 1);
                    break;
                }

                case L'\b':
                {
                    if (ctrlKeyDown || askExit) {
                        break;
                    }

                    if (cursorCol != 0) {
                        cursorCol--;
                        MkLib_DynArray_Remove(textBuffer[cursorRow], cursorCol);
                    } else if (cursorRow != 0) {
                        size_t lineLength = MkLib_DynArray_Count(textBuffer[cursorRow]);
                        size_t prevIndex = cursorRow - 1;
                        size_t prevLineLength = MkLib_DynArray_Count(textBuffer[prevIndex]);
                        MkLib_DynArray_SetCount(&textBuffer[prevIndex], prevLineLength + lineLength);
                        MemCopy(textBuffer[prevIndex] + prevLineLength, textBuffer[cursorRow], lineLength * sizeof(wchar_t));
                        MkLib_DynArray_Destroy(textBuffer[cursorRow]);
                        MkLib_DynArray_Remove(textBuffer, cursorRow);

                        cursorRow--;
                        cursorCol = prevLineLength;

                        if (cursorRow < topVisibleLine) {
                            topVisibleLine--;
                        } else {
                            size_t bottomVisibleLine = topVisibleLine + (visibleLineCount - 1);
                            size_t lineCount = MkLib_DynArray_Count(textBuffer);
                            if (bottomVisibleLine >= lineCount) {
                                if (lineCount <= visibleLineCount) {
                                    topVisibleLine = 0;
                                } else {
                                    topVisibleLine = lineCount - visibleLineCount;
                                }
                            }
                        }
                    }
                    cursorColLast = cursorCol;
                    InvalidateRect(window, NULL, 1);
                    break;
                }

                default:
                {
                    if (ctrlKeyDown) {
                        break;
                    }

                    if (askExit) {
                        wchar_t letter = (wchar_t)wparam;
                        if (letter == L'Y' || letter == L'y') {
                            DestroyWindow(window);
                        } else if (letter == L'N' || letter == L'n') {
                            askExit = 0;
                            InvalidateRect(window, NULL, 1);
                        }
                    } else {
                        MkLib_DynArray_Insert(&textBuffer[cursorRow], cursorCol, (wchar_t)wparam);
                        cursorCol++;
                        cursorColLast = cursorCol;
                        InvalidateRect(window, NULL, 1);
                    }
                    break;
                }
            }
            break;
        }

        case WM_KEYDOWN:
        {
            if (askExit) {
                return 0;
            }

            switch (wparam) {
                
                //-----------------
                // Modifier Keys
                case VK_CONTROL:
                {
                    ctrlKeyDown = 1;
                    break;
                }

                //-------------
                // Arrow Keys
                case VK_LEFT:
                {
                    if (ctrlKeyDown) {
                        break;
                    }

                    if (cursorCol > 0) {
                        cursorCol--;
                    } else if (cursorRow != 0) {
                        cursorRow--;
                        cursorCol = MkLib_DynArray_Count(textBuffer[cursorRow]);

                        if (cursorRow < topVisibleLine) {
                            topVisibleLine--;
                        } else {
                            size_t bottomVisibleLine = topVisibleLine + (visibleLineCount - 1);
                            size_t lineCount = MkLib_DynArray_Count(textBuffer);
                            if (bottomVisibleLine >= lineCount) {
                                if (lineCount <= visibleLineCount) {
                                    topVisibleLine = 0;
                                } else {
                                    topVisibleLine = lineCount - visibleLineCount;
                                }
                            }
                        }
                    }
                    cursorColLast = cursorCol;
                    InvalidateRect(window, NULL, 1);
                    break;
                }

                case VK_RIGHT:
                {
                    if (ctrlKeyDown) {
                        break;
                    }

                    if (cursorCol < MkLib_DynArray_Count(textBuffer[cursorRow])) {
                        cursorCol++;
                    } else if (cursorRow < MkLib_DynArray_Count(textBuffer) - 1) {
                        cursorRow++;
                        cursorCol = 0;
                        if (cursorRow >= topVisibleLine + visibleLineCount) {
                            topVisibleLine++;
                        }
                    }
                    cursorColLast = cursorCol;
                    InvalidateRect(window, NULL, 1);
                    break;
                }

                case VK_UP:
                {
                    if (ctrlKeyDown) {
                        if (topVisibleLine > 0) {
                            topVisibleLine--;
                            if (cursorRow >= topVisibleLine + visibleLineCount) {
                                cursorRow--;

                                size_t lineLength = MkLib_DynArray_Count(textBuffer[cursorRow]);
                                if (cursorColLast <= lineLength) {
                                    cursorCol = cursorColLast;
                                } else {
                                    cursorCol = lineLength;
                                }
                            }
                        }
                    } else {
                        if (cursorRow > 0) {
                            cursorRow--;

                            size_t lineLength = MkLib_DynArray_Count(textBuffer[cursorRow]);
                            if (cursorColLast <= lineLength) {
                                cursorCol = cursorColLast;
                            } else {
                                cursorCol = lineLength;
                            }

                            if (cursorRow < topVisibleLine) {
                                topVisibleLine = cursorRow;
                            }
                        }
                    }
                    InvalidateRect(window, NULL, 1);
                    break;
                }

                case VK_DOWN:
                {
                    size_t lineCount = MkLib_DynArray_Count(textBuffer);
                    if (ctrlKeyDown) {
                        if (topVisibleLine + visibleLineCount < lineCount) {
                            topVisibleLine++;
                            if (cursorRow < topVisibleLine) {
                                cursorRow++;

                                size_t lineLength = MkLib_DynArray_Count(textBuffer[cursorRow]);
                                if (cursorColLast <= lineLength) {
                                    cursorCol = cursorColLast;
                                } else {
                                    cursorCol = lineLength;
                                }
                            }
                        }
                    } else {
                        if (cursorRow < lineCount - 1) {
                            cursorRow++;

                            size_t lineLength = MkLib_DynArray_Count(textBuffer[cursorRow]);
                            if (cursorColLast <= lineLength) {
                                cursorCol = cursorColLast;
                            } else {
                                cursorCol = lineLength;
                            }

                            if (cursorRow >= topVisibleLine + visibleLineCount) {
                                topVisibleLine++;
                            }
                        }
                    }
                    InvalidateRect(window, NULL, 1);
                    break;
                }

                //-------------------------
                // Nagivation Block Keys
                case VK_DELETE:
                {
                    if (ctrlKeyDown) {
                        break;
                    }

                    size_t lineLength = MkLib_DynArray_Count(textBuffer[cursorRow]);
                    if (cursorCol < lineLength) {
                        MkLib_DynArray_Remove(textBuffer[cursorRow], cursorCol);
                        lineLength--;
                        if (cursorCol > lineLength) {
                            cursorCol = lineLength;
                        }
                    } else {
                        size_t lineCount = MkLib_DynArray_Count(textBuffer);
                        if (cursorRow < lineCount - 1) {
                            size_t nextIndex = cursorRow + 1;
                            size_t nextLineLength = MkLib_DynArray_Count(textBuffer[nextIndex]);
                            MkLib_DynArray_SetCount(&textBuffer[cursorRow], lineLength + nextLineLength);
                            MemCopy(textBuffer[cursorRow] + lineLength, textBuffer[nextIndex], nextLineLength * sizeof(wchar_t));
                            MkLib_DynArray_Destroy(textBuffer[nextIndex]);
                            MkLib_DynArray_Remove(textBuffer, nextIndex);
                            lineCount--;

                            size_t bottomVisibleLine = topVisibleLine + (visibleLineCount - 1);
                            if (bottomVisibleLine >= lineCount) {
                                if (lineCount <= visibleLineCount) {
                                    topVisibleLine = 0;
                                } else {
                                    topVisibleLine = lineCount - visibleLineCount;
                                }
                            }
                        }
                    }
                    cursorColLast = cursorCol;
                    InvalidateRect(window, NULL, 1);
                    break;
                }

                case VK_HOME:
                {
                    if (ctrlKeyDown && cursorRow != 0) {
                        cursorRow = 0;
                        topVisibleLine = 0;
                    }
                    if (cursorCol != 0) {
                        cursorCol = 0;
                        cursorColLast = cursorCol;
                    }
                    InvalidateRect(window, NULL, 1);
                    break;
                }

                case VK_END:
                {
                    if (ctrlKeyDown) {
                        size_t lineCount = MkLib_DynArray_Count(textBuffer);
                        if (cursorRow < lineCount - 1) {
                            cursorRow = lineCount - 1;
                            if (lineCount > visibleLineCount) {
                                topVisibleLine = lineCount - visibleLineCount;
                            }
                        }
                    }

                    size_t lineLength = MkLib_DynArray_Count(textBuffer[cursorRow]);
                    cursorCol = lineLength;
                    cursorColLast = cursorCol;

                    InvalidateRect(window, NULL, 1);
                    break;
                }

                case VK_PRIOR:
                {
                    if (ctrlKeyDown) {
                        break;
                    }

                    size_t cursorRowRelative = cursorRow - topVisibleLine;
                    if (cursorRow >= visibleLineCount) {
                        cursorRow -= visibleLineCount;
                        if (cursorRowRelative <= cursorRow) {
                            topVisibleLine = cursorRow - cursorRowRelative;
                        } else {
                            topVisibleLine = 0;
                        }
                    } else {
                        cursorRow = 0;
                        topVisibleLine = 0;
                    }

                    size_t lineLength = MkLib_DynArray_Count(textBuffer[cursorRow]);
                    if (cursorColLast <= lineLength) {
                        cursorCol = cursorColLast;
                    } else {
                        cursorCol = lineLength;
                    }

                    InvalidateRect(window, NULL, 1);
                    break;
                }

                case VK_NEXT:
                {
                    if (ctrlKeyDown) {
                        break;
                    }

                    size_t cursorRowOffset = cursorRow - topVisibleLine;
                    size_t lineCount = MkLib_DynArray_Count(textBuffer);
                    if (lineCount - cursorRow > visibleLineCount) {
                        cursorRow += visibleLineCount;
                        if (cursorRow - cursorRowOffset + (visibleLineCount - 1) < lineCount) {
                            topVisibleLine = cursorRow - cursorRowOffset;
                        } else {
                            topVisibleLine = lineCount - visibleLineCount;
                        }
                    } else {
                        cursorRow = lineCount - 1;
                        if (lineCount > visibleLineCount) {
                            topVisibleLine = lineCount - visibleLineCount;
                        }
                    }

                    size_t lineLength = MkLib_DynArray_Count(textBuffer[cursorRow]);
                    if (cursorColLast <= lineLength) {
                        cursorCol = cursorColLast;
                    } else {
                        cursorCol = lineLength;
                    }

                    InvalidateRect(window, NULL, 1);
                    break;
                }
            }
            return 0;
        }

        case WM_KEYUP:
        {
            switch (wparam) {
                case VK_CONTROL:
                {
                    ctrlKeyDown = 0;
                    break;
                }
            }
            return 0;
        }

        case WM_CREATE:
        {
            HDC deviceContext = GetDC(window);

            int ppi = GetDeviceCaps(deviceContext, LOGPIXELSY);
            int lfHeight = -(10 * ppi / 72);
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
                L"Consolas");
            HFONT oldFont = SelectObject(deviceContext, font);
            if (oldFont) {
                DeleteObject(oldFont);
            }

            TEXTMETRICW fontMetrics;
            GetTextMetricsW(deviceContext, &fontMetrics);
            lineHeight = fontMetrics.tmHeight;

            backgroundColor = RGB(255, 255, 255);
            textColor = RGB(0, 0, 0);
            cursorBackgroundColor = RGB(128, 128, 128);
            statusTextColor = RGB(255, 255, 255);
            statusBackgroundColor = RGB(64, 80, 141);
            statusAlertBackgroundColor = RGB(162, 75, 64);

            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paintStruct;
            HDC deviceContext = BeginPaint(window, &paintStruct);

            RECT drawRect = { 0 };
            drawRect.right = clientWidth;
            drawRect.bottom = clientHeight;

            HBRUSH backgroundBrush = CreateSolidBrush(backgroundColor);
            FillRect(deviceContext, &drawRect, backgroundBrush);

            SetTextColor(deviceContext, textColor);
            SetBkColor(deviceContext, backgroundColor);

            size_t bottomVisibleLine = topVisibleLine + visibleLineCount - 1;
            size_t lineCount = MkLib_DynArray_Count(textBuffer);
            for (size_t i = topVisibleLine; i <= bottomVisibleLine && i != lineCount; i++) {
                size_t lineLength = MkLib_DynArray_Count(textBuffer[i]);

                if (i == cursorRow && !askExit) {
                    SIZE extent;
                    GetTextExtentPoint32W(
                        deviceContext,
                        textBuffer[i],
                        cursorCol,
                        &extent);
                    DrawTextExW(
                        deviceContext,
                        textBuffer[i],
                        cursorCol,
                        &drawRect,
                        DT_NOPREFIX,
                        NULL);
                    drawRect.left += extent.cx;

                    if (drawRect.left < drawRect.right) {
                        SetBkColor(deviceContext, cursorBackgroundColor);
                        if (cursorCol < lineLength) {
                            GetTextExtentPoint32W(
                                deviceContext,
                                textBuffer[i] + cursorCol,
                                1,
                                &extent);
                            DrawTextExW(
                                deviceContext,
                                textBuffer[i] + cursorCol,
                                1,
                                &drawRect,
                                DT_NOPREFIX,
                                NULL);
                            drawRect.left += extent.cx;
                            SetBkColor(deviceContext, backgroundColor);

                            if (drawRect.left < drawRect.right) {
                                DrawTextExW(
                                    deviceContext,
                                    textBuffer[i] + cursorCol + 1,
                                    lineLength - (cursorCol + 1),
                                    &drawRect,
                                    DT_NOPREFIX,
                                    NULL);
                            }
                        } else {
                            DrawTextExW(
                                deviceContext,
                                L" ",
                                1,
                                &drawRect,
                                DT_NOPREFIX,
                                NULL);
                            SetBkColor(deviceContext, backgroundColor);
                        }
                    }
                    drawRect.left = 0;
                } else {
                    DrawTextExW(
                        deviceContext,
                        textBuffer[i],
                        lineLength,
                        &drawRect,
                        DT_NOPREFIX,
                        NULL);
                }
                drawRect.top += lineHeight;
            }

            drawRect.top = drawRect.bottom - lineHeight;
            if (askExit) {
                HBRUSH statusBackgroundBrush = CreateSolidBrush(statusAlertBackgroundColor);
                FillRect(deviceContext, &drawRect, statusBackgroundBrush);

                SetTextColor(deviceContext, statusTextColor);
                SetBkColor(deviceContext, statusAlertBackgroundColor);
                DrawTextExW(
                    deviceContext,
                    L"Quit? (Y/N)",
                    11,
                    &drawRect,
                    DT_NOPREFIX,
                    NULL);
            } else {
                // u64 max = 20 chars
                size_t rowPercent = (100 * cursorRow) / (lineCount - 1);
                wchar_t status[90];
                size_t statusLength = swprintf(status, 90, L"Line: %zu (%zu %%)    Char: %zu", cursorRow, rowPercent, cursorCol);

                HBRUSH statusBackgroundBrush = CreateSolidBrush(statusBackgroundColor);
                FillRect(deviceContext, &drawRect, statusBackgroundBrush);

                SetTextColor(deviceContext, statusTextColor);
                SetBkColor(deviceContext, statusBackgroundColor);
                DrawTextExW(
                    deviceContext,
                    status,
                    statusLength,
                    &drawRect,
                    DT_NOPREFIX,
                    NULL);
            }

            EndPaint(window, &paintStruct);
            return 0;
        }

        case WM_CLOSE:
        {
            askExit = 1;
            InvalidateRect(window, NULL, 1);
            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }

        default:
        {
            return DefWindowProcW(window, msg, wparam, lparam);
        }
    }
}

int WinMain(HINSTANCE instance, HINSTANCE prevInstance, char * commandLine, int showCommand) {
    heap = GetProcessHeap();

    //---------------
    // Test Setup
    {
        HANDLE testTextFile = CreateFileW(
            L"..\\..\\MKlib\\MkString.c",
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        LARGE_INTEGER fileSize;
        GetFileSizeEx(testTextFile, &fileSize);

        byte * utf8 = MemAlloc(fileSize.u.LowPart);
        unsigned long bytesRead;
        ReadFile(
            testTextFile,
            utf8,
            fileSize.u.LowPart,
            &bytesRead,
            NULL);

        CloseHandle(testTextFile);

        textBuffer = MkLib_CwDynArrayLinesFromUtf8(utf8, fileSize.u.LowPart);
        MemFree(utf8);
    }

    //---------------
    // Window Setup
    HCURSOR cursor = LoadCursorW(NULL, IDC_ARROW);

    WNDCLASSEXW windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = cursor;
    windowClass.lpszClassName = L"MKedit";
    RegisterClassExW(&windowClass);

    HWND window = CreateWindowExW(
        0,
        L"MKedit",
        L"MKedit",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        instance,
        NULL);
    ShowWindow(window, showCommand);

    MSG msg;
    int msgRc;
    while ((msgRc = GetMessageW(&msg, NULL, 0, 0)) != 0) {
        if (msgRc == -1) {
            unsigned long errorCode = GetLastError();
            wchar_t errorString[256] = { 0 };
            FormatMessageW(
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                errorCode,
                0,
                errorString,
                256,
                NULL);
            OutputDebugStringW(errorString);
        } else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return 0;
}