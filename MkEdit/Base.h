#pragma once

#include <limits.h>
#include <stdint.h>

#include "Generated/ConfigGen.h"

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;

#define MAX_LINE_LENGTH USHRT_MAX
#define MAX_LINE_COUNT ULONG_MAX
#define MAX_PATH_COUNT 260

enum ResultCode {
    RESULT_OK,
    RESULT_MEMORY_ERROR,
    RESULT_LIMIT_REACHED,
    RESULT_FILE_LOCKED,
    RESULT_FILE_NOT_FOUND,
    RESULT_FILE_ERROR,
    RESULT_FILE_EXISTS,
};

extern Config config;

struct DocLine {
    ulong length;
    ulong capacity;
    wchar_t * chars;
};

struct DocContent {
    ulong lineCount;
    ulong lineCapacity;
    DocLine * lines;
};

struct Doc {
    DocContent content;
    ulong cursorLineIndex;
    ulong cursorCharIndex;
    ulong lastCursorColIndex;
    ulong topPaintLineIndex;
    ulong lastPaintLineCount;
    bool modified;
    uint64_t timestamp;
    wchar_t title[MAX_PATH_COUNT];
};

#define DOCLINE_INIT_CAPACITY 4
#define DOCLINES_GROW_COUNT 16

// Creates a document containing one empty line.
// Returns NULL on memory allocation failure.
Doc * CreateEmptyDoc();

// Frees a document.
void DestroyDoc(Doc * doc);

// Processes character input into the document.
// Returns:
// - RESULT_OK
// - RESULT_MEMORY_ERROR
// - RESULT_LIMIT_REACHED
ResultCode ProcessDocCharInput(Doc * doc, wchar_t c);

// Recalculates the actual cursor column.
void ResetColIndex(Doc * doc);

// Try to set the actual cursor column to the previous position.
void ApplyColIndex(Doc * doc, bool plusOne);