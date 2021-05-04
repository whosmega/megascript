#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"
#include "compiler.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    
    if (!file) {
        fprintf(stderr, "Could not open file on path %s\n", path);
        exit(99);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);

    if (buffer == NULL) {
        fprintf(stderr, "Could not allocate enough memory to start repl\n");
        exit(99);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);

    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file\n");
        exit(99);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

void runFile(const char* fileName) {
    char* source = readFile(fileName);

    VM vm;
    Chunk chunk;

    initChunk(&chunk);
    compile(source, &chunk);
    initVM(&vm, &chunk);

    InterpretResult result = interpret(&vm);
    free(source);
    freeChunk(&chunk);
    freeVM(&vm);

    if (result == INTERPRET_COMPILE_ERROR) exit(100);
    if (result == INTERPRET_RUNTIME_ERROR) exit(101);
    if (result == INTERPRET_YIELD) {
        fprintf(stderr, "Source Corruption : EOF not found\n");
        exit(102);
    }
}

void repl() {
    char buffer[1024];
    VM vm;
    Chunk chunk;
    initChunk(&chunk);
    
    bool initializedVM = false;
    for (;;) {
        printf("> ");
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            printf("\n");
            break;
        }
        
        compile(buffer, &chunk);
        if (initializedVM == false) {
            /* We initialize during runtime because the bytecode has to be loaded in first */
            initVM(&vm, &chunk);
            initializedVM = true;
        }
        InterpretResult result = interpret(&vm);

        if (result == INTERPRET_COMPILE_ERROR) break;
        if (result == INTERPRET_RUNTIME_ERROR) break;
        if (result == INTERPRET_OK) break;
        if (result == INTERPRET_YIELD) continue;
    }

    freeChunk(&chunk);
    freeVM(&vm);
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        runFile(argv[1]);
    } else {
        repl();
    }

    return 0;
}



