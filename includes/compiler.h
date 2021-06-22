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
    bool isCaptured;        /* Tells if a local variable is captured by an upvalue */ 
} Local;

typedef struct {
    uint8_t index;          /* Index of the upvalue on the runtime upvalue array */ 
    bool isLocal;           /* if its a local variable or another upvalue */ 
} Upvalue;

typedef struct Compiler {
    Local locals[LVAR_MAX];
    Upvalue upvalues[UPVAL_MAX];
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

typedef enum {
    FLAG_DISSEMBLY,
    FLAG_COUNT          // number of flags
} FlagType;

typedef struct {
    int numFlags;
    bool flags[FLAG_COUNT];
} FlagContainer;

void initUintArray(UintArray* array);
void writeUintArray(UintArray* array, unsigned int value);
unsigned int getUintArray(UintArray* array, int index);
void freeUintArray(UintArray* array);
void initCompiler(Compiler* compiler, ObjFunction* function);
void initParser(Parser* parser, Compiler* compiler, VM* vm);
InterpretResult compile(const char* source, VM* vm, ObjFunction* function); 

#endif
