#include "../includes/vm.h"
#include "../includes/chunk.h"
#include "../includes/common.h"
#include "../includes/debug.h"
#include "../includes/object.h"
#include "../includes/value.h"
#include "../includes/table.h"
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define READ_BYTE(vmpointer) (*vmpointer->ip++)
#define READ_CONSTANT(vmpointer) \
    (vmpointer->chunk->constants.values[READ_BYTE(vmpointer)])
#define READ_LONG_CONSTANT(vmpointer) \
    (vmpointer->chunk->constants.values[(READ_BYTE(vmpointer) | READ_BYTE(vmpointer) << 8)])

void initVM(VM* vm) {
    vm->ObjHead = NULL;
    initTable(&vm->strings);
    resetStack(vm);
}

void loadChunk(VM* vm, Chunk* chunk) {
    vm->chunk = chunk;
    vm->ip = chunk->code;
}

void freeVM(VM* vm) {
    freeObjects(vm);
    freeTable(&vm->strings);
}

void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
}

static inline void push(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++; 
}

static inline Value pop(VM* vm) {
    return *(--vm->stackTop);
}

static inline Value peek(VM* vm, int offset) {
    return vm->stackTop[-1 - offset];
}



static void runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t ins = vm->ip - vm->chunk->code - 1;
    int line = vm->chunk->lines[ins];
    fprintf(stderr, "Line %d: In Script\n", line);
    resetStack(vm);
}

static inline bool isFalsey(Value value) {
    return CHECK_NIL(value) || (CHECK_BOOLEAN(value) && !AS_BOOL(value));
}

static bool isEqual(Value value1, Value value2) {
    if (value1.type != value2.type) return false;

    switch (value1.type) {
        case VAL_NUMBER: return AS_NUMBER(value1) == AS_NUMBER(value2);
        case VAL_BOOL: return AS_BOOL(value1) == AS_BOOL(value2);
        case VAL_NIL: return true;
        case VAL_OBJ: return AS_OBJ(value1) == AS_OBJ(value2);
        default: return false;
    }
}

/*
 
static Value read_constant(VM* vm) {
    return vm->chunk->constants.values[READ_BYTE(vm)];
}

*/
/*
 
static Value read_long_constant(VM* vm) {
    uint8_t high = READ_BYTE(vm);
    uint8_t low = READ_BYTE(vm);

    uint16_t constantIndex = high | low << 8;
    return vm->chunk->constants.values[constantIndex];
}

*/

static InterpretResult run(VM* vm) {
    for (;;) {
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
        uint8_t ins = *vm->ip++;        /* Points to instruction about to be executed and stores the current */

        switch (ins) {
            case OP_EOF: {
                return INTERPRET_OK;
            }
            case OP_RET: {
                /* Temporary Operation is to print the top of stack */
                Value value = pop(vm);
                printValue(value);
                printf("\n");
                break;
            }
            case OP_POP: pop(vm); break;
            case OP_CONST: {
                Value constant = READ_CONSTANT(vm);
                push(vm, constant);
                break;
            }
            case OP_CONST_LONG: {
                Value constant = READ_LONG_CONSTANT(vm);
                push(vm, constant);
                break;
            }
            case OP_ADD: { 
                if (CHECK_NUMBER(peek(vm, 0)) && CHECK_NUMBER(peek(vm, 1))) {
                    /* No storing operands because addition is commutative */
                    push(vm, NATIVE_TO_NUMBER(AS_NUMBER(pop(vm)) + AS_NUMBER(pop(vm))));
                } else if (CHECK_STRING(peek(vm, 0)) && CHECK_STRING(peek(vm, 1))) {
                    push(vm, OBJ(strConcat(vm, pop(vm), pop(vm))));
                } else {
                    /* Runtime Error */
                    runtimeError(vm, "Error : Expected Numeric or String Operand to '+'");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUB: {
                if (CHECK_NUMBER(peek(vm, 0)) && CHECK_NUMBER(peek(vm, 1))) {
                    Value operand1 = pop(vm);
                    Value operand2 = pop(vm);
                    push(vm, NATIVE_TO_NUMBER(AS_NUMBER(operand2) - AS_NUMBER(operand1)));
                } else {
                    /* Runtime Error */
                    runtimeError(vm, "Error : Expected Numeric Operand to '-'");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_MUL: { 
                if (CHECK_NUMBER(peek(vm, 0)) && CHECK_NUMBER(peek(vm, 1))) {
                    /* No storing operands because multiplication is commutative */
                    push(vm, NATIVE_TO_NUMBER(AS_NUMBER(pop(vm)) * AS_NUMBER(pop(vm))));
                } else {
                    /* Runtime Error */
                    runtimeError(vm, "Error : Expected Numeric Operand to '*'");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_DIV: { 
                if (CHECK_NUMBER(peek(vm, 0)) && CHECK_NUMBER(peek(vm, 1))) {
                    Value operand1 = pop(vm);
                    Value operand2 = pop(vm);

                    double op1 = AS_NUMBER(operand1);
                    if (op1 == 0l) {
                        runtimeError(vm, "Error : Division By 0");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(vm, NATIVE_TO_NUMBER(AS_NUMBER(operand2) / op1));
                } else {
                    
                    runtimeError(vm, "Error : Expected Numeric Operand to '/'");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_POW: {
                if (CHECK_NUMBER(peek(vm, 0)) && CHECK_NUMBER(peek(vm, 1))) {
                    Value operand1 = pop(vm);
                    Value operand2 = pop(vm);
                    push(vm, NATIVE_TO_NUMBER(pow(AS_NUMBER(operand2), AS_NUMBER(operand1))));
                } else {
                    /* Runtime Error */
                    runtimeError(vm, "Error : Expected Numeric Operand to '^'");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_NEGATE: {
                if (CHECK_NUMBER(peek(vm, 0))) {
                    push(vm, NATIVE_TO_NUMBER(-AS_NUMBER(pop(vm))));
                } else {
                    /* Runtime Error */
                    runtimeError(vm, "Error : Expected Numeric Operand to unary negation");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_NOT: {
                push(vm, NATIVE_TO_BOOLEAN(isFalsey(pop(vm))));
                break;
            }
            case OP_EQUAL: {
                push(vm, NATIVE_TO_BOOLEAN(isEqual(pop(vm), pop(vm))));
                break;
            }
            case OP_NOT_EQ: {
                push(vm, NATIVE_TO_BOOLEAN(!isEqual(pop(vm), pop(vm))));
                break;
            }
            case OP_NIL: {
                push(vm, NIL());
                break;
            }
            case OP_FALSE: {
                push(vm, NATIVE_TO_BOOLEAN(false));
                break;
            }
            case OP_TRUE: {
                push(vm, NATIVE_TO_BOOLEAN(true));
                break;
            }
            case OP_AND: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                if (isFalsey(operand2)) {
                    push(vm, operand2);
                    break;
                }
                push(vm, operand1);
                break;
            } 
            case OP_OR: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);
                
                if (!isFalsey(operand2)) {
                    push(vm, operand2);
                    break;
                }

                if (isFalsey(operand1)) {
                    push(vm, operand2);
                    break;
                } 
                    
                push(vm, operand1);

                break;
            }
            case OP_GREATER: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                if (!CHECK_NUMBER(operand2) || !CHECK_NUMBER(operand1)) {
                    runtimeError(vm, "Error : Expected Numeric Operand To '>'");
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(vm, NATIVE_TO_BOOLEAN(AS_NUMBER(operand2) > AS_NUMBER(operand1)));
                break;
            }
            case OP_GREATER_EQ: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                if (!CHECK_NUMBER(operand2) || !CHECK_NUMBER(operand1)) {
                    runtimeError(vm, "Error : Expected Numeric Operand To '>='");
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(vm, NATIVE_TO_BOOLEAN(AS_NUMBER(operand2) >= AS_NUMBER(operand1)));
                break;
            }
            case OP_LESSER: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                if (!CHECK_NUMBER(operand2) || !CHECK_NUMBER(operand1)) {
                    runtimeError(vm, "Error : Expected Numeric Operand To '<'");
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(vm, NATIVE_TO_BOOLEAN(AS_NUMBER(operand2) < AS_NUMBER(operand1)));
                break;
            }
            case OP_LESSER_EQ: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                if (!CHECK_NUMBER(operand2) || !CHECK_NUMBER(operand1)) {
                    runtimeError(vm, "Error : Expected Numeric Operand To '<='");
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(vm, NATIVE_TO_BOOLEAN(AS_NUMBER(operand2) <= AS_NUMBER(operand1)));
                break;
            }
            default:
                printf("Unknown Instruction %ld\n", (long)ins);
                printf("Next : %ld\n", (long)*vm->ip);
                printf("Previous : %ld\n",(long)vm->ip[-2]);
                break;
        }
    }
}

InterpretResult interpret(VM* vm) {
    return run(vm);
}
