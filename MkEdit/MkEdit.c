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

HFONT font;
long lineHeight;

LRESULT WindowProc(HWND window, unsigned int msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_PAINT:
        {
            PAINTSTRUCT paintStruct;
            HDC deviceContext = BeginPaint(window, &paintStruct);

            if (!font) {
                int ppi = GetDeviceCaps(deviceContext, LOGPIXELSY);
                int lfHeight = -(10 * ppi / 72);

                font = CreateFontW(
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
            }

            RECT windowRect;
            GetWindowRect(window, &windowRect);

            RECT drawRect = { 0 };
            drawRect.right = windowRect.right - windowRect.left;
            drawRect.bottom = windowRect.bottom - windowRect.top;

            HBRUSH backgroundBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(deviceContext, &drawRect, backgroundBrush);

            SetTextColor(deviceContext, RGB(0, 0, 0));
            SetBkColor(deviceContext, RGB(255, 255, 255));

            size_t line = 0;
            while (drawRect.top < drawRect.bottom && line != testLineCount) {
                DrawTextExW(
                    deviceContext,
                    testLines[line]->chars,
                    testLines[line]->length,
                    &drawRect,
                    DT_NOPREFIX,
                    NULL);
                drawRect.top += lineHeight;
                line++;
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

    WNDCLASSEXW windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
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