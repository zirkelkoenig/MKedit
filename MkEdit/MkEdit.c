#include <windows.h>
#include "../../MKlib/MkLib.h"
#include "../../MKlib/MkString.h"

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

MkLib_MemAlloc_Cb MkLib_MemAlloc = MemAlloc;
MkLib_MemRealloc_Cb MkLib_MemRealloc = MemRealloc;
MkLib_MemFree_Cb MkLib_MemFree = MemFree;
MkLib_MemCopy_Cb MkLib_MemCopy = MemCopy;

size_t testLineCount;
MkLib_StringW ** testLines;
size_t cursorRow;
size_t cursorCol;

size_t lineHeight;
unsigned long backgroundColor;
unsigned long textColor;
unsigned long cursorBackgroundColor;

size_t clientWidth;
size_t clientHeight;
size_t topVisibleLine;
size_t visibleLineCount;

LRESULT WindowProc(HWND window, unsigned int msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_SIZE:
        {
            clientWidth = LOWORD(lparam);
            clientHeight = HIWORD(lparam);

            visibleLineCount = clientHeight / lineHeight;
            size_t bottomVisibleLine = topVisibleLine + (visibleLineCount - 1);
            if (cursorRow > bottomVisibleLine) {
                topVisibleLine = cursorRow - (visibleLineCount - 1);
            } else if (bottomVisibleLine >= testLineCount) {
                if (testLineCount <= visibleLineCount) {
                    topVisibleLine = 0;
                } else {
                    topVisibleLine = testLineCount - visibleLineCount;
                }
            }
            break;
        }

        case WM_KEYDOWN:
        {
            RECT updateRect = { 0 };
            updateRect.right = clientWidth;
            updateRect.bottom = clientHeight;

            switch (wparam) {
                case VK_LEFT:
                {
                    if (cursorCol > 0) {
                        cursorCol--;
                        InvalidateRect(window, NULL, 1);
                    }
                    break;
                }

                case VK_RIGHT:
                {
                    if (cursorCol < testLines[cursorRow]->length) {
                        cursorCol++;
                        InvalidateRect(window, NULL, 1);
                    }
                    break;
                }

                case VK_UP:
                {
                    if (cursorRow > 0) {
                        cursorRow--;
                        if (cursorCol > testLines[cursorRow]->length) {
                            cursorCol = testLines[cursorRow]->length;
                        }
                        if (cursorRow < topVisibleLine) {
                            topVisibleLine = cursorRow;
                        }
                        InvalidateRect(window, NULL, 1);
                    }
                    break;
                }

                case VK_DOWN:
                {
                    if (cursorRow < testLineCount - 1) {
                        cursorRow++;
                        if (cursorCol > testLines[cursorRow]->length) {
                            cursorCol = testLines[cursorRow]->length;
                        }
                        if (cursorRow >= topVisibleLine + visibleLineCount) {
                            topVisibleLine++;
                        }
                        InvalidateRect(window, NULL, 1);
                    }
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
            for (size_t i = topVisibleLine; i <= bottomVisibleLine && i != testLineCount; i++) {
                if (i == cursorRow) {
                    SIZE extent;
                    GetTextExtentPoint32W(
                        deviceContext,
                        testLines[i]->chars,
                        cursorCol,
                        &extent);
                    DrawTextExW(
                        deviceContext,
                        testLines[i]->chars,
                        cursorCol,
                        &drawRect,
                        DT_NOPREFIX,
                        NULL);
                    drawRect.left += extent.cx;

                    SetBkColor(deviceContext, cursorBackgroundColor);
                    if (cursorCol < testLines[i]->length) {
                        GetTextExtentPoint32W(
                            deviceContext,
                            testLines[i]->chars + cursorCol,
                            1,
                            &extent);
                        DrawTextExW(
                            deviceContext,
                            testLines[i]->chars + cursorCol,
                            1,
                            &drawRect,
                            DT_NOPREFIX,
                            NULL);
                        drawRect.left += extent.cx;
                        SetBkColor(deviceContext, backgroundColor);

                        DrawTextExW(
                            deviceContext,
                            testLines[i]->chars + cursorCol + 1,
                            testLines[i]->length - (cursorCol + 1),
                            &drawRect,
                            DT_NOPREFIX,
                            NULL);
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

                    drawRect.left = 0;
                } else {
                    DrawTextExW(
                        deviceContext,
                        testLines[i]->chars,
                        testLines[i]->length,
                        &drawRect,
                        DT_NOPREFIX,
                        NULL);
                }
                drawRect.top += lineHeight;
            }

            EndPaint(window, &paintStruct);
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

        testLines = MkLib_StringW_LinesFromUtf8(utf8, fileSize.u.LowPart, &testLineCount);
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