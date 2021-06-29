#ifndef ms_scanner_h
#define ms_scanner_h

typedef struct {
    int line;
    const char* start;
    const char* current;
} Scanner;

typedef enum {
    TOKEN_INHERITS,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_ASTERISK, TOKEN_SLASH, TOKEN_EXP,                // [+, -, *, /, ^]  
    TOKEN_ROUND_OPEN, TOKEN_ROUND_CLOSE, TOKEN_SQUARE_OPEN, TOKEN_SQUARE_CLOSE,     // [(, ), [, ], {, }, ,, ;, :, ., ?]
    TOKEN_CURLY_OPEN, TOKEN_CURLY_CLOSE,
    TOKEN_COMMA, TOKEN_SEMICOLON, TOKEN_COLON, TOKEN_DOT,

    TOKEN_BANG, TOKEN_BANG_EQUAL,                                                   // [!, !=, =, ==, >, >=, <, <=, #, :=, ::=]
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_HASH,
    TOKEN_CLASSTEMP_EQ, TOKEN_CLASSINHERIT_EQ,
    TOKEN_MOD, 
    TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL,
    TOKEN_MUL_EQUAL, TOKEN_DIV_EQUAL,
    TOKEN_POW_EQUAL, TOKEN_MOD_EQUAL,

    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,                                   // [variablename, "hello", 121]

    TOKEN_OR, TOKEN_AND, TOKEN_WHILE, TOKEN_FOR,                                   /* [or, and, while, for, false,
                                                                                       true, if, else, elseif] */
    TOKEN_FALSE, TOKEN_TRUE, TOKEN_IF, TOKEN_ELSE, TOKEN_ELSEIF,
    TOKEN_CLASS, TOKEN_INHERITS_POS, TOKEN_FUNC, TOKEN_END, 
    TOKEN_RETURN, TOKEN_GLOBAL, TOKEN_VAR, TOKEN_NIL, TOKEN_BREAK, 
    TOKEN_IN,

    TOKEN_RANGE, TOKEN_VAR_ARGS,                                                     // [{1..4}, abc(args...)]
    TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;


void initScanner(Scanner* scanner, const char* source);
Token scanToken(Scanner* scanner);

#endif
