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

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slotPtr;
    bool shouldReturn;
} CallFrame;

typedef struct {
    CallFrame frames[FRAME_MAX];
    int frameCount;
    Value stack[STACK_MAX]; /* Stack */
    Obj** greyStack;
    int greyCount;
    int greyCapacity;
    Value* stackTop;
    Table strings;          /* Used for string interning */
    Table globals;
    
    PtrTable arrayMethods;     /* Used for storing methods for arrays */ 

    Obj* ObjHead;       /* Used for tracking the object linked list */
    ObjUpvalue* UpvalueHead;
    size_t bytesAllocated;
    size_t nextGC;
    bool running;
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


bool msmethod_array_insert(VM* vm, Obj* self, int argCount, bool shouldReturn); 
/*          API             */ 
void msapi_runtimeError(VM* vm, const char* format, ...); 
bool msapi_pushCallFrame(VM* vm, ObjClosure* closure);
void msapi_popCallFrame(VM* vm);
bool msapi_isFalsey(Value value);
bool msapi_isEqual(Value value1, Value value2);
void msapi_push(VM* vm, Value value);
Value msapi_pop(VM* vm);
Value msapi_peek(VM* vm, unsigned int index);
Value* msapi_peekptr(VM* vm, unsigned int index);
void msapi_pushn(VM* vm, Value value, unsigned int count);
void msapi_popn(VM* vm, unsigned int count); 


#endif
