#include <stddef.h>
#include <stdlib.h>
#include "../includes/memory.h"
#include "../includes/gcollect.h"
#include "../includes/debug.h"

void* reallocate(VM* vm, void* array, size_t oldSize, size_t newSize) {
    vm->bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage(vm);
#endif
        if (vm->bytesAllocated > vm->nextGC) {
            collectGarbage(vm);
        }
    }

    if (newSize == 0) {
        free(array);
        return NULL;
    }

    void* result = realloc(array, newSize);
    if (result == NULL) exit(1);

    return result;
}

void* reallocateArray(void* array, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(array);
        return NULL;
    }

    void* result = realloc(array, newSize);
    if (result == NULL) exit(1);
    return result;
}
