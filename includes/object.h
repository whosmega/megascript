#ifndef ms_object_h
#define ms_object_h 

#include "../includes/common.h"
#include "../includes/value.h"
#include "../includes/vm.h"

typedef bool (*NativeFuncPtr)(VM*, int, bool);
typedef bool (*NativeMethodPtr)(VM*, Obj*, int, bool);

#define CHECK_STRING(val) \
    (isObjType(val, OBJ_STRING))

#define CHECK_ARRAY(val) \
    (isObjType(val, OBJ_ARRAY))

#define CHECK_FUNCTION(val) \
    (isObjType(val, OBJ_FUNCTION))

#define CHECK_CLOSURE(val) \
    (isObjType(val, OBJ_CLOSURE))

#define CHECK_UPVALUE(val) \
    (isObjType(val, OBJ_UPVALUE))

#define CHECK_NATIVE_FUNCTION(val) \
    (isObjType(val, OBJ_NATIVE_FUNCTION))

#define CHECK_CLASS(val) \
    (isObjType(val, OBJ_CLASS))

#define CHECK_INSTANCE(val) \
    (isObjType(val, OBJ_INSTANCE))

#define CHECK_METHOD(val) \
    (isObjType(val, OBJ_METHOD))

#define CHECK_TABLE(val) \
    (isObjType(val, OBJ_TABLE))

#define CHECK_NATIVE_METHOD(val) \
    (isObjType(val, OBJ_NATIVE_METHOD))

#define CHECK_DLL_CONTAINER(val) \
    (isObjType(val, OBJ_DLL_CONTAINER))

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

#define AS_UPVALUE(val) \
    ((ObjUpvalue*)AS_OBJ(val))

#define AS_CLOSURE(val) \
    ((ObjClosure*)AS_OBJ(val))

#define AS_CLASS(val) \
    ((ObjClass*)AS_OBJ(val))

#define AS_INSTANCE(val) \
    ((ObjInstance*)AS_OBJ(val))

#define AS_METHOD(val) \
    ((ObjMethod*)AS_OBJ(val))

#define AS_TABLE(val) \
    ((ObjTable*)AS_OBJ(val))

#define AS_NATIVE_METHOD(val) \
    ((ObjNativeMethod*)AS_OBJ(val))

#define AS_DLL_CONTAINER(val) \
    ((ObjDllContainer*)AS_OBJ(val))

#define OBJ_HEAD Obj obj

typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_UPVALUE,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE_FUNCTION,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_METHOD,
    OBJ_TABLE,
    OBJ_NATIVE_METHOD,
    OBJ_DLL_CONTAINER
} ObjType;

struct Obj {                /* Typedef defined in value.h */
    ObjType type;
    uint32_t hash;
    bool isMarked; 
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

struct ObjUpvalue {
    OBJ_HEAD;
    Value closed;
    Value* value;
    struct ObjUpvalue* next;
};

struct ObjFunction {
    OBJ_HEAD;
    ObjString* name; 
    Chunk chunk;
    int upvalueCount;
    bool variadic;
    int arity;                  /* Number of arguments expected */
};

struct ObjClosure {
    OBJ_HEAD;
    ObjFunction* function;
    int upvalueCount;
    ObjUpvalue** upvalues;
};

struct ObjNativeFunction {
    OBJ_HEAD;
    ObjString* name;
    NativeFuncPtr funcPtr;
};

struct ObjClass {
    OBJ_HEAD;
    ObjString* name;
    Table fields;
    Table methods;
};

struct ObjInstance {
    OBJ_HEAD;
    ObjClass* klass;
    Table table;
};

struct ObjMethod {
    OBJ_HEAD;
    ObjInstance* self;
    ObjClosure* closure;
};

struct ObjTable {
    OBJ_HEAD;
    Table table;
};

struct ObjNativeMethod {
    OBJ_HEAD;
    ObjString* name;
    Obj* self;
    NativeMethodPtr function;
};

struct ObjDllContainer {
    OBJ_HEAD;
    ObjString* fileName;
    bool closed;
    void* handle;
};

ObjString* allocateRawString(VM* vm, int length);
ObjString* allocateString(VM* vm, const char* chars, int length);
ObjString* strConcat(VM* vm, Value val1, Value val2);

ObjFunction* allocateFunction(VM* vm, ObjString* name, int arity);
ObjFunction* newFunction(VM* vm, const char* name, int arity);
ObjFunction* newFunctionFromSource(VM* vm, const char* start, int length, int arity);
ObjArray* allocateArray(VM* vm);
ObjNativeFunction* allocateNativeFunction(VM* vm, ObjString* name, NativeFuncPtr funcPtr);
ObjClosure* allocateClosure(VM* vm, ObjFunction* function);
ObjUpvalue* allocateUpvalue(VM* vm, Value* value);
ObjClass* allocateClass(VM* vm, ObjString* name);
ObjInstance* allocateInstance(VM* vm, ObjClass* klass);
ObjNativeMethod* allocateNativeMethod(VM* vm, ObjString* name, Obj* self, NativeMethodPtr methodPtr);
ObjMethod* allocateMethod(VM* vm, ObjInstance* instance, ObjClosure* closure);
ObjTable* allocateTable(VM* vm);
ObjDllContainer* allocateDllContainer(VM* vm, ObjString* fileName, void* handle);

static inline bool isObjType(Value value, ObjType type) {
    return CHECK_OBJ(value) && AS_OBJ(value)->type == type; 
}

void printObject(Value value);
void freeObjects(VM* vm);
void freeObject(VM* vm, Obj* obj);
#endif
