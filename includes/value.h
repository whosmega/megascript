#ifndef ms_value_h
#define ms_value_h

#include <stdio.h>
#include "../includes/common.h"

/* ValueType contains native VM types */

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ
} ValueType;

/* Heap Allocated (obj stuff) */

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjArray ObjArray;

/* - - - - - - - - - - - - - -*/

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

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

#define NATIVE_TO_NUMBER(num) \
    (Value){VAL_NUMBER, {.number = num}}
#define NATIVE_TO_BOOLEAN(b) \
    (Value){VAL_BOOL, {.boolean = b}}
#define NIL() \
    (Value){VAL_NIL, {.number = 0}}
#define OBJ(object) \
    (Value){VAL_OBJ, {.obj = (Obj*)object}}     /* Takes any form of Obj Pointer (objstring, ect)
                                                   and constructs a valid ms value with type object */

#define AS_BOOL(value) \
    ((value).as.boolean)
#define AS_NUMBER(value) \
    ((value).as.number)
#define AS_OBJ(value) \
    ((value).as.obj)

#define CHECK_NUMBER(value) \
    ((value).type == VAL_NUMBER)
#define CHECK_BOOLEAN(value) \
    ((value).type == VAL_BOOL)
#define CHECK_NIL(value) \
    ((value).type == VAL_NIL)
#define CHECK_OBJ(value) \
    ((value).type == VAL_OBJ)


#endif
