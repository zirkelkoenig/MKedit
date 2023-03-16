#include "DynArray.h"
#include "Platform.h"

unsigned char * DynArrayCreateImpl(size_t elemSize, size_t initCapacity)
{
    DynArrayHeader * header = (DynArrayHeader *)MemAllocImpl(sizeof(DynArrayHeader) + initCapacity * elemSize);
    if (!header) return 0;

    header->count = 0;
    header->capacity = initCapacity;
    header->elemSize = elemSize;
    return DynArrayGetArray(header);
}

int SetMinCapacity(DynArrayHeader ** header, size_t minCapacity)
{
    *header = (DynArrayHeader *)MemReallocImpl(*header, sizeof(DynArrayHeader) + minCapacity * (*header)->elemSize);
    if (!(*header)) return 0;
    (*header)->capacity = minCapacity;
    return 1;
}

int DynArrayIncrement(unsigned char ** array)
{
    DynArrayHeader * header = DynArrayGetHeader(*array);
    if (header->count == header->capacity)
    {
        if (!SetMinCapacity(&header, header->capacity + header->capacity / 2)) return 0;
        *array = DynArrayGetArray(header);
    }
    header->count++;
    return 1;
}

int DynArraySetCountImpl(unsigned char ** array, size_t count)
{
    DynArrayHeader * header = DynArrayGetHeader(*array);
    if (count > header->capacity)
    {
        if (!SetMinCapacity(&header, count)) return 0;
        *array = DynArrayGetArray(header);
    }
    header->count = count;
    return 1;
}