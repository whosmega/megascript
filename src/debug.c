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

int callInstruction(Chunk* chunk, int offset) {
    printf("%-16s %4d %4d\n", "CALL", chunk->code[offset + 1], chunk->code[offset + 2]);
    return offset + 3;
}

int returnInstruction(Chunk* chunk, int offset) {
    printf("%-16s %4d\n", "RET", chunk->code[offset + 1]);
    return offset + 2;
}

int constantInstruction(const char* insName, Chunk* chunk, int offset) {
    uint8_t constantIndex = chunk->code[offset + 1];
    printf("%-16s %4d '", insName, constantIndex);
    printValue(chunk->constants.values[constantIndex]);
    printf("'\n");
    return offset + 2;                                      /* return 2 because it has 1 operand specifying
                                                               the constant index in the constant pool */
}

int doubleOperandInstruction(const char* insName, Chunk* chunk, int offset) {
    uint8_t op1 = chunk->code[offset + 1];
    uint8_t op2 = chunk->code[offset + 2];
    printf("%-16s %4d %4d\n", insName, op1, op2);
    return offset + 3;
}

int jumpInstruction(const char* insName, Chunk* chunk, int offset) {
    uint8_t ins = chunk->code[offset + 1];
    printf("%-16s %4d\n", insName, ins);
    return offset + 2;
}


int localInstruction(const char* insName, Chunk* chunk, int offset) {
    uint8_t localIndex = chunk->code[offset + 1];
    printf("%-16s %4d\n", insName, localIndex);
    return offset + 2;
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
        case OP_RETEOF:
            return simpleInstruction("RETEOF", offset);
        case OP_CALL:
            return callInstruction(chunk, offset);
        case OP_RET:
            return returnInstruction(chunk, offset);
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
        case OP_LENGTH:
            return simpleInstruction("LENGTH", offset);
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
        case OP_DEFINE_GLOBAL:
            return constantInstruction("DEFINE_GLOBAL", chunk, offset);
        case OP_DEFINE_LONG_GLOBAL:
            return constantInstruction("DEFINE_LONG_GLOBAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("GET_GLOBAL", chunk, offset);
        case OP_GET_LONG_GLOBAL:
            return constantInstruction("GET_LONG_GLOBAL", chunk, offset);
        case OP_ASSIGN_GLOBAL:
            return constantInstruction("ASSIGN_GLOBAL", chunk, offset);
        case OP_ASSIGN_LONG_GLOBAL:
            return constantInstruction("ASSIGN_LONG_GLOBAL", chunk, offset);
        case OP_PLUS_ASSIGN_GLOBAL:
            return constantInstruction("PLUS_ASSIGN_GLOBAL", chunk, offset);
        case OP_PLUS_ASSIGN_LONG_GLOBAL:
            return constantInstruction("PLUS_ASSIGN_LONG_GLOBAL", chunk, offset);
        case OP_SUB_ASSIGN_GLOBAL:
            return constantInstruction("SUB_ASSIGN_GLOBAL", chunk, offset);
        case OP_SUB_ASSIGN_LONG_GLOBAL:
            return constantInstruction("SUB_ASSIGN_LONG_GLOBAL", chunk, offset);
        case OP_MUL_ASSIGN_GLOBAL:
            return constantInstruction("MUL_ASSIGN_GLOBAL", chunk, offset);
        case OP_MUL_ASSIGN_LONG_GLOBAL:
            return constantInstruction("MUL_ASSIGN_LONG_GLOBAL", chunk, offset);
        case OP_DIV_ASSIGN_GLOBAL:
            return constantInstruction("DIV_ASSIGN_GLOBAL", chunk, offset);
        case OP_DIV_ASSIGN_LONG_GLOBAL:
            return constantInstruction("DIV_ASSIGN_LONG_GLOBAL", chunk, offset);
        case OP_POW_ASSIGN_GLOBAL:
            return constantInstruction("POW_ASSIGN_GLOBAL", chunk, offset);
        case OP_POW_ASSIGN_LONG_GLOBAL:
            return constantInstruction("POW_ASSIGN_LONG_GLOBAL", chunk, offset);
        case OP_ASSIGN_LOCAL:
            return localInstruction("ASSIGN_LOCAL", chunk, offset);
        case OP_PLUS_ASSIGN_LOCAL:
            return localInstruction("PLUS_ASSIGN_LOCAL", chunk, offset);
        case OP_MINUS_ASSIGN_LOCAL:
            return localInstruction("MINUS_ASSIGN_LOCAL", chunk, offset);
        case OP_MUL_ASSIGN_LOCAL:
            return localInstruction("MUL_ASSIGN_LOCAL", chunk, offset);
        case OP_DIV_ASSIGN_LOCAL:
            return localInstruction("DIV_ASSIGN_LOCAL", chunk, offset);
        case OP_POW_ASSIGN_LOCAL:
            return localInstruction("POW_ASSIGN_LOCAL", chunk, offset);
        case OP_GET_LOCAL:
            return localInstruction("GET_LOCAL", chunk, offset);
        case OP_POPN: {
            return localInstruction("POPN", chunk, offset);
        }
        case OP_JMP: {
            return jumpInstruction("JMP", chunk, offset);
        }
        case OP_JMP_AND: {
            return jumpInstruction("JMP_AND", chunk, offset);
        }
        case OP_JMP_OR: {
            return jumpInstruction("JMP_OR", chunk, offset);
        }
        case OP_JMP_FALSE: {
            return jumpInstruction("JMP_FALSE", chunk, offset);
        }
        case OP_JMP_BACK: {
            return jumpInstruction("JMP_BACK", chunk, offset);
        }
        case OP_ARRAY: {
            return simpleInstruction("ARRAY (emit)", offset);
        }
        case OP_ARRINIT: {
            return localInstruction("ARRINIT", chunk, offset);
        }
        case OP_ARRAY_INS: {
            return simpleInstruction("ARRAY_INS", offset);
        }
        case OP_ARRAY_PINS: {
            return simpleInstruction("ARRAY_PINS", offset);
        }
        case OP_ARRAY_GET: {
            return simpleInstruction("ARRAY_GET", offset);
        }
        case OP_ARRAY_MOD: {
            return simpleInstruction("ARRAY_MOD", offset);
        }
        case OP_ARRAY_PLUS_MOD: {
            return localInstruction("ARRAY_PLUS_MOD", chunk, offset);
        }
        case OP_ARRAY_MIN_MOD: {
            return localInstruction("ARRAY_SUB_MOD", chunk, offset);
        }
        case OP_ARRAY_MUL_MOD: {
            return localInstruction("ARRAY_MUL_MOD", chunk, offset);
        }
        case OP_ARRAY_DIV_MOD: {
            return localInstruction("ARRAY_DIV_MOD", chunk, offset);
        }
        case OP_ARRAY_POW_MOD: {
            return localInstruction("ARRAY_POW_MOD", chunk, offset);
        }
        case OP_ARRAY_RANGE: {
            return simpleInstruction("ARRAY_RANGE", offset);
        }
        case OP_POP: {
            return simpleInstruction("POP", offset);
        }
        case OP_NIL:
            return simpleInstruction("NIL (emit)", offset);
        case OP_ZERO: {
            return simpleInstruction("ZERO (emit)", offset);
        }
        case OP_MIN1: {
            return simpleInstruction("MIN1 (emit)", offset);
        }
        case OP_PLUS1: {
            return simpleInstruction("PLUS1 (emit)", offset);
        }
        case OP_ITERATE: { 
            return localInstruction("ITERATE", chunk, offset); 
        }
        case OP_ITERATE_VALUE: {
            return doubleOperandInstruction("ITERATE_VALUE", chunk, offset);
        }              
        case OP_ITERATE_NUM: {
            return localInstruction("ITERATE_NUM", chunk, offset);
        }
        default:
            printf("Unknown opcode %d\n", instruction); 
            return offset + 1;
    }
}
