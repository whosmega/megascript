#ifndef ms_object_h
#define ms_object_h 
#include "../includes/common.h"
#include "../includes/value.h"
#include "../includes/vm.h"

typedef bool (*NativeFuncPtr)(VM*, int, int);

#define CHECK_STRING(val) \
    (isObjType(val, OBJ_STRING))

#define CHECK_ARRAY(val) \
    (isObjType(val, OBJ_ARRAY))

#define CHECK_FUNCTION(val) \
    (isObjType(val, OBJ_FUNCTION))

#define CHECK_NATIVE_FUNCTION(val) \
    (isObjType(val, OBJ_NATIVE_FUNCTION))

#define AS_STRING(val) \
    ((ObjString*)AS_OBJ(val))

#define AS_NATIVE_STRING(val) \
    ((char*)&(((ObjString*)AS_OBJ(val))->allocated))

#define AS_NATIVE_FUNCTION(val) \
    ((ObjNativeFunction*)AS_OBJ(val))

#define AS_ARRAY(val) \
    ((ObjArray*)AS_OBJ(val))

#define AS_NATIVE_ARRAY(val) \
    (Value*)&(AS_ARRAY(val)->array.values)

#define AS_FUNCTION(val) \
    ((ObjFunction*)AS_OBJ(val))

#define OBJ_HEAD Obj obj

typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_FUNCTION,
    OBJ_NATIVE_FUNCTION
} ObjType;

struct Obj {                /* Typedef defined in value.h */
    ObjType type;
    uint32_t hash;
    Obj* next;
};

struct ObjString {
    OBJ_HEAD;
    int length;
    char allocated[];           /* Flexible Array Member */
};

struct ObjArray {
    OBJ_HEAD;
    ValueArray array;
};

struct ObjFunction {
    OBJ_HEAD;
    ObjString* name; 
    Chunk chunk;
    bool variadic;
    int arity;                  /* Number of arguments expected */
};

struct ObjNativeFunction {
    OBJ_HEAD;
    ObjString* name;
    NativeFuncPtr funcPtr;
};

ObjString* allocateRawString(VM* vm, int length);
ObjString* allocateString(VM* vm, const char* chars, int length);
ObjString* strConcat(VM* vm, Value val1, Value val2);

ObjFunction* allocateFunction(VM* vm, ObjString* name, int arity);
ObjFunction* newFunction(VM* vm, const char* name, int arity);
ObjFunction* newFunctionFromSource(VM* vm, const char* start, int length, int arity);
ObjArray* allocateArray(VM* vm);
ObjNativeFunction* allocateNativeFunction(VM* vm, ObjString* name, NativeFuncPtr funcPtr);

static inline bool isObjType(Value value, ObjType type) {
    return CHECK_OBJ(value) && AS_OBJ(value)->type == type; 
}

void printObject(Value value);
void freeObjects(VM* vm);
void freeObject(Obj* obj);
#endif
