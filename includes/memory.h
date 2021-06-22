#ifndef ms_memory_h
#define ms_memory_h

#include "../includes/vm.h"
#include "../includes/common.h"
// Macros //

#define THRESHOLD 8
#define GROW_CAPACITY(capacity) \
((capacity) < THRESHOLD ? THRESHOLD : (capacity) * 2)

#define GROW_ARRAY(datatype, array, oldS, newS) \
(datatype*)reallocateArray(array, (oldS) * sizeof(datatype), (newS) * sizeof(datatype))

#define FREE_ARRAY(datatype, array, oldS) reallocateArray(array, (oldS) * sizeof(datatype), 0)

#define ALLOCATE(vmptr, type, count) \
    (type*)reallocate(vmptr, NULL, 0, sizeof(type) * count)

#define ALLOCATE_ARRAY(type, count) \
    (type*)reallocateArray(NULL, 0, sizeof(type) * count)

void* reallocate(VM* vm, void* array, size_t oldSize, size_t newSize);
void* reallocateArray(void* array, size_t oldSize, size_t newSize);

#endif


