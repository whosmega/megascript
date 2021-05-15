#include <stdlib.h>
#include "../includes/memory.h"

void* reallocate(void* array, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(array);
        return NULL;
    }

    void* result = realloc(array, newSize);
    if (result == NULL) exit(1);
    return result;
}



