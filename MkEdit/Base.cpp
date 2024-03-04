#include <stdlib.h>
#include <wchar.h>

#include "Base.h"

Doc * CreateEmptyDoc() {
    Doc * doc = (Doc *)malloc(sizeof(Doc));
    if (!doc) {
        return nullptr;
    }

    doc->content.lineCount = 1;
    doc->content.lineCapacity = DOCLINES_GROW_COUNT;
    doc->content.lines = (DocLine *)malloc(doc->content.lineCapacity * sizeof(DocLine));
    if (!doc->content.lines) {
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

    DocLine * line = &doc->content.lines[0];
    line->length = 0;
    line->capacity = DOCLINE_INIT_CAPACITY;
    line->chars = (wchar_t *)malloc(line->capacity * sizeof(wchar_t));
    if (!line->chars) {
        free(doc->content.lines);
        free(doc);
        return nullptr;
    }

    return doc;
}

void DestroyDoc(Doc * doc) {
    if (doc) {
        if (doc->content.lines) {
            for (size_t i = 0; i != doc->content.lineCount; i++) {
                if (doc->content.lines[i].chars) {
                    free(doc->content.lines[i].chars);
                }
            }
            free(doc->content.lines);
        }
        free(doc);
    }
}

void ResetColIndex(Doc * doc) {
    wchar_t * chars = doc->content.lines[doc->cursorLineIndex].chars;
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
            DocLine * line = &doc->content.lines[doc->cursorLineIndex];

            if (config.expandTabs) {
                if (line->length > MAX_LINE_LENGTH - config.tabWidth) {
                    return RESULT_LIMIT_REACHED;
                }

                ulong newLength = line->length + config.tabWidth;
                ulong newCapacity = line->capacity;
                bool grow = false;
                while (newCapacity < newLength) {
                    newCapacity *= 2;
                    grow = true;
                }
                if (grow) {
                    wchar_t * newChars = (wchar_t *)realloc(line->chars, newCapacity * sizeof(wchar_t));
                    if (!newChars) {
                        return RESULT_MEMORY_ERROR;
                    }
                    line->capacity = newCapacity;
                    line->chars = newChars;
                }

                line->length = newLength;
                size_t shiftEnd = doc->cursorCharIndex + config.tabWidth - 1;
                for (size_t i = line->length - 1; i != shiftEnd; i--) {
                    line->chars[i] = line->chars[i - config.tabWidth];
                }
                for (size_t i = 0; i != config.tabWidth; i++) {
                    line->chars[doc->cursorCharIndex + i] = L' ';
                }
                doc->cursorCharIndex += config.tabWidth;
            } else {
                if (line->length == MAX_LINE_LENGTH) {
                    return RESULT_LIMIT_REACHED;
                }

                if (line->length == line->capacity) {
                    ulong newCapacity = line->capacity * 2;
                    wchar_t * newChars = (wchar_t *)realloc(line->chars, newCapacity * sizeof(wchar_t));
                    if (!newChars) {
                        return RESULT_MEMORY_ERROR;
                    }
                    line->capacity = newCapacity;
                    line->chars = newChars;
                }

                line->length++;
                for (size_t i = line->length - 1; i != doc->cursorCharIndex; i--) {
                    line->chars[i] = line->chars[i - 1];
                }
                line->chars[doc->cursorCharIndex] = L'\t';
                doc->cursorCharIndex++;
            }

            ResetColIndex(doc);
            doc->modified = true;
            break;
        }

        case L'\r':
        {
            if (doc->content.lineCount == MAX_LINE_COUNT) {
                return RESULT_LIMIT_REACHED;
            }

            if (doc->content.lineCount == doc->content.lineCapacity) {
                ulong newCapacity = doc->content.lineCapacity + DOCLINES_GROW_COUNT;
                DocLine * newLines = (DocLine *)realloc(doc->content.lines, newCapacity * sizeof(DocLine));
                if (!newLines) {
                    return RESULT_MEMORY_ERROR;
                }
                doc->content.lineCapacity = newCapacity;
                doc->content.lines = newLines;
            }

            doc->content.lineCount++;
            size_t shiftEnd = doc->cursorLineIndex + 1;
            for (size_t i = doc->content.lineCount - 1; i != shiftEnd; i--) {
                doc->content.lines[i] = doc->content.lines[i - 1];
            }

            DocLine * curLine = &doc->content.lines[doc->cursorLineIndex];
            DocLine * nextLine = curLine + 1;

            nextLine->length = curLine->length - doc->cursorCharIndex;
            nextLine->capacity = DOCLINE_INIT_CAPACITY;
            while (nextLine->capacity < nextLine->length) {
                nextLine->capacity *= 2;
            }
            nextLine->chars = (wchar_t *)malloc(nextLine->capacity * sizeof(wchar_t));
            if (!nextLine->chars) {
                size_t undoEnd = doc->content.lineCount - 1;
                for (size_t i = doc->cursorLineIndex + 1; i != undoEnd; i++) {
                    doc->content.lines[i] = doc->content.lines[i + 1];
                }
                doc->content.lineCount--;
                return RESULT_MEMORY_ERROR;
            }

            for (size_t i = 0; i != nextLine->length; i++) {
                nextLine->chars[i] = curLine->chars[doc->cursorCharIndex + i];
            }
            curLine->length = doc->cursorCharIndex;
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
                    DocLine * curLine = &doc->content.lines[doc->cursorLineIndex];
                    DocLine * prevLine = curLine - 1;

                    if (curLine->length > MAX_LINE_LENGTH - prevLine->length) {
                        return RESULT_LIMIT_REACHED;
                    }

                    ulong newLength = prevLine->length + curLine->length;
                    ulong newCapacity = prevLine->capacity;
                    bool grow = false;
                    while (newCapacity < newLength) {
                        newCapacity *= 2;
                        grow = true;
                    }
                    if (grow) {
                        wchar_t * newChars = (wchar_t *)realloc(prevLine->chars, newCapacity * sizeof(wchar_t));
                        if (!newChars) {
                            return RESULT_MEMORY_ERROR;
                        }
                        prevLine->capacity = newCapacity;
                        prevLine->chars = newChars;
                    }

                    doc->cursorLineIndex--;
                    doc->cursorCharIndex = prevLine->length;
                    prevLine->length = newLength;
                    for (size_t i = 0; i != curLine->length; i++) {
                        prevLine->chars[doc->cursorCharIndex + i] = curLine->chars[i];
                    }

                    free(curLine->chars);
                    size_t shiftEnd = doc->content.lineCount - 1;
                    for (size_t i = doc->cursorLineIndex + 1; i != shiftEnd; i++) {
                        doc->content.lines[i] = doc->content.lines[i + 1];
                    }
                    doc->content.lineCount--;

                    ResetColIndex(doc);
                    doc->modified = true;
                }
            } else {
                DocLine * line = &doc->content.lines[doc->cursorLineIndex];
                for (size_t i = doc->cursorCharIndex; i != line->length; i++) {
                    line->chars[i - 1] = line->chars[i];
                }
                doc->cursorCharIndex--;
                line->length--;
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

            DocLine * line = &doc->content.lines[doc->cursorLineIndex];
            if (line->length == MAX_LINE_LENGTH) {
                return RESULT_LIMIT_REACHED;
            }

            if (line->length == line->capacity) {
                ulong newCapacity = line->capacity * 2;
                wchar_t * newChars = (wchar_t *)realloc(line->chars, newCapacity * sizeof(wchar_t));
                if (!newChars) {
                    return RESULT_MEMORY_ERROR;
                }
                line->capacity = newCapacity;
                line->chars = newChars;
            }

            line->length++;
            for (size_t i = line->length - 1; i != doc->cursorCharIndex; i--) {
                line->chars[i] = line->chars[i - 1];
            }
            line->chars[doc->cursorCharIndex] = c;
            doc->cursorCharIndex++;
            ResetColIndex(doc);
            doc->modified = true;
            break;
        }
    }

    return RESULT_OK;
}

void ApplyColIndex(Doc * doc, bool plusOne) {
    DocLine * line = &doc->content.lines[doc->cursorLineIndex];

    ulong end = line->length;
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

        if (line->chars[doc->cursorCharIndex] == L'\t') {
            paintIndex += config.tabWidth;
        } else {
            paintIndex++;
        }
    }
}