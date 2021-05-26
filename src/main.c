#include "../includes/common.h"
#include "../includes/chunk.h"
#include "../includes/debug.h"
#include "../includes/vm.h"
#include "../includes/compiler.h"
#include "../includes/table.h"
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
    initVM(&vm);
    InterpretResult result1 = compile(source, &chunk, &vm);
    
    if (result1 == INTERPRET_COMPILE_ERROR) {
        cleanup(source, &chunk, &vm);
        exit(100);
    }

    loadChunk(&vm, &chunk);

    InterpretResult result2 = interpret(&vm);
    cleanup(source, &chunk, &vm);

    if (result2 == INTERPRET_RUNTIME_ERROR) exit(101);
}

void repl() {
    char buffer[1024];
//     printf("MegaScript Repl Session Started (type '.exit' to exit)\n");
//     printf("Version : %d.%d\n", V_MAJOR, V_MINOR);

    for (;;) {
        VM vm;
        Chunk chunk;
        initVM(&vm);

        printf("> ");
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            printf("\n");
            break;
        }
        
        if (memcmp(buffer, ".exit", 5) == 0) {
            freeChunk(&chunk);
            freeVM(&vm);
            exit(0);
        }
        initChunk(&chunk);
        InterpretResult result1 = compile(buffer, &chunk, &vm);
        loadChunk(&vm, &chunk);
        if (result1 == INTERPRET_OK) { 
            InterpretResult result2 = interpret(&vm);
        }
        freeChunk(&chunk);
        freeVM(&vm);
    }
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        runFile(argv[1]);
    } else {
        repl();
    }

    return 0;
}



