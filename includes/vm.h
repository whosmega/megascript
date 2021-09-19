#ifndef ms_vm_h
#define ms_vm_h

#include "../includes/value.h"
#include "../includes/table.h"
#include "../includes/chunk.h"
#include <stdint.h>
#define LVAR_MAX 256
#define UPVAL_MAX 256
#define FRAME_MAX 256
#define STACK_MAX LVAR_MAX * FRAME_MAX
#define IMPORT_CYCLE_MAX 50

typedef struct {
    int count;
    int capacity;
    ObjString** values;
} ObjStringArray;


void initObjStringArray(ObjStringArray* array);
void writeObjStringArray(ObjStringArray* array, ObjString* value);   
void freeObjStringArray(ObjStringArray* array);

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slotPtr;
    bool shouldReturn;
    bool isCoroutine;
} CallFrame;

typedef struct {
    ObjTable* globals;
    ObjStringArray customGlobals;
    ObjString* moduleName;
} Module;

typedef struct {
    CallFrame frames[FRAME_MAX];
    int frameCount;
    Value stack[STACK_MAX];       /* Stack */
    Obj** greyStack;
    int greyCount;
    int greyCapacity;
    Value* stackTop;
    Table strings;                /* Used for string interning */
    ObjTable* globals;
    Module* currentModule;
    

    PtrTable arrayMethods;        /* Used for storing methods for arrays */ 
    PtrTable stringMethods;     
    PtrTable tableMethods;
    PtrTable dllMethods;

    Obj* ObjHead;                 /* Used for tracking the object linked list */
    ObjUpvalue* UpvalueHead;
    size_t bytesAllocated;
    size_t nextGC;
    bool running;

    Table importCache;
    Module modules[IMPORT_CYCLE_MAX];
    int moduleCount;
} VM;


typedef enum {
    INTERPRET_OK,
    // INTERPRET_YIELD,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;      /* Different result enums returned when interpreting */

void initVM(VM* vm);
void freeVM(VM* vm);
InterpretResult interpret(VM* vm, ObjFunction* function);
void resetStack(VM* vm);

char* findFile(VM* vm, char* path, bool genErr); 
bool msmethod_array_insert(VM* vm, Obj* self, int argCount, bool shouldReturn); 
bool msmethod_string_capture(VM* vm, Obj* self, int argCount, bool shouldReturn);
bool msmethod_string_split(VM* vm, Obj* self, int argCount, bool shouldReturn);
bool msmethod_string_getAscii(VM* vm, Obj* self, int argCount, bool shouldReturn);
bool msmethod_table_keys(VM* vm, Obj* self, int argCount, bool shouldReturn);
bool msmethod_dll_close(VM* vm, Obj* self, int argCount, bool shouldReturn);
bool msmethod_dll_query(VM* vm, Obj* self, int argCount, bool shouldReturn);

#endif
