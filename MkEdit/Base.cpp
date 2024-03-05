#include <stdlib.h>
#include <wchar.h>

#include "Base.h"

Doc * CreateEmptyDoc() {
    Doc * doc = (Doc *)malloc(sizeof(Doc));
    if (!doc) {
        return nullptr;
    }

    doc->lines.Init(DOCLINES_GROW_COUNT);
    MkList2<wchar_t> * line = doc->lines.Insert(ULONG_MAX, 1);
    if (!line) {
        free(doc);
        return nullptr;
    }
    line->Init(DOCLINE_INIT_CAPACITY);
    if (!line->SetCapacity(line->growCount)) {
        doc->lines.Clear();
        free(doc);
        return nullptr;
    }

    doc->cursorLineIndex = 0;
    doc->cursorCharIndex = 0;
    doc->lastCursorColIndex = 0;
    doc->topPaintLineIndex = 0;
    doc->lastPaintLineCount = 0;
    doc->modified = false;
    doc->timestamp = 0;
    doc->title[0] = L'\0';

    return doc;
}

void DestroyDoc(Doc * doc) {
    if (doc) {
        if (doc->lines.elems) {
            for (ulong i = 0; i != doc->lines.count; i++) {
                doc->lines.elems[i].Clear();
            }
            doc->lines.Clear();
        }
        free(doc);
    }
}

void ResetColIndex(Doc * doc) {
    wchar_t * chars = doc->lines.elems[doc->cursorLineIndex].elems;
    doc->lastCursorColIndex = 0;
    for (size_t i = 0; i != doc->cursorCharIndex; i++) {
        if (chars[i] == L'\t') {
            doc->lastCursorColIndex += config.tabWidth;
        } else {
            doc->lastCursorColIndex++;
        }
    }
}

ResultCode ProcessDocCharInput(Doc * doc, wchar_t c) {
    switch (c) {
        case L'\t':
        {
            MkList2<wchar_t> * line = &doc->lines.elems[doc->cursorLineIndex];
            if (config.expandTabs) {
                if (line->count > MAX_LINE_LENGTH - config.tabWidth) {
                    return RESULT_LIMIT_REACHED;
                }

                ulong newLength = line->count + config.tabWidth;
                ulong newCapacity = line->capacity;
                bool grow = false;
                while (newCapacity < newLength) {
                    newCapacity *= 2;
                    grow = true;
                }
                if (grow && !line->SetCapacity(newCapacity)) {
                    return RESULT_MEMORY_ERROR;
                }

                wchar_t * chars = line->Insert(doc->cursorCharIndex, config.tabWidth);
                for (ulong i = 0; i != config.tabWidth; i++) {
                    chars[i] = L' ';
                }
                doc->cursorCharIndex += config.tabWidth;
            } else {
                if (line->count == MAX_LINE_LENGTH) {
                    return RESULT_LIMIT_REACHED;
                }

                if (line->count == line->capacity && !line->SetCapacity(line->capacity * 2)) {
                    return RESULT_MEMORY_ERROR;
                }
                wchar_t * newChar = line->Insert(doc->cursorCharIndex++, 1);
                *newChar = L'\t';
            }

            ResetColIndex(doc);
            doc->modified = true;
            break;
        }

        case L'\r':
        {
            if (doc->lines.count == MAX_LINE_COUNT) {
                return RESULT_LIMIT_REACHED;
            }

            MkList2<wchar_t> * newLine = doc->lines.Insert(doc->cursorLineIndex + 1, 1);
            if (!newLine) {
                return RESULT_MEMORY_ERROR;
            }
            newLine->Init(DOCLINE_INIT_CAPACITY);
            MkList2<wchar_t> * curLine = newLine - 1;

            ulong newLineLength = curLine->count - doc->cursorCharIndex;
            ulong newLineCapacity = newLine->growCount;
            while (newLineLength < newLineCapacity) {
                newLineCapacity *= 2;
            }
            if (!newLine->SetCapacity(newLineCapacity)) {
                curLine->Remove(doc->cursorLineIndex + 1, 1);
                return RESULT_MEMORY_ERROR;
            }
            newLine->count = newLineLength;

            for (ulong i = 0; i != newLineLength; i++) {
                newLine->elems[i] = curLine->elems[doc->cursorCharIndex + i];
            }
            doc->cursorLineIndex++;
            doc->cursorCharIndex = 0;
            doc->lastCursorColIndex = 0;
            doc->modified = true;
            break;
        }

        case L'\b':
        {
            if (doc->cursorCharIndex == 0) {
                if (doc->cursorLineIndex != 0) {
                    MkList2<wchar_t> * curLine = &doc->lines.elems[doc->cursorLineIndex];
                    MkList2<wchar_t> * prevLine = curLine - 1;

                    if (curLine->count > MAX_LINE_LENGTH - prevLine->count) {
                        return RESULT_LIMIT_REACHED;
                    }

                    ulong newLength = prevLine->count + curLine->count;
                    ulong newCapacity = prevLine->capacity;
                    bool grow = false;
                    while (newCapacity < newLength) {
                        newCapacity *= 2;
                        grow = true;
                    }
                    if (grow && !prevLine->SetCapacity(newCapacity)) {
                        return RESULT_MEMORY_ERROR;
                    }

                    doc->cursorLineIndex--;
                    doc->cursorCharIndex = prevLine->count;
                    prevLine->count = newLength;
                    for (ulong i = 0; i != curLine->count; i++) {
                        prevLine->elems[doc->cursorCharIndex + i] = curLine->elems[i];
                    }

                    curLine->Clear();
                    doc->lines.Remove(doc->cursorLineIndex + 1, 1);

                    ResetColIndex(doc);
                    doc->modified = true;
                }
            } else {
                doc->cursorCharIndex--;
                doc->lines.elems[doc->cursorLineIndex].Remove(doc->cursorCharIndex, 1);
                ResetColIndex(doc);
                doc->modified = true;
            }
            break;
        }

        default:
        {
            if (iswcntrl(c)) {
                return RESULT_OK;
            }

            MkList2<wchar_t> * line = &doc->lines.elems[doc->cursorLineIndex];
            if (line->count == MAX_LINE_LENGTH) {
                return RESULT_LIMIT_REACHED;
            }

            if (line->count == line->capacity && !line->SetCapacity(line->capacity * 2)) {
                return RESULT_MEMORY_ERROR;
            }
            
            wchar_t * newChar = line->Insert(doc->cursorCharIndex++, 1);
            *newChar = c;
            ResetColIndex(doc);
            doc->modified = true;
            break;
        }
    }

    return RESULT_OK;
}

void ApplyColIndex(Doc * doc, bool plusOne) {
    MkList2<wchar_t> * line = &doc->lines.elems[doc->cursorLineIndex];

    ulong end = line->count;
    if (!plusOne && end != 0) {
        end--;
    }

    ulong paintIndex = 0;
    for (doc->cursorCharIndex = 0; doc->cursorCharIndex != end; doc->cursorCharIndex++) {
        if (paintIndex >= doc->lastCursorColIndex) {
            if (paintIndex > doc->lastCursorColIndex) {
                doc->cursorCharIndex--;
            }
            break;
        }

        if (line->elems[doc->cursorCharIndex] == L'\t') {
            paintIndex += config.tabWidth;
        } else {
            paintIndex++;
        }
    }
}