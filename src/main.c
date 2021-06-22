#include "../includes/common.h"
#include "../includes/chunk.h"
#include "../includes/debug.h"
#include "../includes/vm.h"
#include "../includes/compiler.h"
#include "../includes/table.h"
#include "../includes/object.h"
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

void cleanup(const char* source, VM* vm) {
    free((char*)source);
    freeVM(vm);
}

void runFile(const char* fileName, FlagContainer flagContainer) {
    char* source = readFile(fileName);

    VM vm;
    initVM(&vm);
    ObjFunction* function = newFunction(&vm, "main", 0);
    InterpretResult result1 = compile(source, &vm, function);

    if (result1 == INTERPRET_COMPILE_ERROR) {
        if (flagContainer.flags[FLAG_DISSEMBLY]) {
            printf("Dissembly was not shown because an error occured during compilation\n");
        }

        cleanup(source, &vm);
        exit(100);
    }

    if (flagContainer.flags[FLAG_DISSEMBLY]) {
        dissembleChunk(0, &function->chunk, "DISSEMBLY");
        return;
    }

    InterpretResult result2 = interpret(&vm, function);
    cleanup(source, &vm);

    if (result2 == INTERPRET_RUNTIME_ERROR) exit(101);
}

void repl() {
    char buffer[1024];
    printf("MegaScript Repl Session Started (type '.exit' to exit)\n");
    printf("Version : %d.%d\n", V_MAJOR, V_MINOR);

    bool ran = false;
    for (;;) {
        VM vm;
        initVM(&vm);
        ObjFunction* function = newFunction(&vm, "mainrepl", 0);
        printf("> ");
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            printf("\n");
            break;
        }
        
        if (memcmp(buffer, ".exit", 5) == 0) {
            freeVM(&vm);
            exit(0);
        }
        ran = true;
        InterpretResult result1 = compile(buffer, &vm, function);
        
        if (result1 == INTERPRET_OK) { 
            InterpretResult result2 = interpret(&vm, function);
        }
        freeVM(&vm);
    }
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        repl();
        return 0;
    }

    char* sourceFile = NULL; 
    
    // Flag 
    FlagContainer flagContainer;
    flagContainer.numFlags = 0;

    for (int i = 0; i < FLAG_COUNT; i++) {
        flagContainer.flags[i] = false;
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // flag 
            if (strcmp("-d", argv[i]) == 0) {
                flagContainer.numFlags++;
                flagContainer.flags[FLAG_DISSEMBLY] = true;
            } else {
                fprintf(stderr, "Unknown Flag\n");
                return 70;
            }
        } else {
            sourceFile = argv[i];
        }
    }

    if (sourceFile != NULL) {
        runFile(sourceFile, flagContainer);
    } else {
        repl();
    }

    return 0;
}



