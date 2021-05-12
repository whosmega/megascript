#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "value.h"
#include <math.h>
#include <stdio.h>

#define READ_BYTE(vmpointer) (*vmpointer->ip++)

void initVM(VM* vm, Chunk* chunk) {
    vm->chunk = chunk;
    vm->ip = chunk->code;
    vm->insInterpreted = 0;
    resetStack(vm);
}

void freeVM(VM* vm) {
    
}

void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
}

void push(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++; 
}

Value pop(VM* vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

static Value read_constant(VM* vm) {
    return vm->chunk->constants.values[READ_BYTE(vm)];
}

static Value read_long_constant(VM* vm) {
    uint8_t high = READ_BYTE(vm);
    uint8_t low = READ_BYTE(vm);

    uint16_t constantIndex = high | low << 8;
    return vm->chunk->constants.values[constantIndex];
}

static InterpretResult run(VM* vm) {
    for (;;) {
        if (vm->insInterpreted >= vm->chunk->elem_count) {
            return INTERPRET_YIELD;
        }

        #ifdef DEBUG_TRACE_EXECUTION
            printf("          ");
            for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
                printf("[ ");
                printValue(*slot);
                printf(" ]");
            }
            printf("\n");
            dissembleInstruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
            
        #endif
        uint8_t ins = *vm->ip++;
        vm->insInterpreted++;
        switch (ins) {
            case OP_EOF: {
                return INTERPRET_OK;
            }
            case OP_RET: {
                break;
            }
            case OP_CONST: {
                Value constant = read_constant(vm);
                push(vm, constant);
                break;
            }
            case OP_CONST_LONG: {
                Value constant = read_long_constant(vm);
                push(vm, constant);
                break;
            }
            case OP_ADD: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                push(vm, operand2 + operand1);
                break;
            }
            case OP_SUB: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                push(vm, operand2 - operand1);
                break;
            }
            case OP_MUL: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                push(vm, operand2 * operand1);
                break;
            }
            case OP_DIV: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                push(vm, operand2 / operand1);
                break;
            }
            case OP_POW: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                push(vm, pow(operand2, operand1));
                break;
            }
            case OP_NEGATE: {
                push(vm, -pop(vm));
                break;
            }
        }
    }
}

InterpretResult interpret(VM* vm) {
    return run(vm);
}
