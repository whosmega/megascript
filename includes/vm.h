#ifndef ms_vm_h
#define ms_vm_h
#include "../includes/value.h"
#include "../includes/table.h"
#include "../includes/chunk.h"
#include <stdint.h>
#define STACK_MAX 256

typedef struct {
    Chunk* chunk;       /* Chunk to be executed */
    uint8_t* ip;        /* Instruction Pointer */
    Value stack[STACK_MAX]; /* Stack */
    Value* stackTop;
    Table strings;          /* Used for string interning */
    Table globals;
    Obj* ObjHead;       /* Used for tracking the object linked list */
} VM;

typedef enum {
    INTERPRET_OK,
    // INTERPRET_YIELD,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;      /* Different result enums returned when interpreting */

void initVM(VM* vm);
void loadChunk(VM* vm, Chunk* chunk);
void freeVM(VM* vm);
InterpretResult interpret(VM* vm);
void resetStack(VM* vm);

#endif
