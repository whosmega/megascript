#include "compiler.h"
#include "chunk.h"
#include "scanner.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Forward Declaration */

static void expression(Scanner* scanner, Parser* parser);
static void grouping(Scanner* scanner, Parser* parser);
static void binary(Scanner* scanner, Parser* parser);
static void unary(Scanner* scanner, Parser* parser);
static void number(Scanner* scanner, Parser* parser);

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
   [TOKEN_BANG]           = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_BANG_EQUAL]     = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_EQUAL]          = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_EQUAL_EQUAL]    = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_GREATER]        = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_GREATER_EQUAL]  = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_LESS]           = {NULL, NULL, NULL, PREC_NONE},
   [TOKEN_LESS_EQUAL]     = {NULL, NULL, NULL, PREC_NONE},
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

};

/* - - - - - - - - - - */


static void errorAt(Parser* parser, Token* token, const char* message) {
    if (parser->panicMode == true) return;      /* Suppress */
    parser->panicMode = true;
    fprintf(stderr, "Error on line %d : ", token->line);

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
        if (parser->current.type == TOKEN_ERROR) break;

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
        default: return;
    }
}

static void unary(Scanner* scanner, Parser* parser) {
    TokenType operator = parser->previous.type;
    
    /* Parse the operand */
    parsePrecedence(scanner, parser, PREC_UNARY);

    switch (operator) {
        case TOKEN_MINUS: emitByte(parser, OP_NEGATE); break;
        default: return;        /* Unknown */
    }
}

static void number(Scanner* scanner, Parser* parser) {
    Value value = strtod(parser->previous.start, NULL);
    int index = writeConstant(currentChunk(parser), value, parser->previous.line);

    if (index > CONSTANT_MAX) {
        error(parser, "Too mant constants in a chunk");
    }
}

InterpretResult compile(const char* source, Chunk* chunk) {
    Scanner scanner;
    Parser parser;
    initScanner(&scanner, source);

    /* Initialize */
    parser.hadError = false;
    parser.panicMode = false;
    /*            */ 

    advance(&scanner, &parser);
    expression(&scanner, &parser);
    consume(&scanner, &parser, TOKEN_EOF, "Expected EOF");
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
