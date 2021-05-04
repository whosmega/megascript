#ifndef ls_memory_h
#define ls_memory_h

// Macros //

#define THRESHOLD 8
#define GROW_CAPACITY(capacity) \
((capacity) < THRESHOLD ? THRESHOLD : (capacity) * 2)

#define GROW_ARRAY(datatype, array, oldS, newS) \
(datatype*)reallocate(array, (oldS) * sizeof(datatype), (newS) * sizeof(datatype))

#define FREE_ARRAY(datatype, array, oldS) reallocate(array, (oldS) * sizeof(datatype), 0)

void* reallocate(void* array, size_t oldSize, size_t newSize);

#endif


