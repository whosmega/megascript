#include "compiler.h"
#include "chunk.h"
#include "scanner.h"
#include <stdio.h>

void compile(const char* source, Chunk* chunk) {
    Scanner scanner;
    initScanner(&scanner, source);
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
}
