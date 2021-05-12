#include <stdio.h>
#include "debug.h"
#include "value.h"

void dissembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->elem_count;) {
        offset = dissembleInstruction(chunk, offset);
    }

}

int simpleInstruction(const char* insName, int offset) {
    printf("%s\n", insName);
    return offset + 1;
}

int constantInstruction(const char* insName, Chunk* chunk, int offset) {
    uint8_t constantIndex = chunk->code[offset + 1];
    printf("%-16s %4d '", insName, constantIndex);
    printValue(chunk->constants.values[constantIndex]);
    printf("'\n");
    return offset + 2;                                      /* return 2 because it has 1 operand specifying
                                                               the constant index in the constant pool */
}

int longConstantInstruction(const char* insName, Chunk* chunk, int offset) {
    uint16_t constantIndex = chunk->code[offset + 1] | chunk->code[offset + 2] << 8;
    printf("%-16s %4d '", insName, constantIndex);
    printValue(chunk->constants.values[(int)constantIndex]);
    printf("'\n");
    return offset + 3;
    
}

int dissembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        /* If the line number is the same as the previous instuction's, use a pipe to denote it */
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];

    switch(instruction) {
        case OP_EOF:
            return simpleInstruction("OP_EOF", offset);
        case OP_RET:
            return simpleInstruction("OP_RET", offset);
        case OP_CONST:
            return constantInstruction("OP_CONST", chunk, offset);
        case OP_CONST_LONG:
            return longConstantInstruction("OP_CONST_LONG", chunk, offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUB:
            return simpleInstruction("OP_SUB", offset);
        case OP_MUL:
            return simpleInstruction("OP_MUL", offset);
        case OP_DIV:
            return simpleInstruction("OP_DIV", offset);
        case OP_POW:
            return simpleInstruction("OP_POW", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        default:
            printf("Unknown opcode %d\n", instruction); 
            return offset + 1;
    }
}
