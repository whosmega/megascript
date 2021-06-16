#include "../includes/vm.h"
#include "../includes/chunk.h"
#include "../includes/common.h"
#include "../includes/debug.h"
#include "../includes/object.h"
#include "../includes/value.h"
#include "../includes/table.h"
#include "../includes/globals.h"
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define READ_BYTE(frameptr) (*frame->ip++)
#define READ_LONG_BYTE(frameptr) (READ_BYTE(frameptr) | READ_BYTE(frameptr) << 8)
#define READ_CONSTANT(frameptr) \
    (frameptr->function->chunk.constants.values[READ_BYTE(frameptr)])
#define READ_LONG_CONSTANT(frameptr) \
    (frameptr->function->chunk.constants.values[READ_LONG_BYTE(frameptr)])

#define ASSIGN_GLOBAL(vmpointer, frameptr, valueName, valueFeeder) \
    valueName = READ_CONSTANT(frameptr); \
    bool found = getTable(&vmpointer->globals, AS_STRING(valueName), &valueFeeder); \
    if (!found) { \
      msapi_runtimeError(vmpointer, "Error : Attempt to assign undefined global '%s'", AS_NATIVE_STRING(valueName)); \
      return INTERPRET_RUNTIME_ERROR; \
    }
 
#define ASSIGN_LONG_GLOBAL(vmpointer, frameptr, valueName, valueFeeder) \
    valueName = READ_LONG_CONSTANT(frameptr); \
    bool found = getTable(&vmpointer->globals, AS_STRING(valueName), &valueFeeder); \
    if (!found) { \
      msapi_runtimeError(vmpointer, "Error : Attempt to assign undefined global '%s'", AS_NATIVE_STRING(valueName)); \
      return INTERPRET_RUNTIME_ERROR; \
    }

#define push(vmptr, v) if (vmptr->stackSize == STACK_MAX) {\
                            msapi_runtimeError(vmptr, "Stack Overflow"); \
                            return INTERPRET_RUNTIME_ERROR; \
                       } \
                       _push_(vmptr, v) \

#define pushn(vmptr, v, c) if (vmptr->stackSize + c >= STACK_MAX) { \
                                msapi_runtimeError(vmptr, "Stack Overflow"); \
                                return INTERPRET_RUNTIME_ERROR; \
                           } \
                           _pushn_(vmptr, v, c)

void initVM(VM* vm) {
    vm->frameCount = 0;
    vm->ObjHead = NULL;
    initTable(&vm->strings);
    initTable(&vm->globals);
    vm->stackSize = 0;
    resetStack(vm);

    // Setup globals 
    injectGlobals(vm);
}

void freeVM(VM* vm) {
    freeTable(&vm->strings);
    freeTable(&vm->globals);
    freeObjects(vm);
}

void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
}

/*                      VM API                      */

void msapi_runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "\033[0;31m");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\033[0m");
    va_end(args);
    fputs("\n", stderr);
    
    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    size_t ins = frame->ip - frame->function->chunk.code - 1;
    int line = frame->function->chunk.lines[ins];
    fprintf(stderr, "Line %d: ", line);
    fprintf(stderr, "\033[0;36m");
    fprintf(stderr, "In Script");
    fprintf(stderr, "\033[0m\n");


    printf("\n\033[0;32mCall Stack Traceback:\033[0m\n");
    printf("In function:\033[0;36m %s\033[0m\n", vm->frames[vm->frameCount - 1].function->name->allocated);
    
    for (int i = vm->frameCount - 2; i != 0; i--) {
        printf("Called by:\033[0;36m %s\033[0m\n", vm->frames[i].function->name->allocated);
    }
    resetStack(vm);
}


static inline void _push_(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
    vm->stackSize++;
}

static inline Value pop(VM* vm) {
    vm->stackSize--;
    return *(--vm->stackTop);
}

static inline void _pushn_(VM* vm, Value value, unsigned int count) {
    for (int i = 0; i < count; i++) {
        _push_(vm, value);
    }
    vm->stackSize += count;
}

static inline void popn(VM* vm, unsigned int count) {
    vm->stackTop -= count;
    vm->stackSize -= count;
}

static inline Value peek(VM* vm, int offset) {
    return vm->stackTop[-1 - offset];
}

static inline Value* peekptr(VM* vm, int offset) {
    return &vm->stackTop[-1 - offset];
}

static inline void popCallFrame(VM* vm) {
    vm->frameCount--;
    pop(vm);        // pop the function
}

static inline bool isFalsey(Value value) {
    return CHECK_NIL(value) || (CHECK_BOOLEAN(value) && !AS_BOOL(value));
}

bool msapi_pushCallFrame(VM* vm, ObjFunction* function) {
    if (vm->frameCount == FRAME_MAX) {
        msapi_runtimeError(vm, "Error : Max Call-Depth reached (Stack Overflow)");
        return false;
    }
    
    // Function should already be pushed on stack
    CallFrame frame;
    frame.function = function;
    frame.ip = function->chunk.code;
    frame.slotPtr = vm->stackTop;     

    vm->frames[vm->frameCount] = frame;
    vm->frameCount++;
    return true;
}

void msapi_popCallFrame(VM* vm) {
    return popCallFrame(vm);
}

bool msapi_isFalsey(Value value) {
    return isFalsey(value);
}

bool msapi_isEqual(Value value1, Value value2) {
    if (value1.type != value2.type) return false;

    switch (value1.type) {
        case VAL_NUMBER: return AS_NUMBER(value1) == AS_NUMBER(value2);
        case VAL_BOOL: return AS_BOOL(value1) == AS_BOOL(value2);
        case VAL_NIL: return true;
        case VAL_OBJ: return AS_OBJ(value1) == AS_OBJ(value2);
        default: return false;
    }
}

void msapi_push(VM* vm, Value value) {
    _push_(vm, value);
}

Value msapi_pop(VM* vm) {
    return pop(vm);
}

Value msapi_peek(VM* vm, unsigned int index) {
    return peek(vm, index);
}

Value* msapi_peekptr(VM* vm, unsigned int index) {
    return peekptr(vm, index);
}

void msapi_pushn(VM* vm, Value value, unsigned int count) {
    _pushn_(vm, value, count);
}

void msapi_popn(VM* vm, unsigned int count) {
    return popn(vm, count);
}

//--------------------------------------------


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

static InterpretResult run(register VM* vm) {
    register CallFrame* frame = &vm->frames[vm->frameCount - 1];
    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
            printf("          ");
            for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
                printf("[ ");
                printValue(*slot);
                printf(" ]");
            }
            printf("\n");
            dissembleInstruction(&frame->function->chunk, (int)(frame->ip - frame->function->chunk.code));
            
        #endif
        uint8_t ins = READ_BYTE(frame);        /* Points to instruction about to be executed and stores the current */

        switch (ins) {
            case OP_ZERO: push(vm, NATIVE_TO_NUMBER(0)); break; 
            case OP_MIN1: push(vm, NATIVE_TO_NUMBER(-1)); break;
            case OP_PLUS1: push(vm, NATIVE_TO_NUMBER(1)); break;
            case OP_ITERATE: {
                uint8_t indexIndex = READ_BYTE(vm);
                Value* oldIndex = peekptr(vm, 0);
                Value valArray = peek(vm, 1);  
                Value* indexValue = &frame->slotPtr[indexIndex];

                if (!CHECK_ARRAY(valArray)) {
                    msapi_runtimeError(vm, "Expected an array for iteration value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                int newIndex = AS_NUMBER(*oldIndex) + 1;
                ObjArray* array = AS_ARRAY(valArray);

                // Check if the index outnumbers the element count 
                if (AS_NUMBER(*oldIndex) + 1 > array->array.count - 1) {
                    // We cannot continue the iteration 
                    push(vm, NATIVE_TO_BOOLEAN(false));
                } else {
                    // Push the new index value 
                    *oldIndex = NATIVE_TO_NUMBER(newIndex);
                    // We can continue the iteration
                    push(vm, NATIVE_TO_BOOLEAN(true));
                    // Update the index variable 
                    *indexValue = NATIVE_TO_NUMBER(newIndex);
                }

                break;
 

            }
            case OP_ITERATE_VALUE: {
                uint8_t indexIndex = READ_BYTE(vm);
                uint8_t valueIndex = READ_BYTE(vm);
                Value* indexValue = &frame->slotPtr[indexIndex];
                Value* valueValue = &frame->slotPtr[valueIndex];

                Value* oldIndex = peekptr(vm, 0);
                Value valArray = peek(vm, 1);

                if (!CHECK_ARRAY(valArray)) {
                    msapi_runtimeError(vm, "Expected an array for iteration value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                int newIndex = AS_NUMBER(*oldIndex) + 1;
                ObjArray* array = AS_ARRAY(valArray);

                // Check if the index outnumbers the element count 
                if (AS_NUMBER(*oldIndex) + 1 > array->array.count - 1) {
                    // We cannot continue the iteration 
                    push(vm, NATIVE_TO_BOOLEAN(false));
                } else {
                    // Push the new index value 
                    *oldIndex = NATIVE_TO_NUMBER(newIndex);                    
                    // We can continue the iteration
                    push(vm, NATIVE_TO_BOOLEAN(true));
                    // Update the index variable 
                    *indexValue = NATIVE_TO_NUMBER(newIndex);
                    // Update the value variable 
                    *valueValue = array->array.values[newIndex];
                }

                break;
                
            }
            case OP_ITERATE_NUM: {
                uint8_t indexIndex = READ_BYTE(vm);
                
                Value* indexValue = &frame->slotPtr[indexIndex];
                Value incrementHolder = peek(vm, 0);
                Value stopHolder = peek(vm, 1);
                Value* startHolder = peekptr(vm, 2);
                Value startHolderV = *startHolder;

                if (!CHECK_NUMBER(startHolderV)) {
                    msapi_runtimeError(vm, "Start Value in numeric for loop is expected to be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!CHECK_NUMBER(stopHolder)) {
                    msapi_runtimeError(vm, "Stop value in numeric for loop is expected to be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!CHECK_NUMBER(incrementHolder)) {
                    msapi_runtimeError(vm, "Increment value in numeric for loop is expected to be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
               
                if (AS_NUMBER(incrementHolder) == 0) {
                    msapi_runtimeError(vm, "Increment Value is 0");

                    return INTERPRET_RUNTIME_ERROR;
                }

                if (AS_NUMBER(startHolderV) <= AS_NUMBER(stopHolder) && AS_NUMBER(incrementHolder) > 0) {
                   push(vm, NATIVE_TO_BOOLEAN(true));
                } else if (AS_NUMBER(startHolderV) >= AS_NUMBER(stopHolder) && AS_NUMBER(incrementHolder) < 0) {
                    push(vm, NATIVE_TO_BOOLEAN(true));
                } else {
                    push(vm, NATIVE_TO_BOOLEAN(false));
                }
                
                *indexValue = startHolderV;         // Assign old value of start 
                // Increment start
                *startHolder = NATIVE_TO_NUMBER(AS_NUMBER(startHolderV) + AS_NUMBER(incrementHolder));
                break;
            } 
            case OP_ARRAY: {
                push(vm, OBJ(allocateArray(vm)));
                break;
            }
            case OP_ARRAY_INS: {
                Value value = pop(vm);
                Value array = peek(vm, 0);
                writeValueArray(&AS_ARRAY(array)->array, value);
                break;
            }
            case OP_ARRAY_PINS: {
                Value value = pop(vm);
                Value array = pop(vm);
                writeValueArray(&AS_ARRAY(array)->array, value);
                break;
            }
            case OP_ARRAY_MOD: {
                Value value = pop(vm); 
                Value index = pop(vm); 
                Value valArray = pop(vm);

                if (!CHECK_ARRAY(valArray)) {
                    msapi_runtimeError(vm, "Attempt to modify a non-array value");
                    return INTERPRET_RUNTIME_ERROR;
                }
   
                ObjArray* array = AS_ARRAY(valArray);

                if (!CHECK_NUMBER(index)) {
                    msapi_runtimeError(vm, "Expected a number for array index");
                    return INTERPRET_RUNTIME_ERROR;
                }

                double numIndex = AS_NUMBER(index);

                if (fmod(numIndex, 1) != 0 || numIndex < 0) {
                    msapi_runtimeError(vm, "Array Index is expected to be a positive integer, got %g", numIndex);
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (numIndex + 1 > array->array.count) {
                    msapi_runtimeError(vm, "Array Index %g out of range", numIndex);

                    return INTERPRET_RUNTIME_ERROR;
                }

                array->array.values[(int)numIndex] = value;
                break;
            }
            case OP_ARRAY_PLUS_MOD: {
                Value value = pop(vm); 
                Value index = pop(vm); 
                Value valArray = pop(vm);

                if (!CHECK_ARRAY(valArray)) {
                    msapi_runtimeError(vm, "Attempt to modify a non-array value");
                    return INTERPRET_RUNTIME_ERROR;
                }
   
                ObjArray* array = AS_ARRAY(valArray);

                if (!CHECK_NUMBER(index)) {
                    msapi_runtimeError(vm, "Expected a number for array index");
                    return INTERPRET_RUNTIME_ERROR;
                }

                double numIndex = AS_NUMBER(index);

                if (fmod(numIndex, 1) != 0 || numIndex < 0) {
                    msapi_runtimeError(vm, "Array Index is expected to be a positive integer, got %g", numIndex);
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (numIndex + 1 > array->array.count) {
                    msapi_runtimeError(vm, "Array Index %g out of range", numIndex);

                    return INTERPRET_RUNTIME_ERROR;
                }

                Value oldValue = array->array.values[(unsigned int)numIndex];
                
                if (CHECK_NUMBER(oldValue) && CHECK_NUMBER(value)) {
                    array->array.values[(unsigned int)numIndex] = NATIVE_TO_NUMBER(AS_NUMBER(oldValue) + AS_NUMBER(value));
                } else if (CHECK_STRING(oldValue) && CHECK_STRING(value)) {
                    array->array.values[(unsigned int)numIndex] = OBJ(strConcat(vm, oldValue, value)); 
                } else {
                    msapi_runtimeError(vm, "Attempt call '+=' on a non-numeric/string value");
                    return INTERPRET_RUNTIME_ERROR;

                }
                break;
            }
            case OP_ARRAY_MIN_MOD: {
                Value value = pop(vm); 
                Value index = pop(vm); 
                Value valArray = pop(vm);

                if (!CHECK_ARRAY(valArray)) {
                    msapi_runtimeError(vm, "Attempt to modify a non-array value");
                    return INTERPRET_RUNTIME_ERROR;
                }
   
                ObjArray* array = AS_ARRAY(valArray);

                if (!CHECK_NUMBER(index)) {
                    msapi_runtimeError(vm, "Expected a number for array index");
                    return INTERPRET_RUNTIME_ERROR;
                }

                double numIndex = AS_NUMBER(index);

                if (fmod(numIndex, 1) != 0 || numIndex < 0) {
                    msapi_runtimeError(vm, "Array Index is expected to be a positive integer, got %g", numIndex);
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (numIndex + 1 > array->array.count) {
                    msapi_runtimeError(vm, "Array Index %g out of range", numIndex);

                    return INTERPRET_RUNTIME_ERROR;
                }

                Value oldValue = array->array.values[(unsigned int)numIndex];
                
                if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                    msapi_runtimeError(vm, "Attempt call '-=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;

                }

                array->array.values[(unsigned int)numIndex] = NATIVE_TO_NUMBER(AS_NUMBER(oldValue) - AS_NUMBER(value));

                break;
            }
            case OP_ARRAY_MUL_MOD: {
                Value value = pop(vm); 
                Value index = pop(vm); 
                Value valArray = pop(vm);

                if (!CHECK_ARRAY(valArray)) {
                    msapi_runtimeError(vm, "Attempt to modify a non-array value");
                    return INTERPRET_RUNTIME_ERROR;
                }
   
                ObjArray* array = AS_ARRAY(valArray);

                if (!CHECK_NUMBER(index)) {
                    msapi_runtimeError(vm, "Expected a number for array index");
                    return INTERPRET_RUNTIME_ERROR;
                }

                double numIndex = AS_NUMBER(index);

                if (fmod(numIndex, 1) != 0 || numIndex < 0) {
                    msapi_runtimeError(vm, "Array Index is expected to be a positive integer, got %g", numIndex);
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (numIndex + 1 > array->array.count) {
                    msapi_runtimeError(vm, "Array Index %g out of range", numIndex);

                    return INTERPRET_RUNTIME_ERROR;
                }

                Value oldValue = array->array.values[(unsigned int)numIndex];
                
                if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                    msapi_runtimeError(vm, "Attempt call '-=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;

                }

                array->array.values[(unsigned int)numIndex] = NATIVE_TO_NUMBER(AS_NUMBER(oldValue) * AS_NUMBER(value));

                break;
            }
            case OP_ARRAY_DIV_MOD: {
                Value value = pop(vm); 
                Value index = pop(vm); 
                Value valArray = pop(vm);

                if (!CHECK_ARRAY(valArray)) {
                    msapi_runtimeError(vm, "Attempt to modify a non-array value");
                    return INTERPRET_RUNTIME_ERROR;
                }
   
                ObjArray* array = AS_ARRAY(valArray);

                if (!CHECK_NUMBER(index)) {
                    msapi_runtimeError(vm, "Expected a number for array index");
                    return INTERPRET_RUNTIME_ERROR;
                }

                double numIndex = AS_NUMBER(index);

                if (fmod(numIndex, 1) != 0 || numIndex < 0) {
                    msapi_runtimeError(vm, "Array Index is expected to be a positive integer, got %g", numIndex);
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (numIndex + 1 > array->array.count) {
                    msapi_runtimeError(vm, "Array Index %g out of range", numIndex);

                    return INTERPRET_RUNTIME_ERROR;
                }

                Value oldValue = array->array.values[(unsigned int)numIndex];
                
                if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                    msapi_runtimeError(vm, "Attempt call '-=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;

                }

                if (AS_NUMBER(value) == 0) {
                    msapi_runtimeError(vm, "Cannot divide by 0");
                    return INTERPRET_RUNTIME_ERROR;
                }

                array->array.values[(unsigned int)numIndex] = NATIVE_TO_NUMBER(AS_NUMBER(oldValue) / AS_NUMBER(value));

                break;
            }

            case OP_ARRAY_POW_MOD: {
                Value value = pop(vm); 
                Value index = pop(vm); 
                Value valArray = pop(vm);

                if (!CHECK_ARRAY(valArray)) {
                    msapi_runtimeError(vm, "Attempt to modify a non-array value");
                    return INTERPRET_RUNTIME_ERROR;
                }
   
                ObjArray* array = AS_ARRAY(valArray);

                if (!CHECK_NUMBER(index)) {
                    msapi_runtimeError(vm, "Expected a number for array index");
                    return INTERPRET_RUNTIME_ERROR;
                }

                double numIndex = AS_NUMBER(index);

                if (fmod(numIndex, 1) != 0 || numIndex < 0) {
                    msapi_runtimeError(vm, "Array Index is expected to be a positive integer, got %g", numIndex);
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (numIndex + 1 > array->array.count) {
                    msapi_runtimeError(vm, "Array Index %g out of range", numIndex);

                    return INTERPRET_RUNTIME_ERROR;
                }

                Value oldValue = array->array.values[(unsigned int)numIndex];
                
                if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                    msapi_runtimeError(vm, "Attempt call '-=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;

                }

                array->array.values[(unsigned int)numIndex] = NATIVE_TO_NUMBER(pow(AS_NUMBER(oldValue), AS_NUMBER(value)));

                break;
            }



            case OP_ARRAY_GET: {
                Value index = pop(vm);
                Value valArray = pop(vm);  

                if (!CHECK_ARRAY(valArray)) {
                    msapi_runtimeError(vm, "Attempt to index a non-array value");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjArray* array = AS_ARRAY(valArray);

                if (!CHECK_NUMBER(index)) {
                    msapi_runtimeError(vm, "Expected a number for array index");
                    return INTERPRET_RUNTIME_ERROR;
                }

                double numIndex = AS_NUMBER(index);

                if (fmod(numIndex, 1) != 0 || numIndex < 0) {
                    msapi_runtimeError(vm, "Array Index is expected to be a positive integer, got %g", numIndex);
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (numIndex + 1 > array->array.count) {
                    msapi_runtimeError(vm, "Array Index %g out of range", numIndex);

                    return INTERPRET_RUNTIME_ERROR;
                }

                push(vm, array->array.values[(int)numIndex]);
                break;
            
            }
            case OP_ARRAY_RANGE: {
                Value increment = pop(vm);
                Value stop = pop(vm); 
                Value start = pop(vm); 
                Value arrayValue = peek(vm, 0);

                if (!CHECK_NUMBER(increment)) {
                    msapi_runtimeError(vm, "Expected increment to be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (AS_NUMBER(increment) == 0) {
                    msapi_runtimeError(vm, "Increment is 0");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!CHECK_NUMBER(start)) {
                    msapi_runtimeError(vm, "Expected start to be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!CHECK_NUMBER(stop)) {
                    msapi_runtimeError(vm, "Expected stop to be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }

                
                if (AS_NUMBER(increment) > 0) {
                    for (double v = AS_NUMBER(start); v <= AS_NUMBER(stop); v += AS_NUMBER(increment)) {
                        writeValueArray(&AS_ARRAY(arrayValue)->array, NATIVE_TO_NUMBER(v));
                    }
                } else if (AS_NUMBER(increment) < 0) {
                    for (double v = AS_NUMBER(start); v >= AS_NUMBER(stop); v += AS_NUMBER(increment)) {
                        writeValueArray(&AS_ARRAY(arrayValue)->array, NATIVE_TO_NUMBER(v));
                    }
                }

                break;
            }
            case OP_JMP: {
                /* Reads 8 bit */ 
                frame->ip += READ_BYTE(frame);
                break;
            }
            case OP_JMP_LONG: {
                /* Reads 16 bit */ 
                frame->ip += READ_LONG_BYTE(frame);
                break;
            }
            case OP_JMP_OR: {
                uint8_t byte = READ_BYTE(frame);
                if (!isFalsey(peek(vm, 0))) {
                    frame->ip += byte; 
                } else {
                    pop(vm);
                }
                break;
            }
            case OP_JMP_OR_LONG: {
                uint16_t byte = READ_LONG_BYTE(frame);
                if (!isFalsey(peek(vm, 0))) {
                    frame->ip += byte;
                } else {
                    pop(vm);
                }
                break;
            }
            case OP_JMP_FALSE: {
                /* Reads 8 bit */
                uint8_t byte = READ_BYTE(frame);
                if (isFalsey(pop(vm))) {
                    frame->ip += byte;
                }

                break;
            }
            case OP_JMP_FALSE_LONG: {
                /* Reads 16 bit */
                uint16_t byte = READ_LONG_BYTE(frame);
                if (!isFalsey(pop(vm))) break;
                frame->ip += byte;
                break;
            
            }
            case OP_JMP_AND: {
                /* Unlike JMP_FALSE, this doesnt pop the value if its falsey */  
                uint8_t byte = READ_BYTE(frame); 
                if (isFalsey(peek(vm, 0))) {
                    frame->ip += byte; 
                } else {
                    pop(vm);
                }
                break;
            }
            case OP_JMP_AND_LONG: {
                uint16_t byte = READ_LONG_BYTE(frame);
                if (isFalsey(peek(vm, 0))) {
                    frame->ip += byte;
                } else {
                    pop(vm);
                }
                break;
            }
            case OP_JMP_BACK: {
                /* Reads 8 bit */ 
                frame->ip -= READ_BYTE(frame);
                break;
            }
            case OP_JMP_BACK_LONG: {
                /* Reads 16 bit */ 
                frame->ip -= READ_LONG_BYTE(frame);
                break;
            }
            case OP_CALL: {
                uint8_t argArity = READ_BYTE(frame); 
                uint8_t expectedReturns = READ_BYTE(frame);
                Value function = peek(vm, argArity);    // first the function is pushed, the the args, then the call instruction
                if (CHECK_FUNCTION(function)) {
                    bool pushed = msapi_pushCallFrame(vm, AS_FUNCTION(function));
                    if (!pushed) return INTERPRET_RUNTIME_ERROR;

                    frame = &vm->frames[vm->frameCount - 1];
                    frame->expectedReturns = expectedReturns;

                    // the arguments have already been pushed before the the stack 
                    // frame was pushed, so we relocate the callframe pointer to the 
                    // beginning of the variable list
                    frame->slotPtr = peekptr(vm, argArity - 1);
                    uint8_t expectedArgArity = frame->function->arity;

                    if (argArity > expectedArgArity) { 
                        uint8_t extra = argArity - expectedArgArity;
                        if (AS_FUNCTION(function)->variadic) {
                            ObjArray* array = allocateArray(vm); 
                            
                            for (int i = extra - 1; i >= 0; i--) {
                                writeValueArray(&array->array, peek(vm, i));   
                            }
                            
                            popn(vm, extra);
                            push(vm, OBJ(array));
                        } else {
                            popn(vm, extra);
                        }
                        break;
                    } else if (argArity < expectedArgArity) {
                        uint8_t padding = expectedArgArity - argArity;
                        pushn(vm, NIL(), padding);
                    }

                    // if it gets till here, we are in an argArity <= expectedArgArity for sure 
                    if (AS_FUNCTION(function)->variadic) {
                        push(vm, OBJ(allocateArray(vm)));
                    }
                } else if (CHECK_NATIVE_FUNCTION(function)) {
                    NativeFuncPtr ptr = AS_NATIVE_FUNCTION(function)->funcPtr;
                    bool result = (*ptr)(vm, argArity, expectedReturns);
                    if (!result) return INTERPRET_RUNTIME_ERROR;
                } else {
                    msapi_runtimeError(vm, "Attempt to call a non-function value");
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
            case OP_RET: {
                uint8_t numReturns = READ_BYTE(frame);
                
                if (frame->expectedReturns > numReturns) {
                    // Pad extra areas with nil 
                    uint8_t padding = frame->expectedReturns - numReturns;
                    pushn(vm, NIL(), padding);
                    numReturns += padding;
                } else if (numReturns > frame->expectedReturns) {
                    uint8_t count = numReturns - frame->expectedReturns;
                    popn(vm, count);
                    numReturns -= count;
                }
                
                // numReturns should now be equal to expected returns 
                // Return values are already on the stack 
               
                Value returns[numReturns];
                for (int i = 0; i < numReturns; i++) {
                    returns[i] = pop(vm);
                }

                vm->stackTop = frame->slotPtr;
                popCallFrame(vm);
                // Update the frame variable
                frame = &vm->frames[vm->frameCount - 1];
                
                // Push the return values back in reverse 

                for (int i = numReturns - 1; i >= 0; i--) {
                    push(vm, returns[i]);
                }

                break;
            }
            case OP_RETEOF: {
                vm->stackTop = frame->slotPtr;
                popCallFrame(vm);
                return INTERPRET_OK;
            }
            case OP_DEFINE_GLOBAL: {
                Value name = READ_CONSTANT(frame);
                insertTable(&vm->globals, AS_STRING(name), pop(vm));
                break;
            }
            case OP_DEFINE_LONG_GLOBAL: {
                Value name = READ_LONG_CONSTANT(frame);
                insertTable(&vm->globals, AS_STRING(name), pop(vm));
                break;
            }
            case OP_GET_GLOBAL: {
                Value feeder;
                Value name = READ_CONSTANT(frame);
                bool found = getTable(&vm->globals, AS_STRING(name), &feeder);

                push(vm, found ? feeder : NIL());
                break;
            }
            case OP_GET_LONG_GLOBAL: {
                Value feeder;
                Value name = READ_LONG_CONSTANT(frame);
                bool found = getTable(&vm->globals, AS_STRING(name), &feeder);

                push(vm, found ? feeder : NIL());
                break;
            }
            case OP_ASSIGN_GLOBAL: {
                Value name;
                Value feeder;
                ASSIGN_GLOBAL(vm, frame, name, feeder);
                insertTable(&vm->globals, AS_STRING(name), pop(vm));
                break;
            }
            case OP_ASSIGN_LONG_GLOBAL: {
                Value name;
                Value feeder;
                ASSIGN_LONG_GLOBAL(vm, frame, name, feeder);
                insertTable(&vm->globals, AS_STRING(name), pop(vm));
                break;
            }
            case OP_PLUS_ASSIGN_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = pop(vm);
                ASSIGN_GLOBAL(vm, frame, name, feeder);
            
                if (CHECK_NUMBER(feeder) && CHECK_NUMBER(increment)) {
                    insertTable(&vm->globals, AS_STRING(name), NATIVE_TO_NUMBER(
                            AS_NUMBER(feeder) + AS_NUMBER(increment)
                    ));

                } else if (CHECK_STRING(feeder) && CHECK_STRING(increment)) {
                    insertTable(&vm->globals, AS_STRING(name), OBJ(
                                strConcat(vm, feeder, increment)
                    ));
                } else {
                    msapi_runtimeError(vm, "Attempt call '+=' on a non-numeric/string value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                

                break;
            }
            case OP_PLUS_ASSIGN_LONG_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = pop(vm);
                ASSIGN_LONG_GLOBAL(vm, frame, name, feeder);
                
                if (CHECK_NUMBER(feeder) && CHECK_NUMBER(increment)) {
                    insertTable(&vm->globals, AS_STRING(name), NATIVE_TO_NUMBER(
                            AS_NUMBER(feeder) + AS_NUMBER(increment)
                    ));

                } else if (CHECK_STRING(feeder) && CHECK_STRING(increment)) {
                    insertTable(&vm->globals, AS_STRING(name), OBJ(
                                strConcat(vm, feeder, increment)
                    ));
                } else {
                    msapi_runtimeError(vm, "Attempt call '+=' on a non-numeric/string value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                break;

            }
            case OP_SUB_ASSIGN_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = pop(vm);
                ASSIGN_GLOBAL(vm, frame, name, feeder);
                
                if (!CHECK_NUMBER(feeder)) {
                    msapi_runtimeError(vm, "Attempt call '-=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!CHECK_NUMBER(increment)) {
                    msapi_runtimeError(vm, "Non-numeric value to '-='");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                insertTable(&vm->globals, AS_STRING(name), NATIVE_TO_NUMBER(
                            AS_NUMBER(feeder) - AS_NUMBER(increment)
                ));
                break;

            }
            case OP_SUB_ASSIGN_LONG_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = pop(vm);
                ASSIGN_LONG_GLOBAL(vm, frame, name, feeder);
                
                if (!CHECK_NUMBER(feeder)) {
                    msapi_runtimeError(vm, "Attempt call '-=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!CHECK_NUMBER(increment)) {
                    msapi_runtimeError(vm, "Non-numeric value to '-='");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                insertTable(&vm->globals, AS_STRING(name), NATIVE_TO_NUMBER(
                            AS_NUMBER(feeder) - AS_NUMBER(increment)
                ));
                break;


            }
            case OP_MUL_ASSIGN_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = pop(vm);
                ASSIGN_GLOBAL(vm, frame, name, feeder);
                
                if (!CHECK_NUMBER(feeder)) {
                    msapi_runtimeError(vm, "Attempt call '*=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!CHECK_NUMBER(increment)) {
                    msapi_runtimeError(vm, "Non-numeric value to '*='");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                insertTable(&vm->globals, AS_STRING(name), NATIVE_TO_NUMBER(
                            AS_NUMBER(feeder) * AS_NUMBER(increment)
                ));
                break;

            }
            case OP_MUL_ASSIGN_LONG_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = pop(vm);
                ASSIGN_LONG_GLOBAL(vm, frame, name, feeder);
                
                if (!CHECK_NUMBER(feeder)) {
                    msapi_runtimeError(vm, "Attempt call '*=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!CHECK_NUMBER(increment)) {
                    msapi_runtimeError(vm, "Non-numeric value to '*='");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                insertTable(&vm->globals, AS_STRING(name), NATIVE_TO_NUMBER(
                            AS_NUMBER(feeder) * AS_NUMBER(increment)
                ));
                break;


            }
            case OP_DIV_ASSIGN_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = pop(vm);
                ASSIGN_GLOBAL(vm, frame, name, feeder);
                
                if (!CHECK_NUMBER(feeder)) {
                    msapi_runtimeError(vm, "Attempt call '/=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!CHECK_NUMBER(increment)) {
                    msapi_runtimeError(vm, "Non-numeric value to '/='");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (AS_NUMBER(increment)) {
                    msapi_runtimeError(vm, "Cannot divide by 0");
                    return INTERPRET_RUNTIME_ERROR;
                }

                insertTable(&vm->globals, AS_STRING(name), NATIVE_TO_NUMBER(
                            AS_NUMBER(feeder) / AS_NUMBER(increment)
                ));
                break;

            }
            case OP_DIV_ASSIGN_LONG_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = pop(vm);
                ASSIGN_LONG_GLOBAL(vm, frame, name, feeder);
                
                if (!CHECK_NUMBER(feeder)) {
                    msapi_runtimeError(vm, "Attempt call '/=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!CHECK_NUMBER(increment)) {
                    msapi_runtimeError(vm, "Non-numeric value to '/='");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (AS_NUMBER(increment)) {
                    msapi_runtimeError(vm, "Cannot divide by 0");
                    return INTERPRET_RUNTIME_ERROR;
                }


                insertTable(&vm->globals, AS_STRING(name), NATIVE_TO_NUMBER(
                            AS_NUMBER(feeder) / AS_NUMBER(increment)
                ));
                break;


            }
            case OP_POW_ASSIGN_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = pop(vm);
                ASSIGN_GLOBAL(vm, frame, name, feeder);
                
                if (!CHECK_NUMBER(feeder)) {
                    msapi_runtimeError(vm, "Attempt call '^=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!CHECK_NUMBER(increment)) {
                    msapi_runtimeError(vm, "Non-numeric value to '^='");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                insertTable(&vm->globals, AS_STRING(name), NATIVE_TO_NUMBER(
                            pow(AS_NUMBER(feeder), AS_NUMBER(increment))
                ));
                break;

            }
            case OP_POW_ASSIGN_LONG_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = pop(vm);
                ASSIGN_LONG_GLOBAL(vm, frame, name, feeder);
                
                if (!CHECK_NUMBER(feeder)) {
                    msapi_runtimeError(vm, "Attempt call '^=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!CHECK_NUMBER(increment)) {
                    msapi_runtimeError(vm, "Non-numeric value to '^='");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                insertTable(&vm->globals, AS_STRING(name), NATIVE_TO_NUMBER(
                            pow(AS_NUMBER(feeder), AS_NUMBER(increment))
                ));
                break;


            }
            case OP_ASSIGN_LOCAL: { 
                uint8_t localIndex = READ_BYTE(frame);
                frame->slotPtr[localIndex] = pop(vm);
                break;
            }
            case OP_PLUS_ASSIGN_LOCAL: {
                uint8_t localIndex = READ_BYTE(frame);
                Value old = frame->slotPtr[localIndex];
                Value new = pop(vm);

                if (CHECK_NUMBER(old) && CHECK_NUMBER(new)) {
                    frame->slotPtr[localIndex] = NATIVE_TO_NUMBER(AS_NUMBER(old) + AS_NUMBER(new));
                } else if (CHECK_STRING(old) && CHECK_STRING(new)) {
                    frame->slotPtr[localIndex] = OBJ(strConcat(vm, old, new));                    
                } else {
                    msapi_runtimeError(vm, "Attempt to call '+=' on a non-numeric/string value");
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
            case OP_MINUS_ASSIGN_LOCAL: {
                uint8_t localIndex = READ_BYTE(frame);
                Value old = frame->slotPtr[localIndex];
                Value new = pop(vm);

                if (!CHECK_NUMBER(old)) {
                    msapi_runtimeError(vm, "Attempt to call '-=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!CHECK_NUMBER(new)) {
                    msapi_runtimeError(vm, "Non-numeric value to '-='");
                    return INTERPRET_RUNTIME_ERROR;
                }

                frame->slotPtr[localIndex] = NATIVE_TO_NUMBER(AS_NUMBER(old) - AS_NUMBER(new));
                break;
            }
            case OP_MUL_ASSIGN_LOCAL: {
                uint8_t localIndex = READ_BYTE(frame);
                Value old = frame->slotPtr[localIndex];
                Value new = pop(vm);

                if (!CHECK_NUMBER(old)) {
                    msapi_runtimeError(vm, "Attempt to call '*=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!CHECK_NUMBER(new)) {
                    msapi_runtimeError(vm, "Non-numeric value to '*='");
                    return INTERPRET_RUNTIME_ERROR;
                }

                frame->slotPtr[localIndex] = NATIVE_TO_NUMBER(AS_NUMBER(old) * AS_NUMBER(new));
                break;
            }
            case OP_DIV_ASSIGN_LOCAL: {
                uint8_t localIndex = READ_BYTE(frame);
                Value old = frame->slotPtr[localIndex];
                Value new = pop(vm);

                if (!CHECK_NUMBER(old)) {
                    msapi_runtimeError(vm, "Attempt to call '/=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!CHECK_NUMBER(new)) {
                    msapi_runtimeError(vm, "Non-numeric value to '/='");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (AS_NUMBER(new)) {
                    msapi_runtimeError(vm, "Cannot divide by 0");
                    return INTERPRET_RUNTIME_ERROR;
                }

                frame->slotPtr[localIndex] = NATIVE_TO_NUMBER(AS_NUMBER(old) / AS_NUMBER(new));
                break;
            }
            case OP_POW_ASSIGN_LOCAL: {
                uint8_t localIndex = READ_BYTE(frame);
                Value old = frame->slotPtr[localIndex];
                Value new = pop(vm);

                if (!CHECK_NUMBER(old)) {
                    msapi_runtimeError(vm, "Attempt to call '^=' on a non-numeric value");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!CHECK_NUMBER(new)) {
                    msapi_runtimeError(vm, "Non-numeric value to '^='");
                    return INTERPRET_RUNTIME_ERROR;
                }

                frame->slotPtr[localIndex] = NATIVE_TO_NUMBER(pow(AS_NUMBER(old), AS_NUMBER(new)));
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t localIndex = READ_BYTE(frame);
                push(vm, frame->slotPtr[localIndex]);
                break;
            }
            case OP_POP: pop(vm); break;
            case OP_POPN: {
                uint8_t count = READ_BYTE(frame);
                popn(vm, count);
                break;
            }
            case OP_CONST: {
                Value constant = READ_CONSTANT(frame);
                push(vm, constant);
                break;
            }
            case OP_CONST_LONG: {
                Value constant = READ_LONG_CONSTANT(frame);
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
                    msapi_runtimeError(vm, "Error : Expected Numeric or String Operand to '+'");
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
                    msapi_runtimeError(vm, "Error : Expected Numeric Operand to '-'");
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
                    msapi_runtimeError(vm, "Error : Expected Numeric Operand to '*'");
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
                        msapi_runtimeError(vm, "Error : Division By 0");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(vm, NATIVE_TO_NUMBER(AS_NUMBER(operand2) / op1));
                } else {
                    
                    msapi_runtimeError(vm, "Error : Expected Numeric Operand to '/'");
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
                    msapi_runtimeError(vm, "Error : Expected Numeric Operand to '^'");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_NEGATE: {
                if (CHECK_NUMBER(peek(vm, 0))) {
                    push(vm, NATIVE_TO_NUMBER(-AS_NUMBER(pop(vm))));
                } else {
                    /* Runtime Error */
                    msapi_runtimeError(vm, "Error : Expected Numeric Operand to unary negation");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_LENGTH: {
                Value val = pop(vm);;

                if (CHECK_ARRAY(val)) {
                    push(vm, NATIVE_TO_NUMBER(AS_ARRAY(val)->array.count));
                } else if (CHECK_STRING(val)) {
                    push(vm, NATIVE_TO_NUMBER(AS_STRING(val)->length));
                } else {
                    msapi_runtimeError(vm, "Error : Expected Array/String for unary '#' operator");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_NOT: {
                push(vm, NATIVE_TO_BOOLEAN(isFalsey(pop(vm))));
                break;
            }
            case OP_EQUAL: {
                push(vm, NATIVE_TO_BOOLEAN(msapi_isEqual(pop(vm), pop(vm))));
                break;
            }
            case OP_NOT_EQ: {
                push(vm, NATIVE_TO_BOOLEAN(!msapi_isEqual(pop(vm), pop(vm))));
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
            case OP_GREATER: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                if (!CHECK_NUMBER(operand2) || !CHECK_NUMBER(operand1)) {
                    msapi_runtimeError(vm, "Error : Expected Numeric Operand To '>'");
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(vm, NATIVE_TO_BOOLEAN(AS_NUMBER(operand2) > AS_NUMBER(operand1)));
                break;
            }
            case OP_GREATER_EQ: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                if (!CHECK_NUMBER(operand2) || !CHECK_NUMBER(operand1)) {
                    msapi_runtimeError(vm, "Error : Expected Numeric Operand To '>='");
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(vm, NATIVE_TO_BOOLEAN(AS_NUMBER(operand2) >= AS_NUMBER(operand1)));
                break;
            }
            case OP_LESSER: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                if (!CHECK_NUMBER(operand2) || !CHECK_NUMBER(operand1)) {
                    msapi_runtimeError(vm, "Error : Expected Numeric Operand To '<'");
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(vm, NATIVE_TO_BOOLEAN(AS_NUMBER(operand2) < AS_NUMBER(operand1)));
                break;
            }
            case OP_LESSER_EQ: {
                Value operand1 = pop(vm);
                Value operand2 = pop(vm);

                if (!CHECK_NUMBER(operand2) || !CHECK_NUMBER(operand1)) {
                    msapi_runtimeError(vm, "Error : Expected Numeric Operand To '<='");
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(vm, NATIVE_TO_BOOLEAN(AS_NUMBER(operand2) <= AS_NUMBER(operand1)));
                break;
            }
            default:
                printf("Unknown Instruction %ld\n", (long)ins);
                printf("Next : %ld\n", (long)*frame->ip);
                printf("Previous : %ld\n",(long)frame->ip[-2]);

error_label:

                return INTERPRET_RUNTIME_ERROR;
        }
    }
}

InterpretResult interpret(VM* vm, ObjFunction* function) {
    push(vm, OBJ(function));
    msapi_pushCallFrame(vm, function);
    InterpretResult result = run(vm);
    popCallFrame(vm);
    return result;
}
