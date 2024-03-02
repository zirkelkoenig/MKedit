#include <Windows.h>

#include <KnownFolders.h>
#include <objbase.h>
#include <ShlObj.h>

#include <stdio.h>
#include <stdlib.h>
#include <wctype.h>

#include "Generated/ConfigGen.h"
#include "Import/MkList.h"
#include "Import/MkString.h"
#include "Base.h"

#if false
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
#endif


Config config;

bool ReadFileCallback(void * stream, void * buffer, ulong count, void * status) {
    ulong * error = static_cast<ulong *>(status);

    ulong readCount;
    if (ReadFile(stream, buffer, count, &readCount, nullptr)) {
        *error = ERROR_SUCCESS;
        if (readCount == 0) {
            return false;
        } else {
            return true;
        }
    } else {
        *error = GetLastError();
        return false;
    }
}

// Returns:
// - RESULT_OK
// - RESULT_MEMORY_ERROR
ResultCode LoadConfigFile(const wchar_t filePath[MAX_PATH_COUNT]) {
    HANDLE file = CreateFileW(
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return RESULT_OK;
    }

    MkList content;
    MkListInit(&content, 128, sizeof(wchar_t));

    auto writeCallback = [](void * stream, const void * buffer, ulong count, void * status) {
        MkList * content = static_cast<MkList *>(stream);
        const wchar_t * wcs = static_cast<const wchar_t *>(buffer);

        wchar_t * newElems = static_cast<wchar_t *>(MkListInsert(content, ULONG_MAX, count));
        if (!newElems) {
            return false;
        }
        for (ulong i = 0; i != count; i++) {
            newElems[i] = wcs[i];
        }
        return true;
    };

    ulong readStatus;
    bool utf8Success = MkUtf8Read(
        ReadFileCallback, file, &readStatus,
        writeCallback, &content, nullptr);

    CloseHandle(file);

    if (!utf8Success) {
        return RESULT_MEMORY_ERROR;
    }

    MkConfGenLoadError * loadErrors;
    ulong loadErrorCount;
    bool loadSuccess = ConfigLoad(&config, static_cast<wchar_t *>(content.elems), content.count, &loadErrors, &loadErrorCount);
    if (loadErrors) {
        free(loadErrors);
    }

    MkListClear(&content);

    if (loadSuccess) {
        return RESULT_OK;
    } else {
        return RESULT_MEMORY_ERROR;
    }
}

// Returns:
// - RESULT_OK
// - RESULT_FILE_LOCKED - existing file is locked by another process
// - RESULT_FILE_EXISTS - file with same name already exists or was modified externally
// - RESULT_FILE_ERROR
// - RESULT_FILE_NOT_FOUND - file was removed
ResultCode WriteDoc(Doc * doc, const wchar_t * newPath, bool overwrite) {
    DWORD disposition;
    const wchar_t * path;
    if (newPath) {
        path = newPath;
        if (overwrite) {
            disposition = CREATE_ALWAYS;
        } else {
            disposition = CREATE_NEW;
        }
    } else {
        path = doc->title;
        if (overwrite) {
            disposition = CREATE_ALWAYS;
        } else if (doc->timestamp == 0) {
            disposition = CREATE_NEW;
        } else {
            disposition = OPEN_EXISTING;
        }
    }

    HANDLE file = CreateFileW(
        path,
        GENERIC_WRITE,
        0,
        nullptr,
        disposition,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        ulong error = GetLastError();
        switch (error) {
            case ERROR_FILE_EXISTS:
                return RESULT_FILE_EXISTS;

            case ERROR_FILE_NOT_FOUND:
                return RESULT_FILE_NOT_FOUND;

            case ERROR_SHARING_VIOLATION:
                return RESULT_FILE_LOCKED;

            default:
                return RESULT_FILE_ERROR;
        }
    }

    if (disposition == OPEN_EXISTING) {
        FILETIME fileTimestamp;
        GetFileTime(file, nullptr, nullptr, &fileTimestamp);
        ULARGE_INTEGER timestamp;
        timestamp.LowPart = fileTimestamp.dwLowDateTime;
        timestamp.HighPart = fileTimestamp.dwHighDateTime;
        if (timestamp.QuadPart != doc->timestamp) {
            CloseHandle(file);
            return RESULT_FILE_EXISTS;
        }
    }

    auto writeCallback = [](void * stream, const void * buffer, ulong count, void * status) {
        ulong writeCount;
        return static_cast<bool>(WriteFile(stream, buffer, count, &writeCount, nullptr));
    };

    bool writeSuccess = MkUtf8WriteWcs(
        doc->content.lines[0].chars, doc->content.lines[0].length, true,
        writeCallback, file, nullptr);
    if (!writeSuccess) {
        CloseHandle(file);
        return RESULT_FILE_ERROR;
    }

    for (ulong i = 1; i != doc->content.lineCount; i++) {
        writeSuccess = MkUtf8WriteWcs(
            L"\n", 1, true,
            writeCallback, file, nullptr);
        if (!writeSuccess) {
            CloseHandle(file);
            return RESULT_FILE_ERROR;
        }

        writeSuccess = MkUtf8WriteWcs(
            doc->content.lines[i].chars, doc->content.lines[i].length, true,
            writeCallback, file, nullptr);
        if (!writeSuccess) {
            CloseHandle(file);
            return RESULT_FILE_ERROR;
        }
    }

    if (disposition == OPEN_EXISTING) {
        SetEndOfFile(file);
    }

    FILETIME fileTimestamp;
    GetFileTime(file, nullptr, nullptr, &fileTimestamp);
    ULARGE_INTEGER timestamp;
    timestamp.LowPart = fileTimestamp.dwLowDateTime;
    timestamp.HighPart = fileTimestamp.dwHighDateTime;
    doc->timestamp = timestamp.QuadPart;

    CloseHandle(file);
    return RESULT_OK;
}

// Returns:
// - RESULT_OK
// - RESULT_MEMORY_ERROR
// - RESULT_LIMIT_REACHED
// - RESULT_FILE_ERROR
// - RESULT_FILE_LOCKED
// - RESULT_FILE_NOT_FOUND
ResultCode LoadFile(const wchar_t * path, Doc ** doc) {
    HANDLE file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        ulong error = GetLastError();
        switch (error) {
            case ERROR_SHARING_VIOLATION:
                return RESULT_FILE_LOCKED;

            case ERROR_FILE_NOT_FOUND:
                return RESULT_FILE_NOT_FOUND;

            default:
                return RESULT_FILE_ERROR;
        }
    }

    *doc = CreateEmptyDoc();
    if (!(*doc)) {
        CloseHandle(file);
        return RESULT_MEMORY_ERROR;
    }

    auto writeCallback = [](void * stream, const void * buffer, ulong count, void * status) {
        Doc * doc = static_cast<Doc *>(stream);
        const wchar_t * chars = static_cast<const wchar_t *>(buffer);
        ResultCode * result = static_cast<ResultCode *>(status);

        for (ulong i = 0; i != count; i++) {
            if (chars[i] == L'\n') {
                if (doc->content.lineCount == MAX_LINE_COUNT) {
                    *result = RESULT_LIMIT_REACHED;
                    return false;
                }

                if (doc->content.lineCount == doc->content.lineCapacity) {
                    ulong newCapacity = doc->content.lineCapacity + DOCLINES_GROW_COUNT;
                    DocLine * newLines = static_cast<DocLine *>(realloc(doc->content.lines, newCapacity * sizeof(DocLine)));
                    if (!newLines) {
                        *result = RESULT_MEMORY_ERROR;
                        return false;
                    }
                    doc->content.lineCapacity = newCapacity;
                    doc->content.lines = newLines;
                }

                DocLine * line = &doc->content.lines[doc->content.lineCount++];
                line->length = 0;
                line->capacity = DOCLINE_INIT_CAPACITY;
                line->chars = static_cast<wchar_t *>(malloc(line->capacity * sizeof(wchar_t)));
                if (!line->chars) {
                    *result = RESULT_MEMORY_ERROR;
                    return false;
                }
            } else {
                DocLine * line = &doc->content.lines[doc->content.lineCount - 1];

                if (line->length == MAX_LINE_LENGTH) {
                    *result = RESULT_LIMIT_REACHED;
                    return false;
                }

                if (line->length == line->capacity) {
                    ulong newCapacity = line->capacity * 2;
                    wchar_t * newChars = static_cast<wchar_t *>(realloc(line->chars, newCapacity * sizeof(wchar_t)));
                    if (!newChars) {
                        *result = RESULT_MEMORY_ERROR;
                        return false;
                    }
                    line->capacity = newCapacity;
                    line->chars = newChars;
                }

                line->chars[line->length++] = chars[i];
            }
        }

        return true;
    };

    ulong readStatus;
    ResultCode writeStatus;
    bool readSuccess = MkUtf8Read(
        ReadFileCallback, file, &readStatus,
        writeCallback, *doc, &writeStatus);
    if (!readSuccess) {
        CloseHandle(file);
        DestroyDoc(*doc);
        return writeStatus;
    }

    FILETIME fileTimestamp;
    GetFileTime(file, nullptr, nullptr, &fileTimestamp);
    ULARGE_INTEGER timestamp;
    timestamp.LowPart = fileTimestamp.dwLowDateTime;
    timestamp.HighPart = fileTimestamp.dwHighDateTime;

    CloseHandle(file);

    (*doc)->cursorLineIndex = 0;
    (*doc)->cursorCharIndex = 0;
    (*doc)->timestamp = timestamp.QuadPart;
    wcscpy_s((*doc)->title, MAX_PATH_COUNT, path);
    return RESULT_OK;
}

wchar_t * tabSpaces;

HBRUSH textBrush;
HBRUSH backgroundBrush;
HBRUSH cursorBrush;
HBRUSH statusBackgroundBrush;
HBRUSH docTitleBackgroundBrush;
HBRUSH promptTextBrush;
HBRUSH promptBackgroundBrush;

enum Mode {
    MODE_NORMAL,
    MODE_COMMAND,
    MODE_INSERT,
};

Mode currentMode = MODE_NORMAL;

wchar_t workingFolderPath[MAX_PATH];
Doc * currentDoc;
bool paintContentCursor = true;
bool paintStatusCursor = false;

#define MAX_STATUS_COUNT 512
wchar_t statusLine[MAX_STATUS_COUNT];
ulong statusLength = 0;
ulong statusCursorChar = 0;
bool statusPrompt = false;

void SetStatusLineNormal() {
    ulong cursorLinePercent;
    if (currentDoc->content.lineCount > 1) {
        cursorLinePercent = (100 * currentDoc->cursorLineIndex) / (currentDoc->content.lineCount - 1);
    } else {
        cursorLinePercent = 0;
    }

    DocLine * cursorLine = &currentDoc->content.lines[currentDoc->cursorLineIndex];

    ulong cursorColCount = 0;
    for (ulong i = 0; i != currentDoc->cursorCharIndex; i++) {
        if (cursorLine->chars[i] == L'\t') {
            cursorColCount += config.tabWidth;
        } else {
            cursorColCount++;
        }
    }

    ulong cursorLineColCount = 0;
    for (ulong i = 0; i != cursorLine->length; i++) {
        if (cursorLine->chars[i] == L'\t') {
            cursorLineColCount += config.tabWidth;
        } else {
            cursorLineColCount++;
        }
    }


    const wchar_t modeNameNormal[] = L"NORMAL";
    const wchar_t modeNameInsert[] = L"INSERT";
    const wchar_t * modeName;
    if (currentMode == MODE_INSERT) {
        modeName = modeNameInsert;
    } else {
        modeName = modeNameNormal;
    }

    statusLength = swprintf_s(
        statusLine,
        MAX_STATUS_COUNT,
        L"%s | Line: %lu/%lu (%lu %%) | Char: %lu/%lu (%lu/%lu)",
        modeName,
        currentDoc->cursorLineIndex + 1,
        currentDoc->content.lineCount,
        cursorLinePercent,
        currentDoc->cursorCharIndex + 1,
        cursorLine->length,
        cursorColCount + 1,
        cursorLineColCount);

    statusPrompt = false;
}

void SetStatusInvalidCommand(const wchar_t * text) {
    currentMode = MODE_NORMAL;
    paintContentCursor = true;
    paintStatusCursor = false;
    wcscpy_s(statusLine, MAX_STATUS_COUNT, text);
    statusLength = static_cast<ulong>(wcslen(text));
    statusPrompt = true;
}

void ProcessNormalCharInput(wchar_t c) {
    switch (c) {
        case L':':
        {
            currentMode = MODE_COMMAND;
            statusLine[0] = L':';
            statusLength = 1;
            statusCursorChar = 1;
            paintContentCursor = false;
            paintStatusCursor = true;
            statusPrompt = true;
            break;
        }

        case L'i':
        {
            currentMode = MODE_INSERT;
            SetStatusLineNormal();
            break;
        }

        case 0x1b: // Esc
        {
            SetStatusLineNormal();
            break;
        }

        case L'h':
        {
            if (currentDoc->cursorCharIndex != 0) {
                currentDoc->cursorCharIndex--;
            }
            ResetColIndex(currentDoc);
            SetStatusLineNormal();
            break;
        }

        case L'l':
        {
            ulong length = currentDoc->content.lines[currentDoc->cursorLineIndex].length;
            if (length != 0 && currentDoc->cursorCharIndex != length - 1) {
                currentDoc->cursorCharIndex++;
            }
            ResetColIndex(currentDoc);
            SetStatusLineNormal();
            break;
        }

        case L'k':
        {
            if (currentDoc->cursorLineIndex != 0) {
                currentDoc->cursorLineIndex--;
                ApplyColIndex(currentDoc, false);
            }
            SetStatusLineNormal();
            break;
        }

        case L'j':
        {
            if (currentDoc->cursorLineIndex != currentDoc->content.lineCount - 1) {
                currentDoc->cursorLineIndex++;
                ApplyColIndex(currentDoc, false);
            }
            SetStatusLineNormal();
            break;
        }

        case L'0':
        {
            currentDoc->cursorCharIndex = 0;
            currentDoc->lastCursorColIndex = 0;
            SetStatusLineNormal();
            break;
        }

        case L'$':
        {
            ulong length = currentDoc->content.lines[currentDoc->cursorLineIndex].length;
            if (length != 0) {
                currentDoc->cursorCharIndex = length - 1;
            }
            ResetColIndex(currentDoc);
            SetStatusLineNormal();
            break;
        }
    }
}

void ExecuteCommandEdit(const wchar_t * args, ulong argsLength) {
    const wchar_t statusArgsInvalid[] = L"Command args invalid!";
    const wchar_t statusPathTooLong[] = L"Path too long!";
    const wchar_t statusFileTooLarge[] = L"File too large!";
    const wchar_t statusFileError[] = L"Could not open file.";
    const wchar_t statusFileLocked[] = L"File locked!";
    const wchar_t statusOutOfMemory[] = L"Out of memory!";
    const wchar_t statusCurDocUnsaved[] = L"Current document has unsaved changes!";

    //-----------
    // Parse Args

    ulong i = 0;
    while (i != argsLength && iswspace(args[i])) {
        i++;
    }
    if (i == argsLength) {
        SetStatusInvalidCommand(statusArgsInvalid);
        return;
    }

    if (args[i] == L'!') {
        i++;
    } else if (currentDoc->modified) {
        SetStatusInvalidCommand(statusCurDocUnsaved);
        return;
    }

    while (i != argsLength && iswspace(args[i])) {
        i++;
    }
    if (i == argsLength) {
        SetStatusInvalidCommand(statusArgsInvalid);
        return;
    }

    ulong j;
    if (args[i] == L'\"') {
        i++;
        j = i;
        while (j != argsLength && args[j] != L'\"') {
            j++;
        }
        if (j == argsLength) {
            SetStatusInvalidCommand(statusArgsInvalid);
            return;
        }
    } else {
        j = i;
        while (j != argsLength && !iswspace(args[j])) {
            j++;
        }
    }

    for (ulong k = j; k != argsLength; k++) {
        if (!iswspace(args[k])) {
            SetStatusInvalidCommand(statusArgsInvalid);
            return;
        }
    }

    //-------------
    // Load File

    if (j - i > MAX_PATH_COUNT) {
        SetStatusInvalidCommand(statusPathTooLong);
        return;
    }
    wchar_t path[MAX_PATH_COUNT];
    wcsncpy_s(path, MAX_PATH_COUNT, args + i, j - i);

    Doc * fileDoc;
    ResultCode resultCode = LoadFile(path, &fileDoc);
    switch (resultCode) {
        case RESULT_LIMIT_REACHED:
        {
            SetStatusInvalidCommand(statusFileTooLarge);
            break;
        }

        case RESULT_FILE_LOCKED:
        {
            SetStatusInvalidCommand(statusFileLocked);
            break;
        }

        case RESULT_FILE_NOT_FOUND:
        {
            Doc * newDoc = CreateEmptyDoc();
            if (!newDoc) {
                SetStatusInvalidCommand(statusOutOfMemory);
                break;
            }
            DestroyDoc(currentDoc);
            currentDoc = newDoc;
            wcscpy_s(currentDoc->title, MAX_PATH_COUNT, path);

            currentMode = MODE_NORMAL;
            paintContentCursor = true;
            paintStatusCursor = false;
            statusPrompt = false;
            SetStatusLineNormal();
            break;
        }

        case RESULT_FILE_ERROR:
        {
            SetStatusInvalidCommand(statusFileError);
            break;
        }

        case RESULT_MEMORY_ERROR:
        {
            SetStatusInvalidCommand(statusOutOfMemory);
            break;
        }

        default:
        {
            DestroyDoc(currentDoc);
            currentDoc = fileDoc;

            currentMode = MODE_NORMAL;
            paintContentCursor = true;
            paintStatusCursor = false;
            statusPrompt = false;
            SetStatusLineNormal();
            break;
        }
    }
}

void ExecuteCommandWrite(const wchar_t * args, ulong argsLength) {
    const wchar_t statusNoName[] = L"No file name!";
    const wchar_t statusFileLocked[] = L"File locked!";
    const wchar_t statusFileModified[] = L"File was modified by another program!";
    const wchar_t statusFileRemoved[] = L"File was deleted!";
    const wchar_t statusFileWriteError[] = L"Could not write file.";
    const wchar_t statusArgsInvalid[] = L"Command args invalid!";
    const wchar_t statusPathTooLong[] = L"Path too long!";
    const wchar_t statusFileExists[] = L"File already exists!";

    ulong i = 0;

    while (i != argsLength && iswspace(args[i])) {
        i++;
    }
    
    bool overwrite;
    if (i != argsLength && args[i] == L'!') {
        overwrite = true;
        i++;
    } else {
        overwrite = false;
    }

    while (i != argsLength && iswspace(args[i])) {
        i++;
    }

    if (i == argsLength) {
        if (currentDoc->title[0] == L'\0') {
            SetStatusInvalidCommand(statusNoName);
            return;
        } else {
            ResultCode resultCode = WriteDoc(currentDoc, nullptr, overwrite);
            switch (resultCode) {
                case RESULT_FILE_LOCKED:
                {
                    SetStatusInvalidCommand(statusFileLocked);
                    return;
                }

                case RESULT_FILE_EXISTS:
                {
                    SetStatusInvalidCommand(statusFileModified);
                    return;
                }

                case RESULT_FILE_NOT_FOUND:
                {
                    SetStatusInvalidCommand(statusFileRemoved);
                    return;
                }

                case RESULT_FILE_ERROR:
                {
                    SetStatusInvalidCommand(statusFileWriteError);
                    return;
                }
            }

            currentDoc->modified = false;
        }
    } else {
        ulong j;
        if (args[i] == L'\"') {
            i++;
            j = i;
            while (j != argsLength && args[j] != L'\"') {
                j++;
            }
            if (j == argsLength) {
                SetStatusInvalidCommand(statusArgsInvalid);
                return;
            }
        } else {
            j = i;
            while (j != argsLength && !iswspace(args[j])) {
                j++;
            }
        }

        for (ulong k = j; k != argsLength; k++) {
            if (!iswspace(args[k])) {
                SetStatusInvalidCommand(statusArgsInvalid);
                return;
            }
        }

        if (j - i > MAX_PATH_COUNT) {
            SetStatusInvalidCommand(statusPathTooLong);
            return;
        }
        wchar_t path[MAX_PATH_COUNT];
        wcsncpy_s(path, MAX_PATH_COUNT, args + i, j - i);

        ResultCode resultCode = WriteDoc(currentDoc, path, overwrite);
        switch (resultCode) {
            case RESULT_FILE_LOCKED:
            {
                SetStatusInvalidCommand(statusFileLocked);
                return;
            }

            case RESULT_FILE_EXISTS:
            {
                SetStatusInvalidCommand(statusFileExists);
                return;
            }

            case RESULT_FILE_NOT_FOUND:
            {
                SetStatusInvalidCommand(statusFileRemoved);
                return;
            }

            case RESULT_FILE_ERROR:
            {
                SetStatusInvalidCommand(statusFileWriteError);
                return;
            }
        }

        currentDoc->modified = false;
        wcscpy_s(currentDoc->title, MAX_PATH_COUNT, path);
    }

    currentMode = MODE_NORMAL;
    paintContentCursor = true;
    paintStatusCursor = false;
    statusPrompt = false;
    SetStatusLineNormal();
}

void ExecuteCommandNew(const wchar_t * args, ulong argsLength) {
    ulong i = 0;
    while (i != argsLength && iswspace(args[i])) {
        i++;
    }

    if (i != argsLength && args[i] == L'!') {
        i++;
    } else if (currentDoc->modified) {
        SetStatusInvalidCommand(L"Current document has unsaved changes!");
        return;
    }

    for (ulong j = i; j != argsLength; j++) {
        if (!iswspace(args[j])) {
            SetStatusInvalidCommand(L"Invalid command args!");
            return;
        }
    }

    DestroyDoc(currentDoc);
    currentDoc = CreateEmptyDoc();

    currentMode = MODE_NORMAL;
    paintContentCursor = true;
    paintStatusCursor = false;
    statusPrompt = false;
    SetStatusLineNormal();
}

static bool WcIsAsciiAlpha(wchar_t c) {
    return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
}

static bool WcIsCName(wchar_t c) {
    return WcIsAsciiAlpha(c) || iswdigit(c) || c == L'_';
}

void ExecuteCommand(const wchar_t * commandLine, ulong commandLength) {
    ulong i = 0;
    while (i != commandLength && iswspace(commandLine[i])) {
        i++;
    }
    if (i == commandLength) {
        return;
    }
    
    if (!(WcIsAsciiAlpha(commandLine[i]) || commandLine[i] == L'_')) {
        SetStatusInvalidCommand(L"Invalid command!");
    }
    ulong j = i;
    do {
        j++;
    } while (j != commandLength && WcIsCName(commandLine[j]));
    ulong initLength = j - i;

    const wchar_t editCommand[] = L"edit";
    const wchar_t writeCommand[] = L"write";
    const wchar_t newCommand[] = L"enew";

    if (wcsncmp(commandLine + i, editCommand, initLength) == 0 && initLength == wcslen(editCommand)) {
        ExecuteCommandEdit(commandLine + j, commandLength - j);
    } else if (wcsncmp(commandLine + i, writeCommand, initLength) == 0 && initLength == wcslen(writeCommand)) {
        ExecuteCommandWrite(commandLine + j, commandLength - j);
    } else if (wcsncmp(commandLine + i, newCommand, initLength) == 0 && initLength == wcslen(newCommand)) {
        ExecuteCommandNew(commandLine + j, commandLength - j);
    } else {
        SetStatusInvalidCommand(L"Unknown command!");
    }
}

void ProcessCommandCharInput(wchar_t c) {
    switch (c) {
        case 0x1b: // Esc
        {
            currentMode = MODE_NORMAL;
            paintContentCursor = true;
            paintStatusCursor = false;
            SetStatusLineNormal();
            break;
        }

        case L'\b':
        {
            if (statusCursorChar == 1) {
                if (statusLength == 1) {
                    currentMode = MODE_NORMAL;
                    paintContentCursor = true;
                    paintStatusCursor = false;
                    SetStatusLineNormal();
                }
            } else {
                statusCursorChar--;
                ulong shiftEnd = statusLength - 1;
                for (ulong i = statusCursorChar; i != shiftEnd; i++) {
                    statusLine[i] = statusLine[i + 1];
                }
                statusLength--;
            }
            break;
        }

        case L'\r':
        {
            ExecuteCommand(statusLine + 1, statusLength - 1);
            break;
        }

        default:
        {
            if (iswcntrl(c)) {
                break;
            }

            if (statusLength == MAX_STATUS_COUNT - 1) {
                break;
            }
            statusLength++;
            for (ulong i = statusLength - 1; i != statusCursorChar; i--) {
                statusLine[i] = statusLine[i - 1];
            }
            statusLine[statusCursorChar++] = c;
            break;
        }
    }
}

HDC bitmapDeviceContext;
ushort bitmapWidth;
ushort bitmapHeight;

long lineHeight;
long avgCharWidth;

static void PaintCursorLine(const RECT * textRect, const wchar_t * text, ulong length, ulong charIndex) {
    SIZE extent;

    RECT lineRect = *textRect;

    const wchar_t * currentText = text;
    ulong currentLength = length;
    const wchar_t * nextTabPos = MkWcsFindCharConst(currentText, currentLength, L'\t');
    long cursorOffset = charIndex;
    while (nextTabPos) {
        ulong nextTabOffset = static_cast<ulong>(nextTabPos - currentText);
        if (cursorOffset < 0 || static_cast<ulong>(cursorOffset) >= nextTabOffset) {
            GetTextExtentPoint32W(
                bitmapDeviceContext,
                currentText,
                nextTabOffset,
                &extent);
            DrawTextExW(
                bitmapDeviceContext,
                const_cast<wchar_t *>(currentText),
                nextTabOffset,
                &lineRect,
                DT_NOCLIP | DT_NOPREFIX,
                nullptr);
            lineRect.left += extent.cx;
            if (lineRect.left >= lineRect.right) {
                break;
            }

            if (cursorOffset == nextTabOffset) {
                RECT cursorRect;
                cursorRect.left = lineRect.left;
                cursorRect.top = lineRect.top;
                cursorRect.right = lineRect.left + avgCharWidth;
                cursorRect.bottom = lineRect.top + lineHeight;
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
                &lineRect,
                DT_NOCLIP | DT_NOPREFIX,
                nullptr);
            lineRect.left += extent.cx;
            if (lineRect.left >= lineRect.right) {
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
                const_cast<wchar_t *>(currentText),
                cursorOffset,
                &lineRect,
                DT_NOCLIP | DT_NOPREFIX,
                nullptr);
            lineRect.left += extent.cx;
            if (lineRect.left >= lineRect.right) {
                break;
            }

            GetTextExtentPoint32W(
                bitmapDeviceContext,
                &currentText[cursorOffset],
                1,
                &extent);
            RECT cursorRect;
            cursorRect.left = lineRect.left;
            cursorRect.top = lineRect.top;
            cursorRect.right = lineRect.left + extent.cx;
            cursorRect.bottom = lineRect.top + lineHeight;
            FillRect(bitmapDeviceContext, &cursorRect, cursorBrush);

            GetTextExtentPoint32W(
                bitmapDeviceContext,
                currentText + cursorOffset,
                nextTabOffset - cursorOffset,
                &extent);
            DrawTextExW(
                bitmapDeviceContext,
                const_cast<wchar_t *>(currentText + cursorOffset),
                nextTabOffset - cursorOffset,
                &lineRect,
                DT_NOCLIP | DT_NOPREFIX,
                nullptr);
            lineRect.left += extent.cx;
            if (lineRect.left >= lineRect.right) {
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
                &lineRect,
                DT_NOCLIP | DT_NOPREFIX,
                nullptr);
            lineRect.left += extent.cx;
            if (lineRect.left >= lineRect.right) {
                break;
            }
        }

        currentText = nextTabPos + 1;
        nextTabOffset++;
        currentLength -= nextTabOffset;
        cursorOffset -= nextTabOffset;
        nextTabPos = MkWcsFindCharConst(currentText, currentLength, L'\t');
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
                const_cast<wchar_t *>(currentText),
                currentLength,
                &lineRect,
                DT_NOCLIP | DT_NOPREFIX,
                nullptr);
            lineRect.left += extent.cx;
            if (lineRect.left < lineRect.right) {
                RECT cursorRect;
                cursorRect.left = lineRect.left;
                cursorRect.top = lineRect.top;
                cursorRect.right = lineRect.left + avgCharWidth;
                cursorRect.bottom = lineRect.top + lineHeight;
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
                const_cast<wchar_t *>(currentText),
                cursorOffset,
                &lineRect,
                DT_NOCLIP | DT_NOPREFIX,
                nullptr);
            lineRect.left += extent.cx;
            if (lineRect.left < lineRect.right) {
                GetTextExtentPoint32W(
                    bitmapDeviceContext,
                    currentText + cursorOffset,
                    1,
                    &extent);
                RECT cursorRect;
                cursorRect.left = lineRect.left;
                cursorRect.top = lineRect.top;
                cursorRect.right = lineRect.left + extent.cx;
                cursorRect.bottom = lineRect.top + lineHeight;
                FillRect(bitmapDeviceContext, &cursorRect, cursorBrush);

                DrawTextExW(
                    bitmapDeviceContext,
                    const_cast<wchar_t *>(currentText + cursorOffset),
                    currentLength - cursorOffset,
                    &lineRect,
                    DT_NOCLIP | DT_NOPREFIX,
                    nullptr);
            }
        } else {
            DrawTextExW(
                bitmapDeviceContext,
                const_cast<wchar_t *>(currentText),
                currentLength,
                &lineRect,
                DT_NOCLIP | DT_NOPREFIX,
                nullptr);
        }
    }
}

static void PaintLine(const RECT * textRect, const wchar_t * text, ulong length) {
    RECT lineRect = *textRect;

    const wchar_t * currentText = text;
    ulong currentLength = length;
    const wchar_t * nextTabPos = MkWcsFindCharConst(currentText, currentLength, L'\t');
    while (nextTabPos) {
        SIZE extent;
        ulong nextTabOffset = static_cast<ulong>(nextTabPos - currentText);

        GetTextExtentPoint32W(
            bitmapDeviceContext,
            currentText,
            nextTabOffset,
            &extent);
        DrawTextExW(
            bitmapDeviceContext,
            const_cast<wchar_t *>(currentText),
            nextTabOffset,
            &lineRect,
            DT_NOCLIP | DT_NOPREFIX,
            nullptr);
        lineRect.left += extent.cx;
        if (lineRect.left >= lineRect.right) {
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
            &lineRect,
            DT_NOCLIP | DT_NOPREFIX,
            nullptr);
        lineRect.left += extent.cx;
        if (lineRect.left >= lineRect.right) {
            break;
        }

        nextTabOffset++;
        currentText = nextTabPos + 1;
        currentLength -= nextTabOffset;
        nextTabPos = MkWcsFindCharConst(currentText, currentLength, L'\t');
    }

    if (!nextTabPos) {
        DrawTextExW(
            bitmapDeviceContext,
            const_cast<wchar_t *>(currentText),
            currentLength,
            &lineRect,
            DT_NOCLIP | DT_NOPREFIX,
            nullptr);
    }
}

static void Paint(Doc * doc) {
    doc->lastPaintLineCount = 0;

    if (bitmapWidth == 0 || bitmapHeight == 0) {
        return;
    }

    RECT paintRect;
    paintRect.left = 0;
    paintRect.top = 0;
    paintRect.right = bitmapWidth;
    paintRect.bottom = bitmapHeight;
    FillRect(bitmapDeviceContext, &paintRect, backgroundBrush);

    SetTextColor(bitmapDeviceContext, config.textColor);
    SetBkMode(bitmapDeviceContext, TRANSPARENT);

    ulong paintLineCount = bitmapHeight / lineHeight;
    if (paintLineCount == 0) {
        return;
    }
    paintLineCount--; // space for status line

    //---------------------------
    // Paint Working Folder Line

    if (paintLineCount != 0) {
        paintRect.bottom = lineHeight;

        wchar_t workingFolderLine[MAX_PATH + 32];
        swprintf_s(workingFolderLine, MAX_PATH + 32, L"Working Folder: %s", workingFolderPath);

        DrawTextExW(
            bitmapDeviceContext,
            workingFolderLine,
            -1,
            &paintRect,
            DT_NOCLIP | DT_NOPREFIX,
            nullptr);

        paintLineCount--;
    }

    //------------------------
    // Paint Doc Title Line

    if (paintLineCount != 0) {
        paintRect.top += lineHeight;
        paintRect.bottom += lineHeight;
        FillRect(bitmapDeviceContext, &paintRect, docTitleBackgroundBrush);

        wchar_t docTitleLine[MAX_PATH_COUNT + 32];
        if (doc->title[0] == L'\0') {
            wcscpy_s(docTitleLine, MAX_PATH_COUNT + 32, L"<UNTITLED>");
        } else {
            wcscpy_s(docTitleLine, MAX_PATH_COUNT + 32, doc->title);
            if (doc->timestamp == 0) {
                wcscat_s(docTitleLine, MAX_PATH_COUNT + 32, L" <NEW>");
            }
        }
        if (doc->modified) {
            wcscat_s(docTitleLine, MAX_PATH_COUNT + 32, L" (modified)");
        }

        DrawTextExW(
            bitmapDeviceContext,
            docTitleLine,
            -1,
            &paintRect,
            DT_NOCLIP | DT_NOPREFIX,
            nullptr);

        paintLineCount--;
    }

    //-----------------
    // Paint Content

    paintRect.top += lineHeight;
    paintRect.bottom = bitmapHeight - lineHeight;

    if (doc->cursorLineIndex < doc->topPaintLineIndex) {
        doc->topPaintLineIndex = doc->cursorLineIndex;
    }
    ulong endPaintLineIndex = doc->topPaintLineIndex + paintLineCount;
    if (doc->cursorLineIndex >= endPaintLineIndex) {
        doc->topPaintLineIndex += doc->cursorLineIndex - endPaintLineIndex + 1;
        endPaintLineIndex = doc->cursorLineIndex + 1;
    } else if (doc->topPaintLineIndex != 0 && endPaintLineIndex > doc->content.lineCount) {
        ulong diff = endPaintLineIndex - doc->content.lineCount;
        if (diff > doc->topPaintLineIndex) {
            doc->topPaintLineIndex = 0;
        } else {
            doc->topPaintLineIndex -= diff;
        }
    }

    for (ulong i = doc->topPaintLineIndex; i != doc->content.lineCount; i++) {
        if (paintRect.bottom - paintRect.top < lineHeight) {
            break;
        }

        DocLine * line = &doc->content.lines[i];
        if (i == doc->cursorLineIndex && paintContentCursor) {
            PaintCursorLine(&paintRect, line->chars, line->length, doc->cursorCharIndex);
        } else {
            PaintLine(&paintRect, line->chars, line->length);
        }

        paintRect.left = 0;
        paintRect.top += lineHeight;
        doc->lastPaintLineCount++;
    }

    //---------------------
    // Paint Status Line

    paintRect.top = bitmapHeight - lineHeight;
    paintRect.bottom = bitmapHeight;
 
    if (statusPrompt) {
        FillRect(bitmapDeviceContext, &paintRect, promptBackgroundBrush);
        SetTextColor(bitmapDeviceContext, config.promptTextColor);
    } else {
        FillRect(bitmapDeviceContext, &paintRect, statusBackgroundBrush);
    }

    if (paintStatusCursor) {
        PaintCursorLine(&paintRect, statusLine, statusLength, statusCursorChar);
    } else {
        DrawTextExW(
            bitmapDeviceContext,
            statusLine,
            statusLength,
            &paintRect,
            DT_NOCLIP | DT_NOPREFIX,
            nullptr);
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
                HFONT oldFont = static_cast<HFONT>(SelectObject(bitmapDeviceContext, font));
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
            HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(bitmapDeviceContext, bitmap));
            if (oldBitmap) {
                DeleteObject(oldBitmap);
            }

            Paint(currentDoc);
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paintStruct;
            HDC deviceContext = BeginPaint(window, &paintStruct);

            BitBlt(
                deviceContext,
                0, 0, bitmapWidth, bitmapHeight,
                bitmapDeviceContext,
                0, 0,
                SRCCOPY);

            EndPaint(window, &paintStruct);
            return 0;
        }

        case WM_CLOSE:
        {
            if (currentDoc->modified) {
                int promptResult = MessageBoxW(
                    window,
                    L"One or more documents contain unsaved changes. Close anyway?",
                    L"Warning",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (promptResult != IDYES) {
                    return 0;
                }
            }
            DestroyWindow(window);
            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }

        case WM_CHAR:
        {
            wchar_t c = static_cast<wchar_t>(wparam);

            switch (currentMode) {
                case MODE_NORMAL:
                {
                    ProcessNormalCharInput(c);
                    Paint(currentDoc);
                    InvalidateRect(window, nullptr, false);
                    break;
                }

                case MODE_COMMAND:
                {
                    ProcessCommandCharInput(c);
                    Paint(currentDoc);
                    InvalidateRect(window, nullptr, false);
                    break;
                }

                case MODE_INSERT:
                {
                    if (c == 0x1b) { // Esc
                        currentMode = MODE_NORMAL;
                        if (currentDoc->cursorCharIndex != 0 && currentDoc->cursorCharIndex == currentDoc->content.lines[currentDoc->cursorLineIndex].length) {
                            currentDoc->cursorCharIndex--;
                        }
                    } else {
                        ProcessDocCharInput(currentDoc, c);
                    }
                    SetStatusLineNormal();
                    Paint(currentDoc);
                    InvalidateRect(window, nullptr, false);
                    break;
                }
            }

            return 0;
        }

        default:
        {
            return DefWindowProcW(window, message, wparam, lparam);
        }
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, wchar_t * commandLine, int showCommand) {
    ConfigInit(&config);

    wchar_t * appDataFolderPath;
    SHGetKnownFolderPath(
        FOLDERID_RoamingAppData,
        KF_FLAG_DEFAULT,
        nullptr,
        &appDataFolderPath);
    wchar_t configFilePath[MAX_PATH];
    swprintf_s(configFilePath, MAX_PATH, L"%s\\MKedit\\Config.cfg", appDataFolderPath);
    CoTaskMemFree(appDataFolderPath);
    LoadConfigFile(configFilePath);

    tabSpaces = static_cast<wchar_t *>(malloc(config.tabWidth * sizeof(wchar_t)));
    for (ulong i = 0; i != config.tabWidth; i++) {
        tabSpaces[i] = L' ';
    }

    textBrush = CreateSolidBrush(config.textColor);
    backgroundBrush = CreateSolidBrush(config.backgroundColor);
    cursorBrush = CreateSolidBrush(config.cursorColor);
    statusBackgroundBrush = CreateSolidBrush(config.statusBackgroundColor);
    docTitleBackgroundBrush = CreateSolidBrush(config.docTitleBackgroundColor);
    promptTextBrush = CreateSolidBrush(config.promptTextColor);
    promptBackgroundBrush = CreateSolidBrush(config.promptBackgroundColor);

    currentDoc = CreateEmptyDoc();
    GetCurrentDirectoryW(MAX_PATH, workingFolderPath);
    SetStatusLineNormal();

    const wchar_t windowClassName[] = L"MKedit";

    WNDCLASSEXW windowClass = {};
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