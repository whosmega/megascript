#include "../includes/compiler.h"
#include "../includes/chunk.h"
#include "../includes/scanner.h"
#include "../includes/vm.h"
#include "../includes/debug.h"
#include "../includes/value.h"
#include "../includes/object.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Forward Declaration */

static void expression(Scanner* scanner, Parser* parser);
static void grouping(Scanner* scanner, Parser* parser);
static void binary(Scanner* scanner, Parser* parser);
static void unary(Scanner* scanner, Parser* parser);
static void number(Scanner* scanner, Parser* parser);
static void bool_true(Scanner* scanner, Parser* parser);
static void bool_false(Scanner* scanner, Parser* parser);
static void nil(Scanner* scanner, Parser* parser);
static void string(Scanner* scanner, Parser* parser);

static ParseRule* getRule(TokenType type);
static void parsePrecedence(Scanner* scanner, Parser* parser, Precedence precedence);

/* - - - - - - - - - - */

/*  Parse Rule Table   */ 

ParseRule rules[] = {
   [TOKEN_PLUS]           = {NULL, binary, NULL, PREC_TERM},
   [TOKEN_MINUS]          = {unary, binary, NULL, PREC_TERM},
   [TOKEN_ASTERISK]       = {NULL, binary, NULL, PREC_FACTOR},
   [TOKEN_SLASH]          = {NULL, binary, NULL, PREC_FACTOR},
   [TOKEN_EXP]            = {NULL, binary, NULL, PREC_EXP_MOD},
   [TOKEN_ROUND_OPEN]     = {grouping, NULL, NULL, PREC_NONE},
   [TOKEN_ROUND_CLOSE]    = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_SQUARE_OPEN]    = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_SQUARE_CLOSE]   = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_CURLY_OPEN]     = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_CURLY_CLOSE]    = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_COMMA]          = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_SEMICOLON]      = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_COLON]          = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_DOT]            = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_BANG]           = {unary, NULL, NULL, PREC_UNARY},
   [TOKEN_BANG_EQUAL]     = {NULL, binary, NULL, PREC_EQUALITY},
   [TOKEN_EQUAL]          = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_EQUAL_EQUAL]    = {NULL, binary, NULL, PREC_EQUALITY},
   [TOKEN_GREATER]        = {NULL, binary, NULL, PREC_COMPARISON},
   [TOKEN_GREATER_EQUAL]  = {NULL, binary, NULL, PREC_COMPARISON},
   [TOKEN_LESS]           = {NULL, binary, NULL, PREC_COMPARISON},
   [TOKEN_LESS_EQUAL]     = {NULL, binary, NULL, PREC_COMPARISON},
   [TOKEN_CLASSTEMP_EQ]   = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_CLASSINHERIT_EQ]= {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_MOD]            = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_PLUS_EQUAL]     = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_MINUS_EQUAL]    = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_MUL_EQUAL]      = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_DIV_EQUAL]      = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_POW_EQUAL]      = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_MOD_EQUAL]      = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_PLUS_PLUS]      = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_MINUS_MINUS]    = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_IDENTIFIER]     = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_STRING]         = {string, NULL, NULL, PREC_PRIMARY},
   [TOKEN_NUMBER]         = {number, NULL, NULL, PREC_PRIMARY},
   [TOKEN_OR]             = {NULL, binary, NULL, PREC_OR},
   [TOKEN_AND]            = {NULL, binary, NULL, PREC_AND},
   [TOKEN_WHILE]          = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_FOR]            = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_FALSE]          = {bool_false, NULL, NULL, PREC_PRIMARY},
   [TOKEN_TRUE]           = {bool_true, NULL, NULL, PREC_PRIMARY},
   [TOKEN_IF]             = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_ELSE]           = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_ELSEIF]         = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_CLASS]          = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_SUPER]          = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_SELF]           = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_INHERITS]       = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_FUNC]           = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_END]            = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_RETURN]         = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_VAR]            = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_NIL]            = {nil, NULL, NULL, PREC_PRIMARY},
   [TOKEN_IMPORTCLASS]    = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_RANGE]          = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_VAR_ARGS]       = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_EOF]            = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_ERROR]          = {NULL, NULL, NULL, PREC_NONE}
};

/* - - - - - - - - - - */


static void errorAt(Parser* parser, Token* token, const char* message) {
    if (parser->panicMode == true) return;      /* Suppress */
    parser->panicMode = true;
    fprintf(stderr, "Error on line %d : ", token->type == TOKEN_EOF ? 1 : token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, "At End");
    } else if (token->type != TOKEN_ERROR) {
        /* Everything else */
        fprintf(stderr, "At '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}


static void errorAtCurrent(Parser* parser, const char* message) {
    errorAt(parser, &parser->current, message);
}

static void error(Parser* parser, const char* message) {
    errorAt(parser, &parser->previous, message);
}


static void advance(Scanner* scanner, Parser* parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = scanToken(scanner);
        if (parser->current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser, parser->current.start);
    }
}

static void consume(Scanner* scanner, Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(scanner, parser);
        return;
    }

    errorAtCurrent(parser, message);
}

static Chunk* currentChunk(Parser* parser) {
    return parser->compilingChunk;
}

static void emitByte(Parser* parser, uint8_t byte) {
    writeChunk(currentChunk(parser), byte, parser->previous.line); 
}

static void emitBytes(Parser* parser, uint8_t byte1, uint8_t byte2) {
    /* Meant for easy operand passing */
    emitByte(parser, byte1);
    emitByte(parser, byte2);
}

static void endCompiler(Parser* parser) {
    emitByte(parser, OP_RET);
}

static void parsePrecedence(Scanner* scanner, Parser* parser, Precedence precedence) {
    /* Parses given precedence and anything higher */ 
    advance(scanner, parser);
    /* Prefix Expressions */
    ParseFn prefixRule = getRule(parser->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(parser, "Expected an expression");
        return;
    }
    prefixRule(scanner, parser);
    
    /* Infix Expressions */

    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(scanner, parser);
        ParseFn infixRule = getRule(parser->previous.type)->infix;

        if (infixRule == NULL) {
            /* What if we have a prefix expression proceeding another prefix expression 
             * Handle the error here
             */
            error(parser, "Expected an expression");
            return;
        }

        infixRule(scanner, parser);
    }
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression(Scanner* scanner, Parser* parser) {
    parsePrecedence(scanner, parser, PREC_ASSIGNMENT);
}

static void grouping(Scanner* scanner, Parser* parser) {
    expression(scanner, parser);
    consume(scanner, parser, TOKEN_ROUND_CLOSE, "Expected an ')'");
}

static void binary(Scanner* scanner, Parser* parser) {
    TokenType operator = parser->previous.type;
    ParseRule* rule = getRule(operator);
    /* Parse the right operand */
    parsePrecedence(scanner, parser, (Precedence)rule->precedence + 1);

    switch (operator) {
        case TOKEN_PLUS: emitByte(parser, OP_ADD); break;
        case TOKEN_MINUS: emitByte(parser, OP_SUB); break;
        case TOKEN_ASTERISK: emitByte(parser, OP_MUL); break;
        case TOKEN_SLASH: emitByte(parser, OP_DIV); break;
        case TOKEN_EXP: emitByte(parser, OP_POW); break;
        case TOKEN_EQUAL_EQUAL: emitByte(parser, OP_EQUAL); break;
        case TOKEN_BANG_EQUAL: emitByte(parser, OP_NOT_EQ); break;
        case TOKEN_AND: emitByte(parser, OP_AND); break;
        case TOKEN_OR: emitByte(parser, OP_OR); break;
        case TOKEN_GREATER: emitByte(parser, OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitByte(parser, OP_GREATER_EQ); break;
        case TOKEN_LESS: emitByte(parser, OP_LESSER); break;
        case TOKEN_LESS_EQUAL: emitByte(parser, OP_LESSER_EQ); break;
        default: return;
    }
}

static void unary(Scanner* scanner, Parser* parser) {
    TokenType operator = parser->previous.type;
    
    /* Parse the operand */
    parsePrecedence(scanner, parser, PREC_UNARY);

    switch (operator) {
        case TOKEN_MINUS: emitByte(parser, OP_NEGATE); break;
        case TOKEN_BANG: emitByte(parser, OP_NOT); break;
        default: return;        /* Unknown */
    }
}

static void bool_false(Scanner* scanner, Parser* parser) {
    emitByte(parser, OP_FALSE);    
}

static void bool_true(Scanner* scanner, Parser* parser) {
    emitByte(parser, OP_TRUE);
}

static void nil(Scanner* scanner, Parser* parser) {
    emitByte(parser, OP_NIL);
}

static void string(Scanner* scanner, Parser* parser) {
    int index = writeConstant(currentChunk(parser), OBJ(allocateString(parser->vm, parser->previous.start + 1, parser->previous.length - 2)), 
                parser->previous.line);

    if (index > CONSTANT_MAX) {
        error(parser, "Too many constants in a chunk");
    }
}

static void number(Scanner* scanner, Parser* parser) {
    Value value = NATIVE_TO_NUMBER(strtod(parser->previous.start, NULL));
    int index = writeConstant(currentChunk(parser), value, parser->previous.line);

    if (index > CONSTANT_MAX) {
        error(parser, "Too many constants in a chunk");
    }
}

InterpretResult compile(const char* source, Chunk* chunk, VM* vm) {
    Scanner scanner;
    Parser parser;
    initScanner(&scanner, source);

    /* Initialize */
    parser.hadError = false;
    parser.panicMode = false;
    parser.vm = vm;
    /*            */ 
    parser.compilingChunk = chunk;
    advance(&scanner, &parser);
    expression(&scanner, &parser);
    endCompiler(&parser);

    consume(&scanner, &parser, TOKEN_EOF, "Expected EOF");
    emitByte(&parser, OP_EOF);

    #ifdef DEBUG_PRINT_BYTECODE
    
    if (!parser.hadError) {
        dissembleChunk(chunk, "MAIN");
    }
    #endif
    /* 
    int line = -1;
    

    
    for (;;) {
        Token token = scanToken(&scanner);
        if (token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   |");
        }

        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if (token.type == TOKEN_EOF) break;
    }
    */

    return !parser.hadError ? INTERPRET_OK : INTERPRET_COMPILE_ERROR;
}
