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

#define V_MAJOR 0
#define V_MINOR 1

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

void cleanup(const char* source, Chunk* chunk, VM* vm) {
    free((char*)source);
    freeChunk(chunk);
    freeVM(vm);
}

void runFile(const char* fileName) {
    char* source = readFile(fileName);

    VM vm;
    Chunk chunk;

    initChunk(&chunk);
    InterpretResult result1 = compile(source, &chunk);
    
    if (result1 == INTERPRET_COMPILE_ERROR) {
        cleanup(source, &chunk, &vm);
        exit(100);
    }

    initVM(&vm, &chunk);

    InterpretResult result2 = interpret(&vm);
    cleanup(source, &chunk, &vm);

    if (result2 == INTERPRET_RUNTIME_ERROR) exit(101);
    if (result2 == INTERPRET_YIELD) {
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

    printf("MegaScript Repl Session Started (type '.exit' to exit)\n");
    printf("Version : %d.%d\n", V_MAJOR, V_MINOR);
    for (;;) {
        printf("> ");
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            printf("\n");
            break;
        }
        
        if (memcmp(buffer, ".exit", 5) == 0) {
            if (initializedVM) {
                freeChunk(&chunk);
                freeVM(&vm);
            }
            break;
        }
        InterpretResult result1 = compile(buffer, &chunk);
        if (result1 == INTERPRET_COMPILE_ERROR) {
            freeChunk(&chunk);
            freeVM(&vm);
            exit(100);
        }
        if (initializedVM == false) {
            /* We initialize during runtime because the bytecode has to be loaded in first */
            initVM(&vm, &chunk);
            initializedVM = true;
        }
        InterpretResult result = interpret(&vm);

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



