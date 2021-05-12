#ifndef ms_vm_h
#define ms_vm_h
#include "value.h"

#include "chunk.h"
#define STACK_MAX 256

typedef struct {
    Chunk* chunk;       /* Chunk to be executed */
    uint8_t* ip;        /* Instruction Pointer */
    Value stack[STACK_MAX]; /* Stack */
    Value* stackTop;
    int insInterpreted;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_YIELD,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;      /* Different result enums returned when interpreting */

void initVM(VM* vm, Chunk* chunk);
void freeVM(VM* vm);
InterpretResult interpret(VM* vm);
void resetStack(VM* vm);
void push(VM* vm, Value value);
Value pop(VM* vm);

#endif
