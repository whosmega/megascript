#include <stdio.h>
#include "../includes/debug.h"
#include "../includes/value.h"

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
            return constantInstruction("CONST (emit)", chunk, offset);
        case OP_CONST_LONG:
            return longConstantInstruction("CONST_LONG (emit)", chunk, offset);
        case OP_ADD:
            return simpleInstruction("ADD", offset);
        case OP_SUB:
            return simpleInstruction("SUB", offset);
        case OP_MUL:
            return simpleInstruction("MUL", offset);
        case OP_DIV:
            return simpleInstruction("DIV", offset);
        case OP_POW:
            return simpleInstruction("POW", offset);
        case OP_NEGATE:
            return simpleInstruction("NEGATE", offset);
        case OP_TRUE:
            return simpleInstruction("TRUE (emit)", offset);
        case OP_FALSE:
            return simpleInstruction("FALSE (emit)", offset);
        case OP_NIL:
            return simpleInstruction("NIL", offset);
        case OP_INCR_POST:
            return simpleInstruction("INCR_POST", offset);
        case OP_INCR_PRE:
            return simpleInstruction("INCR_PRE", offset);
        case OP_DECR_POST:
            return simpleInstruction("DECR_POST", offset);
        case OP_DECR_PRE:
            return simpleInstruction("DECR_PRE", offset);
        case OP_GREATER:
            return simpleInstruction("GREATER", offset);
        case OP_LESSER:
            return simpleInstruction("LESSER", offset);
        case OP_GREATER_EQ:
            return simpleInstruction("GREATER_EQ", offset);
        case OP_LESSER_EQ:
            return simpleInstruction("LESSER_EQ", offset);
        case OP_NOT_EQ:
            return simpleInstruction("NOT_EQ", offset);
        case OP_NOT:
            return simpleInstruction("NOT", offset);
        case OP_EQUAL:
            return simpleInstruction("EQUAL", offset);
        default:
            printf("Unknown opcode %d\n", instruction); 
            return offset + 1;
    }
}
