#include <stdio.h>
#include "../includes/common.h"
#include "../includes/scanner.h"
#include <string.h>

void initScanner(Scanner* scanner, const char* source) {
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
}

static Token makeError(Scanner* scanner, const char* message) {
    Token errToken;
    errToken.type = TOKEN_ERROR;
    errToken.line = scanner->line;
    errToken.start = message;
    errToken.length = (int)strlen(message);
    return errToken;
}

static Token makeToken(Scanner* scanner, TokenType type) {
    Token token;
    token.type = type;
    token.line = scanner->line;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    return token;
}

static TokenType checkKeyword(Scanner* scanner, int start, int length, const char* rest, TokenType type) {
    if (scanner->current - scanner->start == start + length && (memcmp((char*)scanner->start + start, rest, length) == 0)) {
        /* If the length of the characters is same and the characters themselves are same then 
         * return the given type, otherwise return identifier */
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static bool isAtEnd(Scanner* scanner) {
    return *scanner->current == '\0';
}

static char advance(Scanner* scanner) {
    scanner->current++;                     /* Advance to point to the next character */
    return scanner->current[-1];            /* Return the previmous character */
}

static bool match(Scanner* scanner, char c) {
    /* If the next character is the desired, advance the current char pointer
     * and return true, otherwise return false */
    
    if (isAtEnd(scanner)) return false;
    if (*scanner->current != c) return false;
    scanner->current++;
    return true;
}

static char peek(Scanner* scanner) {
    return *scanner->current;           /* Return the next character without incrementing
                                         * the pointer */
}

static char peekNext(Scanner* scanner) {
    if (isAtEnd(scanner)) return '\0';
    return scanner->current[1];
}

static void skipWhitespace(Scanner* scanner) {
    for (;;) {
        char c = peek(scanner);
        switch(c) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            case '/':
                if (peekNext(scanner) == '/') {
                    while ((peek(scanner) != '\n') && !isAtEnd(scanner)) advance(scanner);
                } else if (peekNext(scanner) == '*') {

                    for (;;) {
                        if (peek(scanner) == '*' && peekNext(scanner) == '/') {
                            advance(scanner);
                            advance(scanner);
                            advance(scanner);
                            break;
                        }
                        if (isAtEnd(scanner)) break;
                        advance(scanner);
                    }
                    break;
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static bool isDigit(char c) {
    return (c >= '0' && c <= '9');
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c == '_');
}

static bool isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}

static Token scanString(Scanner* scanner, char type) {
    while (!isAtEnd(scanner)) {
        if (peek(scanner) == type) break;
        if (peek(scanner) == '\\') {
            if (peekNext(scanner) == type) advance(scanner);
        }
        if (peek(scanner) == '\n') scanner->line++;
        advance(scanner);
    }

    if (isAtEnd(scanner)) {
        return makeError(scanner, "Expected String Termination");
    }
    
    advance(scanner);           /* Consume last quote */
    return makeToken(scanner, TOKEN_STRING);
}

static Token scanNumber(Scanner* scanner) {
    while (isDigit(peek(scanner)) && !isAtEnd(scanner)) {
        advance(scanner);
    }

    if (peek(scanner) == '.') {
        if (peekNext(scanner) != '.') {
            advance(scanner);       /* Consume the decimal point */ 
        
            while (isDigit(peek(scanner)) && !isAtEnd(scanner)) {
                advance(scanner);
            }
        }
    }

    return makeToken(scanner, TOKEN_NUMBER);
}

static Token scanIdentifier(Scanner* scanner) {
   while (isAlphaNumeric(peek(scanner))) advance(scanner);
   TokenType type = TOKEN_IDENTIFIER;

   switch (scanner->start[0]) {
       case 'a': type = checkKeyword(scanner, 1, 2, "nd", TOKEN_AND); break;
       case 'b': type = checkKeyword(scanner, 1, 4, "reak", TOKEN_BREAK); break;
       case 'o': type = checkKeyword(scanner, 1, 1, "r", TOKEN_OR); break;
       case 'w': type = checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE); break;
       case 'f':
            if (scanner->current - scanner->start > 1) {    /* If identifier length is greater than 1 */
                switch (scanner->start[1]) {
                    case 'o': type = checkKeyword(scanner, 2, 1, "r", TOKEN_FOR); break;
                    case 'a': type = checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE); break;
                    case 'u': type = checkKeyword(scanner, 2, 2, "nc", TOKEN_FUNC); break;
                    case 'r': type = checkKeyword(scanner, 2, 2, "om", TOKEN_FROM); break;
                }
            }
            break;
       case 't': type = checkKeyword(scanner, 1, 3, "rue", TOKEN_TRUE); break;
       case 'i': 
            type = checkKeyword(scanner, 1, 1, "f", TOKEN_IF); 
            if (type == TOKEN_IDENTIFIER) {
                if (scanner->current - scanner->start > 1) {
                    switch (scanner->start[1]) {
                        case 'n': {
                            if (scanner->current - scanner->start > 2) {
                                type = checkKeyword(scanner, 2, 6, "herits", TOKEN_INHERITS);
                            } else {
                                type = TOKEN_IN;
                            }
                            break;
                        }
                        case 'm': type = checkKeyword(scanner, 2, 4, "port", TOKEN_IMPORT); break;
                    }
                }
            }
            break;
       case 'e': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'l':
                        type = checkKeyword(scanner, 2, 2, "se", TOKEN_ELSE);
                        
                        /* If 'else' fails, try 'elseif' */
                        if (type == TOKEN_IDENTIFIER) {
                            type = checkKeyword(scanner, 2, 4, "seif", TOKEN_ELSEIF); break;
                        }
                        break;
                    case 'n': type = checkKeyword(scanner, 2, 1, "d", TOKEN_END); break;
                }
            }
            break;
        case 'c': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'l': type = checkKeyword(scanner, 2, 3, "ass", TOKEN_CLASS); break;
                }
            }
            break;
        case 'v': type = checkKeyword(scanner, 1, 2, "ar", TOKEN_VAR); break;
        case 'r': type = checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN); break;
        case 'n': type = checkKeyword(scanner, 1, 2, "il", TOKEN_NIL); break;
        case 'g': type = checkKeyword(scanner, 1, 5, "lobal", TOKEN_GLOBAL); break;
        case 's': {
            type = checkKeyword(scanner, 1, 3, "elf", TOKEN_SELF);
        
            if (type == TOKEN_IDENTIFIER) {
                type = checkKeyword(scanner, 1, 4, "uper", TOKEN_SUPER);
                break;
            }
            break;
        }
   }

   return makeToken(scanner, type);
}

Token scanToken(Scanner* scanner) {
    skipWhitespace(scanner);
    scanner->start = scanner->current;
    if (isAtEnd(scanner)) return makeToken(scanner, TOKEN_EOF);
    
    char c = advance(scanner);

    if (isAlpha(c)) return scanIdentifier(scanner);
    if (isDigit(c)) return scanNumber(scanner);
    switch (c) {
        case '"': return scanString(scanner, c);
        case '\'': return scanString(scanner, c); 
        case '(': return makeToken(scanner, TOKEN_ROUND_OPEN);
        case ')': return makeToken(scanner, TOKEN_ROUND_CLOSE);
        case '{': return makeToken(scanner, TOKEN_CURLY_OPEN);
        case '}': return makeToken(scanner, TOKEN_CURLY_CLOSE);
        case '[': return makeToken(scanner, TOKEN_SQUARE_OPEN);
        case ']': return makeToken(scanner, TOKEN_SQUARE_CLOSE);
        case ';': return makeToken(scanner, TOKEN_SEMICOLON);
        case ',': return makeToken(scanner, TOKEN_COMMA);
        case '#': return makeToken(scanner, TOKEN_HASH);
        case '!': return makeToken(scanner, 
            match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG
        );
        case '.': return makeToken(scanner,
            match(scanner, '.') ? (match(scanner, '.') ? TOKEN_VAR_ARGS : TOKEN_RANGE) : TOKEN_DOT
        );
        case '+': return makeToken(scanner, 
            match(scanner, '=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS
        );
        case '-': return makeToken(scanner, 
            match(scanner, '=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS
        );
        case '*': return makeToken(scanner, 
            match(scanner, '=') ? TOKEN_MUL_EQUAL : TOKEN_ASTERISK
        );
        case '/': return makeToken(scanner,
            match(scanner, '=') ? TOKEN_DIV_EQUAL : TOKEN_SLASH
        );
        case '%': return makeToken(scanner,
            match(scanner, '=') ? TOKEN_MOD_EQUAL : TOKEN_MOD
        );
        case '^': return makeToken(scanner,
            match(scanner, '=') ? TOKEN_POW_EQUAL : TOKEN_EXP
        );
        case '=': return makeToken(scanner,
            match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL
        );
        case '>': return makeToken(scanner,
            match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER
        );
        case '<': return makeToken(scanner, 
            match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS
        );
        case ':': {
            if (match(scanner, '=')) {
                return makeToken(scanner, TOKEN_CLASSTEMP_EQ);
            } else if (peek(scanner) == ':' && peekNext(scanner) == '=') {
                advance(scanner);
                advance(scanner);
                return makeToken(scanner, TOKEN_CLASSINHERIT_EQ);
            } else {
                return makeToken(scanner, TOKEN_COLON);
            }
        }
    }

    return makeError(scanner, "Unexpected Character");
}
