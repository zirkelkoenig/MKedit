#ifndef MK_MATH_HEADER
#define MK_MATH_HEADER

#include <stdlib.h>

typedef size_t MkSize;

typedef struct {
    float X;
    float Y;
} MkVector2F;

static inline MkVector2F MkVector2F_Add(MkVector2F a, MkVector2F b) {
    MkVector2F result;
    result.X = a.X + b.X;
    result.Y = a.Y + b.Y;
    return result;
}

static inline MkVector2F MkVector2F_Scale(float alpha, MkVector2F v) {
    MkVector2F result;
    result.X = alpha * v.X;
    result.Y = alpha * v.Y;
    return result;
}

unsigned int MkFactorial(unsigned int n);

static inline long MkMaxL(long a, long b) {
    return a > b ? a : b;
}

void MkGetBezierVertices(MkVector2F* points, MkSize pointCount, MkVector2F* vertices, MkSize vertexCount);

#endif


#ifdef MK_MATH_IMPLEMENTATION

#if !(defined(MK_ALLOC) && defined(MK_REALLOC) && defined(MK_FREE) && defined(MK_MEMCOPY))
#if defined(MK_ALLOC) || defined(MK_REALLOC) || defined(MK_FREE) || defined(MK_MEMCOPY)
#error "You need to either define all or none of the MK header memory macros!"
#endif

#include <stdlib.h>
#include <string.h>

#define MK_ALLOC(size) malloc(size)
#define MK_REALLOC(pointer, size) realloc(pointer, size)
#define MK_FREE(pointer) free(pointer)
#define MK_MEMCOPY(destination, source, size) memcpy(destination, source, size)
#define MK_MEMMOVE(destination, source, size) memmove(destination, source, size)
#endif

unsigned int MkFactorial(unsigned int n) {
    if (n < 2u) return 1u;

    unsigned int result = 1u;
    for (unsigned int i = 2u; i <= n; i++) {
        result *= i;
    }
    return result;
}

void MkGetBezierVertices(MkVector2F* points, MkSize pointCount, MkVector2F* vertices, MkSize vertexCount) {
    MkSize n = pointCount - 1u;
    float interval = 1.0f / (vertexCount + 1u);

    float nFactorial = (float)MkFactorial(n);
    float* combinations = (float*)MK_ALLOC((n + 1u) * sizeof(float));
    for (MkSize i = 0u; i <= n; i++) {
        combinations[i] = nFactorial / (float)(MkFactorial(i) * MkFactorial(n - i));
    }

    for (MkSize j = 0u; j != vertexCount; j++) {
        float t = (j + 1u) * interval;
        float tComplement = 1.0f - t;

        MkVector2F vertex = { 0 };
        for (MkSize i = 0u; i <= n; i++) {
            float factor = powf(t, i) * powf(tComplement, n - i);
            vertex = MkVector2F_Add(vertex, MkVector2F_Scale(combinations[i] * factor, points[i]));
        }
        vertices[j] = vertex;
    }

    MK_FREE(combinations);
}

#endif