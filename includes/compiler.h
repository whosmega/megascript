#ifndef ms_compiler_h
#define ms_compiler_h
#include <string.h>
#include "../includes/vm.h"
#include "../includes/chunk.h"
#include "../includes/scanner.h"

typedef struct {
    Token identifier;
    int significantTemps;
    int depth;
} Local;

typedef struct Compiler {
    Local locals[LVAR_MAX];
    ObjFunction* function;
    struct Compiler* enclosing;
    int localCount;
    int scopeDepth;
    int significantTemps;
} Compiler;

typedef struct {
    unsigned int* array;
    int count;
    int capacity;
} UintArray;

typedef struct {
    Token previous;
    Token current;
    Compiler* compiler;
    UintArray* unpatchedBreaks;
    VM* vm;
    bool hadError;
    bool panicMode;         /* When panic mode is set to true all 
                             * further errors get suppressed */
} Parser;


typedef enum {
    CALL_FUNC,
    CALL_ARRAY,
    CALL_NONE
} CallType;

void initUintArray(UintArray* array);
void writeUintArray(UintArray* array, unsigned int value);
unsigned int getUintArray(UintArray* array, int index);
void freeUintArray(UintArray* array);
void initCompiler(Compiler* compiler, ObjFunction* function);
void initParser(Parser* parser, Compiler* compiler, VM* vm);
InterpretResult compile(const char* source, VM* vm, ObjFunction* function); 

#endif
