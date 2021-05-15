#ifndef ms_memory_h
#define ms_memory_h

#include "../includes/common.h"
// Macros //

#define THRESHOLD 8
#define GROW_CAPACITY(capacity) \
((capacity) < THRESHOLD ? THRESHOLD : (capacity) * 2)

#define GROW_ARRAY(datatype, array, oldS, newS) \
(datatype*)reallocate(array, (oldS) * sizeof(datatype), (newS) * sizeof(datatype))

#define FREE_ARRAY(datatype, array, oldS) reallocate(array, (oldS) * sizeof(datatype), 0)

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * count)

void* reallocate(void* array, size_t oldSize, size_t newSize);

#endif


