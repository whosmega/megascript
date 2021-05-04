#ifndef ls_value_h
#define ls_value_h

#include <stdio.h>
#include "common.h"
typedef double Value;

typedef struct {
    int count;
    int capacity;
    Value* values;
} ValueArray;                                           /* Value array to store constants in a chunk
                                                           acting as a constant chunk */

void initValueArray(ValueArray* array);                 /* Initializes a value array */
void writeValueArray(ValueArray* array, Value value);   /* Writes a constant to a value array */
void freeValueArray(ValueArray* array);                 /* Frees the array */

void printValue(Value value);

#endif
