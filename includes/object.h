#ifndef ms_object_h
#define ms_object_h 
#include "../includes/common.h"
#include "../includes/value.h"
#include "../includes/vm.h"

#define CHECK_STRING(val) \
    (isObjType(val, OBJ_STRING))

#define AS_STRING(val) \
    ((ObjString*)AS_OBJ(val))

#define AS_NATIVE_STRING(val) \
    ((char*)&(((ObjString*)AS_OBJ(val))->allocated))

typedef enum {
    OBJ_STRING
} ObjType;

struct Obj {                /* Typedef defined in value.h */
    ObjType type;
    uint32_t hash;
    Obj* next;
};

struct ObjString {
    Obj obj;
    int length;
    char allocated[];           /* Flexible Array Member */
};

ObjString* allocateRawString(VM* vm, int length);
ObjString* allocateString(VM* vm, const char* chars, int length);
ObjString* strConcat(VM* vm, Value val1, Value val2);

static inline bool isObjType(Value value, ObjType type) {
    return CHECK_OBJ(value) && AS_OBJ(value)->type == type; 
}

void printObject(Value value);
void freeObjects(VM* vm);

#endif
