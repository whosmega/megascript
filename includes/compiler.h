#ifndef ms_compiler_h
#define ms_compiler_h
#include <string.h>
#include "../includes/vm.h"
#include "../includes/chunk.h"
#include "../includes/scanner.h"

typedef struct {
    Token previous;
    Token current;
    Chunk* compilingChunk;
    VM* vm;
    bool hadError;
    bool panicMode;         /* When panic mode is set to true all 
                             * further errors get suppressed */
} Parser;

typedef enum {
    PREC_NONE,              // None 
    PREC_ASSIGNMENT,        // =
    PREC_OR,                // or
    PREC_AND,               // and 
    PREC_EQUALITY,          // == !=
    PREC_COMPARISON,        // > < >= <=
    PREC_TERM,              // + -
    PREC_FACTOR,            // * /
    PREC_EXP_MOD,           // ^ %
    PREC_UNARY,             // - ++a --a a++ a-- 
    PREC_CALL,              // . ()
    PREC_PRIMARY            // 4 90.3
} Precedence;

typedef void (*ParseFn)(Scanner* scanner, Parser* parser);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    ParseFn postfix;
    Precedence precedence;
} ParseRule;


InterpretResult compile(const char* source, Chunk* chunk, VM* vm); 

#endif
