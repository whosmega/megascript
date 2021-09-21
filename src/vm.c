#include "../includes/vm.h"
#include "../includes/chunk.h"
#include "../includes/common.h"
#include "../includes/debug.h"
#include "../includes/object.h"
#include "../includes/value.h"
#include "../includes/table.h"
#include "../includes/globals.h"
#include "../includes/memory.h"
#include "../includes/compiler.h"
#include "../includes/msapi.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#ifdef _WIN32
#include "../includes/win_unistd.h"                 /* File system checking */ 
#include "../includes/win_dlfnc.h"                  /* Loading DLLs */ 
#else
#include <unistd.h>                     /* File system checking */ 
#include <dlfcn.h>                      /* Loading DLLs */ 
#endif

#define READ_BYTE(frameptr) (*frame->ip++)
#define READ_LONG_BYTE(frameptr) (READ_BYTE(frameptr) | READ_BYTE(frameptr) << 8)
#define READ_CONSTANT(frameptr) \
    (frameptr->closure->function->chunk.constants.values[READ_BYTE(frameptr)])
#define READ_LONG_CONSTANT(frameptr) \
    (frameptr->closure->function->chunk.constants.values[READ_LONG_BYTE(frameptr)])

#define ASSIGN_GLOBAL(vmpointer, frameptr, valueName, valueFeeder) \
    valueName = READ_CONSTANT(frameptr); \
    bool found = getTable(&vmpointer->globals->table, AS_STRING(valueName), &valueFeeder); \
    if (!found) { \
      msapi_runtimeError(vmpointer, "Error : Attempt to assign undefined global '%s'", AS_NATIVE_STRING(valueName)); \
      return INTERPRET_RUNTIME_ERROR; \
    } \
    if (vmpointer->currentModule != NULL) { \
        writeObjStringArray(&vmpointer->currentModule->customGlobals, AS_STRING(valueName)); \
    }
 
 
#define ASSIGN_LONG_GLOBAL(vmpointer, frameptr, valueName, valueFeeder) \
    valueName = READ_LONG_CONSTANT(frameptr); \
    bool found = getTable(&vmpointer->globals->table, AS_STRING(valueName), &valueFeeder); \
    if (!found) { \
      msapi_runtimeError(vmpointer, "Error : Attempt to assign undefined global '%s'", AS_NATIVE_STRING(valueName)); \
      return INTERPRET_RUNTIME_ERROR; \
    } \
    if (vmpointer->currentModule != NULL) { \
        writeObjStringArray(&vmpointer->currentModule->customGlobals, AS_STRING(valueName)); \
    }
 

#define push(vmptr, v) _push_(vmptr, v) 

                       // if ((vmptr->stackTop - vmptr->stack) == STACK_MAX) {\
                       //      msapi_runtimeError(vmptr, "Stack Overflow"); \
                       //      return INTERPRET_RUNTIME_ERROR; \
                       // }
                      
#define pushn(vmptr, v, c)  _pushn_(vmptr, v, c)

                           //  if ((vmptr->stackTop - vmptr->stack) >= STACK_MAX) { \
                           //     msapi_runtimeError(vmptr, "Stack Overflow"); \
                           //     return INTERPRET_RUNTIME_ERROR; \
                           //  } 
                          

static void injectArrayMethods(VM* vm) {
    ObjString* string = allocateString(vm, "insert", 6);

    insertPtrTable(&vm->arrayMethods, string, &msmethod_array_insert);
}

static void injectStringMethods(VM* vm) {
    ObjString* captureString = allocateString(vm, "capture", 7);
    ObjString* getAsciiString = allocateString(vm, "getAscii", 8);
    ObjString* splitString = allocateString(vm, "split", 5);

    insertPtrTable(&vm->stringMethods, splitString, &msmethod_string_split);
    insertPtrTable(&vm->stringMethods, getAsciiString, &msmethod_string_getAscii);
    insertPtrTable(&vm->stringMethods, captureString, &msmethod_string_capture);
}

static void injectTableMethods(VM* vm) {
    ObjString* string = allocateString(vm, "keys", 4);

    insertPtrTable(&vm->tableMethods, string, &msmethod_table_keys);
}

static void injectDllMethods(VM* vm) {
    ObjString* closeString = allocateString(vm, "close", 5);
    ObjString* queryString = allocateString(vm, "query", 5); 
    
    insertPtrTable(&vm->dllMethods, queryString, &msmethod_dll_query);
    insertPtrTable(&vm->dllMethods, closeString, &msmethod_dll_close);
}

void initVM(VM* vm) {
    vm->frameCount = 0;
    vm->ObjHead = NULL;
    vm->greyCount = 0;
    vm->greyCapacity = 0;
    vm->greyStack = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = 1024 * 1024;
    initTable(&vm->strings);
    initPtrTable(&vm->arrayMethods);
    initPtrTable(&vm->stringMethods);
    initPtrTable(&vm->tableMethods);
    initPtrTable(&vm->dllMethods);

    resetStack(vm);
    vm->running = false;
    initTable(&vm->importCache);
    vm->moduleCount = 0;
    vm->currentModule = NULL; 

    injectArrayMethods(vm);
    injectStringMethods(vm);
    injectTableMethods(vm);
    injectDllMethods(vm);
    vm->globals = allocateTable(vm);
    // Setup globals 
    injectGlobals(vm);
}

void freeVM(VM* vm) {
    freeTable(&vm->strings);
    freeObject(vm, &vm->globals->obj);
    freeTable(&vm->importCache);
    freePtrTable(&vm->arrayMethods);
    freePtrTable(&vm->stringMethods);
    freePtrTable(&vm->tableMethods);
    freePtrTable(&vm->dllMethods);
    freeTable(&vm->importCache);
    freeObjects(vm);
    FREE_ARRAY(Obj*, vm->greyStack, vm->greyCapacity);
}

void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
    vm->UpvalueHead = NULL;
}

/* Making string array methods */
void initObjStringArray(ObjStringArray* array) {
    array->capacity = 0;
    array->count = 0;
    array->values = NULL;
}


void writeObjStringArray(ObjStringArray* array, ObjString* value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(ObjString*, array->values, oldCapacity, array->capacity);
    }


    array->values[array->count] = value;
    array->count++;
}

void freeObjStringArray(ObjStringArray* array) {
    FREE_ARRAY(ObjString*, array->values, array->capacity);
    initObjStringArray(array);
}



/*                      VM API                      */

void msapi_runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
    
    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    size_t ins = frame->ip - frame->closure->function->chunk.code - 1;
    int line = frame->closure->function->chunk.lines[ins];
    fprintf(stderr, "Line %d: ", line);
    fprintf(stderr, "In Script\n");


    printf("\nCall Stack Traceback:\n");

    printf("In function: %s\n", vm->frames[vm->frameCount - 1].closure->function->name->allocated);
    
    for (int i = vm->frameCount - 2; i >= 0; i--) {
        printf("Called by: %s\n", vm->frames[i].closure->function->name->allocated);
    }
    resetStack(vm);
}


static inline void _push_(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
    // printf("After pushing : %d : %u\n", vm->stackSize, 1);

}

static inline Value pop(VM* vm) { 
    // printf("After popping : %d : %d\n", vm->stackSize, 1);
    return *(--vm->stackTop);
}

static inline void _pushn_(VM* vm, Value value, unsigned int count) {
    for (int i = 0; i < count; i++) {
        _push_(vm, value);
    }
}

static inline void popn(VM* vm, unsigned int count) {
    vm->stackTop -= count;
    // printf("After popping : %d : %u\n", vm->stackSize, count);
}

static inline Value peek(VM* vm, int offset) {
    return vm->stackTop[-1 - offset];
}

static inline Value* peekptr(VM* vm, int offset) {
    return &vm->stackTop[-1 - offset];
}

static inline bool isFalsey(Value value) {
    return CHECK_NIL(value) || (CHECK_BOOLEAN(value) && !AS_BOOL(value));
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

Value msapi_getArg(VM *vm, int number, int argCount) {
    return vm->stackTop[-argCount + number - 1]; 
}

//--------------------------------------------

static bool invokeNativeMethod(VM* vm, ObjString* string, Obj* self, 
        int argCount, bool shouldReturn, PtrTable* ptrTable) {

    NativeMethodPtr ptr = NULL;
    bool found = getPtrTable(ptrTable, string, (void*)&ptr);

    if (!found) {
        msapi_runtimeError(vm, "Attempt to invoke a nil value");
        return false;
    }

    bool result = (*ptr)(vm, self, argCount, shouldReturn);
    if (!result) return false;

    return true;
}

static bool callClosure(VM* vm, ObjClosure* closure, bool shouldReturn, int argCount, bool isCoroutine) {
    ObjFunction* function = closure->function;
            
    if (vm->frameCount == FRAME_MAX) {
        msapi_runtimeError(vm, "Error : Max Call-Depth reached (Stack Overflow)");
        return false;
    }
    
    // Function should already be pushed on stack
    CallFrame frame;
    frame.closure = closure;
    frame.ip = closure->function->chunk.code; 
    frame.shouldReturn = shouldReturn;
    frame.isCoroutine = isCoroutine;

    // the arguments have already been pushed before the the stack 
    // frame was pushed, so we relocate the callframe pointer to the 
    // beginning of the variable list before the function 
    frame.slotPtr = peekptr(vm, argCount);
    vm->frames[vm->frameCount] = frame;
    vm->frameCount++;
    
    uint8_t expectedArgCount = closure->function->arity;

    if (argCount > expectedArgCount) { 
        uint8_t extra = argCount - expectedArgCount;
        if (function->variadic) {
            ObjArray* array = allocateArray(vm); 
                            
            for (int i = extra - 1; i >= 0; i--) {
                writeValueArray(&array->array, peek(vm, i));   
            }
                            
            popn(vm, extra);
            push(vm, OBJ(array));
        } else {
            popn(vm, extra);
        }
        return true;
    } else if (argCount < expectedArgCount) {
        uint8_t padding = expectedArgCount - argCount;
        pushn(vm, NIL(), padding);
    }

    // if it gets till here, we are in an argCount <= expectedArgCount for sure 
    if (function->variadic) {
        push(vm, OBJ(allocateArray(vm)));
    }
    
    /* Set the appropriate global environment, when returning, the caller's env will be set */ 
    vm->globals = closure->env;
    return true;
}

static bool call(VM* vm, Value value, bool shouldReturn, int argCount) {    
    switch (AS_OBJ(value)->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = AS_CLOSURE(value);
            return callClosure(vm, closure, shouldReturn, argCount, false);
        }
        case OBJ_NATIVE_FUNCTION: {
            NativeFuncPtr ptr = AS_NATIVE_FUNCTION(value)->funcPtr;
            bool result = (*ptr)(vm, argCount, shouldReturn);
            if (!result) return false;
            break;
        }
        case OBJ_NATIVE_METHOD: {
            ObjNativeMethod* native = AS_NATIVE_METHOD(value);
            NativeMethodPtr ptr = native->function;
            bool result = (*ptr)(vm, native->self, argCount, shouldReturn);
            if (!result) return false;
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = AS_CLASS(value);
            ObjInstance* instance = allocateInstance(vm, klass); 
            Value _init;
            if (getTable(&klass->methods, allocateString(vm, "_init", 5), &_init)) {
                vm->stackTop[-argCount - 1] = OBJ(instance);        // set self
                return callClosure(vm, AS_CLOSURE(_init), true, argCount, false);
            }
            
            if (shouldReturn) {
                popn(vm, argCount + 1);             // pop the args and the class
                push(vm, OBJ(instance));
            }
            break;
        }
        case OBJ_METHOD: {
            ObjMethod* method = AS_METHOD(value);
            vm->stackTop[-argCount - 1] = OBJ(method->self);
            callClosure(vm, method->closure, shouldReturn, argCount, false);
            break;
        }
        default:
            msapi_runtimeError(vm, "Attempt to call a non-callable value");
            return false;
    }
    return true;
}

static bool callValue(VM* vm, Value value, bool shouldReturn, int argCount) {
    if (!CHECK_OBJ(value)) {
        msapi_runtimeError(vm, "Attempt to call a non-callable value");
        return false;
    }

    return call(vm, value, shouldReturn, argCount);
}

bool msapi_callClosure(VM* vm, ObjClosure* closure, bool shouldReturn, int argCount, bool isCoroutine) {
    return callClosure(vm, closure, shouldReturn, argCount, isCoroutine);
}

char* findFile(VM* vm, char* path, bool genErr) {
    /* This function searches for the file in all supported ways */ 
   
    if (access(path, F_OK) == -1) {
        /* Not in current directory */ 
        char* var = getenv("MEGPATH");
        if (var != NULL) {
            /* User has set env variable */ 
            int length = strlen(var) + strlen(path) + 2;
            char* chars = (char*)reallocate(vm, NULL, 0, sizeof(char) * length);
            strcpy(chars, var);
            strcat(chars, "/");
            strcat(chars, path); 
            chars[length] = '\0';
            
            if (access(chars, F_OK) != -1) {
                /* We found it bois */
                return chars;
            }
        }

        if (genErr) msapi_runtimeError(vm, "Unable to locate file : %s", path);
        return NULL;
    } else {
        int length = strlen(path);
        char* chars = (char*)reallocate(vm, NULL, 0, sizeof(char) * length);
        chars[length] = '\0';
        strcpy(chars, path);
        return chars;
    }
}

static bool importScript(VM* vm, const char* path, ObjString* fileName) {
    /* First we check for this file in the cache, 
     * to make sure its not getting re-imported */
    Value cached = NIL();

    if (getTable(&vm->importCache, fileName, &cached)) {
        insertTable(&vm->globals->table, fileName, cached);
        return true;
    }

    /* If this file is not in the cache, we start by reading the file contents */ 
    FILE* file = fopen(path, "r");
    size_t fileLength = 0;
    
    /* We first move our cursor to the end of the file */
    fseek(file, 0, SEEK_END);
    /* Now we read the offset from the start of the file, 
     * which is the total length in bytes */
    fileLength = ftell(file);
    /* We move the cursor back to the start */
    fseek(file, 0, SEEK_SET);

    /* We allocate a block to read our file into */
    char* fileContent = (char*)reallocate(vm, NULL, 0, fileLength + 1);
    fileContent[fileLength] = '\0';
    fread(fileContent, 1, fileLength, file);
    fclose(file);

    /* We can now allocate a function and begin compiling *
     * We also push it to avoid the garbage collection of it */
    ObjFunction* function = allocateFunction(vm, fileName, 0);
    push(vm, OBJ(function));

    /* We change the state of the vm to 'not running' since we 
     * will begin compiling the file */
    vm->running = false;
    InterpretResult compilationResult = compile(fileContent, vm, function, false);
    vm->running = true;

    /* We have finished compiling, we check the result */
    if (compilationResult == INTERPRET_COMPILE_ERROR) return false;
    if (vm->moduleCount >= IMPORT_CYCLE_MAX) {
        msapi_runtimeError(vm, "Import stack full (possible circular import)");
        return false;
    }
    
    /* We can now proceed to create a new module object for the vm */
    Module module;
    module.moduleName = fileName;
    module.globals = allocateTable(vm);
    initObjStringArray(&module.customGlobals);
    
    /* After initialization, we add it to the vm and update */
    vm->modules[vm->moduleCount] = module;
    vm->currentModule = &vm->modules[vm->moduleCount];
    vm->moduleCount++;
    vm->globals = vm->currentModule->globals;
    
    /* We inject globals into it */ 
    injectGlobals(vm);
    
    /* Now we wrap it into a closure, with the latest global environment, 
     * and pop the function we pushed earlier for garbge collection protection */
    ObjClosure* closure = allocateClosure(vm, function, vm->globals);
    pop(vm);
    
    /* We can finally attempt a call to the module */
    push(vm, OBJ(closure));
    return callClosure(vm, closure, false, 0, false);
}

static bool importSharedLib(VM* vm, const char* path, ObjString* fileName) {
    void* fileHandle = dlopen(path, RTLD_NOW);
    
    if (fileHandle == NULL) {
        msapi_runtimeError(vm, "An error occured while opening share library");
        return false;
    }

    ObjDllContainer* container = allocateDllContainer(vm, fileName, fileHandle);
    insertTable(&vm->globals->table, fileName, OBJ(container));
    return true;
}

static bool import(VM* vm, ObjString* importPath) {
    char* name = &importPath->allocated[0];
    int nameLength = 0;

    for (int i = 0; i < importPath->length; i++) {

        if (importPath->allocated[i] == '/' || importPath->allocated[i] == '\\') {
            name = &importPath->allocated[i + 1];
            nameLength = 0;
        } else {
            nameLength++;
        }
    }
    
    int pathLen = strlen(importPath->allocated);
    char buffer[pathLen + 5];
    ObjString* fileName = allocateString(vm, name, nameLength);
    char* foundFile = NULL;
    char* pathEndPtr = &buffer[pathLen];

    memcpy(buffer, importPath->allocated, importPath->length);
    memcpy(pathEndPtr, ".meg\0", 5);
    foundFile = findFile(vm, buffer, false);
    
    if (foundFile != NULL) {
        bool result = importScript(vm, foundFile, fileName);
        reallocate(vm, foundFile, sizeof(*foundFile), 0);
        return result;
    }
    
    memcpy(pathEndPtr, ".so\0", 4);
    foundFile = findFile(vm, buffer, false);

    if (foundFile != NULL) {
        bool result = importSharedLib(vm, foundFile, fileName);
        reallocate(vm, foundFile, sizeof(*foundFile), 0);
        return result;
    }

    memcpy(pathEndPtr, ".dll\0", 5);
    foundFile = findFile(vm, buffer, false);

    if (foundFile != NULL) {
        bool result = importSharedLib(vm, foundFile, fileName);
        reallocate(vm, foundFile, sizeof(*foundFile), 0);
        return result;
    }

    /* No suitable file found */
    msapi_runtimeError(vm, "Could not locate file");
    return false;
}

static bool checkCustomIndexArray(VM* vm, ObjArray* array, Value index) {
    if (!CHECK_NUMBER(index)) {
        msapi_runtimeError(vm, "Expected a number for array index");
        return false;
    }

    double numIndex = AS_NUMBER(index);

    if (floor(numIndex) != numIndex || numIndex < 0) {
        msapi_runtimeError(vm, "Array Index is expected to be a positive integer, got %g", numIndex);
        return false;
    }

    if (numIndex + 1 > array->array.count) {
        msapi_runtimeError(vm, "Array Index %g out of range", numIndex);

        return false;
    }
    
    return true;
}

static ObjUpvalue* captureUpvalue(VM* vm, Value* value, uint8_t stackIndex) {
    ObjUpvalue* previousUpvalue = NULL;
    ObjUpvalue* upvalue = vm->UpvalueHead;

    /* We try to find an existing upvalue pointing to the same stack slot 
     * we do that by iterating from the head of the upvalue linked list to the very end 
     * We keep moving ahead from the head of the list, if its over or we have passed the 
     * memory address of the slot we were looking for, we assume there isnt a copy*/

    while (upvalue != NULL && upvalue->value > value) {
        previousUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->value == value) {
        // We return the upvalue we found instead of making a new one 
        return upvalue;
    }

    /* In case we dont end up finding an already existing upvalue, when creating
     * a new upvalue, we link it in the right place by setting the next upvalue 
     * pointer to the last upvalue we iterated over*/

    ObjUpvalue* newUpvalue = allocateUpvalue(vm, value);
    newUpvalue->next = upvalue;

    if (previousUpvalue == NULL) {
        vm->UpvalueHead = newUpvalue;
    } else {
        previousUpvalue->next = newUpvalue;    
    }

    return newUpvalue;
}

static ObjUpvalue* closeUpvalues(VM* vm, Value* slot) {
    ObjUpvalue* last = NULL;
    ObjUpvalue* first = vm->UpvalueHead;
    while (vm->UpvalueHead != NULL && vm->UpvalueHead->value >= slot) {
        ObjUpvalue* openUpvalue = vm->UpvalueHead;
        openUpvalue->closed = *openUpvalue->value;
        openUpvalue->value = &openUpvalue->closed;
        vm->UpvalueHead = openUpvalue->next;
        last = openUpvalue;
    }

    if (last != NULL) {
        last->next = NULL;
    }
    
    /* Returns a pointer to the first upvalue that was closed,
     * it preserves the linked list order, and marks the last upvalue's next to 'NULL' 
     * The first upvalue is the last on the stack */

    return first;
}

ObjUpvalue* msapi_closeUpvalues(VM* vm, Value* slot) {
    return closeUpvalues(vm, slot);
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

/* --------------- Native Methods --------------- */ 
bool msmethod_array_insert(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount == 0) {
        msapi_runtimeError(vm, "Expected atleast 1 argument for insert");
        return false;
    }
    ObjArray* array = (ObjArray*)self; 
    
    for (int i = argCount - 1; i >= 0; i--) {
        writeValueArray(&array->array, peek(vm, i));
    }

    popn(vm, argCount + 1);
    
    if (shouldReturn) push(vm, NIL());
    return true;
}

bool msmethod_string_getAscii(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    ObjString* string = (ObjString*)self;
    popn(vm, argCount + 1);
        
    if (string->length < 1) {
        msapi_runtimeError(vm, "Got empty string");
        return false;
    }
    if (!shouldReturn) return true;

    if (string->length == 1) {
        push(vm, NATIVE_TO_NUMBER((double)string->allocated[0]));
    } else {
        ObjArray* array = allocateArray(vm);

        for (int i = 0; i < string->length; i++) {
            writeValueArray(&array->array, 
                    NATIVE_TO_NUMBER((double)string->allocated[i]));
        }
        push(vm, OBJ(array));
    }

    return true;
}

bool msmethod_string_split(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 1) {
        msapi_runtimeError(vm, "Too less arguments, expected 1, got 0");
        return false;
    }

    Value string = msapi_getArg(vm, 1, argCount);
    ObjString* selfString = (ObjString*)self;
    
    if (!CHECK_STRING(string)) {
        msapi_runtimeError(vm, "Expected a string");
        return false;
    }

    char* delimeter = AS_STRING(string)->allocated;
    char* mainString = selfString->allocated;
    char* token = strstr(mainString, delimeter);
    int delLen = strlen(delimeter);
    char* right = NULL;

    ObjArray* array = allocateArray(vm);
    
    while (token != NULL) {
        int length = (int)(token - mainString);
        ObjString* string = allocateString(vm, mainString, length);
        right = token + delLen;
        writeValueArray(&array->array, OBJ(string));

        token = strstr(right, delimeter);
        mainString = right;
    }

    if (right != NULL) {
        ObjString* rightString = allocateString(vm, right, strlen(right));
        writeValueArray(&array->array, OBJ(rightString));
    }

    msapi_popn(vm, argCount + 1);

    if (shouldReturn) {
        msapi_push(vm, OBJ(array));
    }

    return true;
}

bool msmethod_string_capture(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 2) {
        msapi_runtimeError(vm, "Insufficient argument count");
        return false;
    }
    Value _arg1 = msapi_getArg(vm, 1, argCount);
    Value _arg2 = msapi_getArg(vm, 2, argCount);

    if (!CHECK_NUMBER(_arg1) || !CHECK_NUMBER(_arg2)) {
        msapi_runtimeError(vm, "Expected arguments to be numberS");
        return false;
    }
    
    double arg1 = AS_NUMBER(_arg1);
    double arg2 = AS_NUMBER(_arg2);

    if (fmod(arg1, 1) != 0 || arg1 < 0) {
        msapi_runtimeError(vm, "Expected 'start' to be a positive integer");
        return false;
    }

    if (fmod(arg2, 1) != 0 || arg2 < 0) {
        msapi_runtimeError(vm, "Expected 'end' to be a positive integer");
        return false;
    }

    ObjString* string = (ObjString*)self;
    if (arg1 >= string->length || arg1 < 0) {
        msapi_runtimeError(vm, "Argument 'start' out of range");
        return false;
    }

    if (arg2 >= string->length + 1 || arg2 < 0) {
        msapi_runtimeError(vm, "Argument 'end' out of range");
        return false;
    }

    if (arg1 > arg2) {
        msapi_runtimeError(vm, "Argument 'start' cannot be greater than 'end'");
        return false;
    }
    
    popn(vm, argCount + 1);
    
    if (shouldReturn) {
        push(vm, OBJ(allocateString(vm, &string->allocated[(int)arg1], arg2 - arg1)));
    }
    return true;
}

bool msmethod_table_keys(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    popn(vm, argCount + 1);
    ObjArray* array = allocateArray(vm);
    ObjTable* table = (ObjTable*)self;

    for (int i = 0; i < table->table.capacity; i++) {
        Entry* entry = &table->table.entries[i];

        if (entry->key != NULL) {
            writeValueArray(&array->array, OBJ(entry->key));
        }
    }

    if (shouldReturn) {
        push(vm, OBJ(array));
    }
    return true;
}

bool msmethod_dll_close(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    popn(vm, argCount + 1);
    ObjDllContainer* container = (ObjDllContainer*)self;
    
    if (container->closed) {
        printf("Warning : DLL object has already been closed\n");
        return true;
    }

    dlclose(container->handle);
    container->handle = NULL;
    container->closed = true;
    
    if (shouldReturn) push(vm, NIL());
    return true;
}

bool msmethod_dll_query(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 1) {
        msapi_runtimeError(vm, "Expected query string");
        return false;
    }

    Value query = msapi_getArg(vm, 1, argCount);

    if (!CHECK_STRING(query)) {
        msapi_runtimeError(vm, "Expected argument to be a string");
        return false;
    }
    
    ObjDllContainer* container = (ObjDllContainer*)self;

    if (container->closed) {
        msapi_runtimeError(vm, "Attempt to query a closed dll");
        return false;
    }

    popn(vm, argCount + 1);
    NativeMethodPtr ptr = dlsym(container->handle, AS_NATIVE_STRING(query));
    
    push(vm, ptr == NULL ? NIL() : OBJ(allocateNativeMethod(vm, AS_STRING(query), &container->obj, ptr)));
    return true;
}
/* ---------------------------------------------- */

static InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
            printf("          ");
            for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
                printf("[ ");
                printValue(*slot);
                printf(" ]");
            }
            printf("\n");
            dissembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
            
        #endif
        uint8_t ins = READ_BYTE(frame);        /* Points to instruction about to be executed and stores the current */

        switch (ins) {
            case OP_ZERO: push(vm, NATIVE_TO_NUMBER(0)); break; 
            case OP_MIN1: push(vm, NATIVE_TO_NUMBER(-1)); break;
            case OP_PLUS1: push(vm, NATIVE_TO_NUMBER(1)); break;
            case OP_UNPACK: {
                /* Unpacks an array into freely suspended values */
                uint8_t expectedCount = AS_NUMBER(READ_CONSTANT(frame));
                Value arrayValue = pop(vm);

                if (!CHECK_ARRAY(arrayValue)) {
                    msapi_runtimeError(vm, "Expected an array to unpack");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjArray* array = AS_ARRAY(arrayValue);
                int arrayCount = array->array.count;

                for (int i = 0; i < arrayCount && i < expectedCount; i++) {
                    Value v = array->array.values[i];
                    push(vm, v);
                }
            }
            case OP_BIT_AND: {
                Value rightOp = pop(vm);
                Value leftOp = pop(vm);

                if (!CHECK_NUMBER(rightOp) || !CHECK_NUMBER(leftOp)) {
                    msapi_runtimeError(vm, "Expected integer operands to `&`");
                    return INTERPRET_RUNTIME_ERROR;
                }

                double rightOpNum = AS_NUMBER(rightOp);
                double leftOpNum = AS_NUMBER(leftOp);
                
                if (floor(rightOpNum) != rightOpNum || floor(leftOpNum) != leftOpNum) {
                    msapi_runtimeError(vm, "Expected operands to be integers");
                    return INTERPRET_RUNTIME_ERROR;
                }

                int result = (int)leftOpNum & (int)rightOpNum;
                
                push(vm, NATIVE_TO_NUMBER(result));
                break;
            }
            case OP_BIT_OR: {
                Value rightOp = pop(vm);
                Value leftOp = pop(vm);

                if (!CHECK_NUMBER(rightOp) || !CHECK_NUMBER(leftOp)) {
                    msapi_runtimeError(vm, "Expected integer operands to `|`");
                    return INTERPRET_RUNTIME_ERROR;
                }

                double rightOpNum = AS_NUMBER(rightOp);
                double leftOpNum = AS_NUMBER(leftOp);
                
                if (floor(rightOpNum) != rightOpNum || floor(leftOpNum) != leftOpNum) {
                    msapi_runtimeError(vm, "Expected operands to be integers");
                }

                int result = (int)leftOpNum | (int)rightOpNum;
                
                push(vm, NATIVE_TO_NUMBER(result));
                break;
 
            }
            case OP_SHIFTL: {
                Value rightOp = pop(vm);
                Value leftOp = pop(vm);

                if (!CHECK_NUMBER(rightOp) || !CHECK_NUMBER(leftOp)) {
                    msapi_runtimeError(vm, "Expected integer operands to `<<`");
                    return INTERPRET_RUNTIME_ERROR;
                }

                double rightOpNum = AS_NUMBER(rightOp);
                double leftOpNum = AS_NUMBER(leftOp);
                
                if (floor(rightOpNum) != rightOpNum || floor(leftOpNum) != leftOpNum) {
                    msapi_runtimeError(vm, "Expected operands to be integers");
                }

                int result = (int)leftOpNum << (int)rightOpNum;
                
                push(vm, NATIVE_TO_NUMBER(result));
                break;
 
            }
            case OP_SHIFTR: {
                Value rightOp = pop(vm);
                Value leftOp = pop(vm);

                if (!CHECK_NUMBER(rightOp) || !CHECK_NUMBER(leftOp)) {
                    msapi_runtimeError(vm, "Expected integer operands to `>>`");
                    return INTERPRET_RUNTIME_ERROR;
                }

                double rightOpNum = AS_NUMBER(rightOp);
                double leftOpNum = AS_NUMBER(leftOp);
                
                if (floor(rightOpNum) != rightOpNum || floor(leftOpNum) != leftOpNum) {
                    msapi_runtimeError(vm, "Expected operands to be integers");
                }

                int result = (int)leftOpNum >> (int)rightOpNum;
                
                push(vm, NATIVE_TO_NUMBER(result));
                break;
 
            }
            case OP_IMPORT: {
                ObjString* string = AS_STRING(READ_CONSTANT(frame));
                if (!import(vm, string)) return INTERPRET_RUNTIME_ERROR;
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_IMPORT_LONG: {
                ObjString* string = AS_STRING(READ_LONG_CONSTANT(frame));
                if (!import(vm, string)) return INTERPRET_RUNTIME_ERROR;
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_RETFILE: {
                ObjString* moduleName = vm->currentModule->moduleName;

                /* Move all upvalues to the heap */
                closeUpvalues(vm, frame->slotPtr);
                
                vm->stackTop = frame->slotPtr;
                vm->frameCount--;
                frame = &vm->frames[vm->frameCount - 1];
                                
                /* Make a copy for user use */
                ObjTable* userTable = allocateTable(vm);

                for (int i = 0; i < vm->currentModule->customGlobals.count; i++) {
                    ObjString* stringKey = vm->currentModule->customGlobals.values[i];
                    Value holder = NIL();
                    getTable(&vm->currentModule->globals->table, stringKey, &holder);
                    insertTable(&userTable->table, stringKey, holder);
                }

                /* Restore old state */ 
                freeObjStringArray(&vm->currentModule->customGlobals);
                vm->modules[vm->moduleCount - 1].globals = NULL;
                vm->moduleCount--;

                if (vm->moduleCount == 0)
                    /* If there is no module before this one, we mark this as null */
                    vm->currentModule = NULL;
                else
                    /* Otherwise we mark it as the previous module */
                    vm->currentModule--;

                /* Restore globals */
                vm->globals = frame->closure->env;

                insertTable(&vm->importCache, moduleName, OBJ(userTable));
                insertTable(&vm->globals->table, moduleName, OBJ(userTable));
                break;
            }
            case OP_CLASS: {
                Value string = READ_CONSTANT(frame);
                ObjClass* klass = allocateClass(vm, AS_STRING(string));
                push(vm, OBJ(klass));
                break;
            }
            case OP_CLASS_LONG: {
                Value string = READ_LONG_CONSTANT(frame);
                ObjClass* klass = allocateClass(vm, AS_STRING(string));
                push(vm, OBJ(klass));
                break;
            }
            case OP_SET_CLASS_FIELD: {
                ObjString* fieldName = AS_STRING(READ_CONSTANT(frame));
                uint8_t inherits = READ_BYTE(frame);

                Value val = peek(vm,  0);  
                /* In case we're inheriting, the super variable comes right after the 
                 * local variable, and so we might need to look for the class 1 or 0 place higher */
                ObjClass* klass = AS_CLASS(peek(vm, inherits + 1));
                insertTable(&klass->fields, fieldName, val);
                pop(vm);
                break;
            }
            case OP_SET_CLASS_FIELD_LONG: {
                ObjString* fieldName = AS_STRING(READ_LONG_CONSTANT(frame));
                uint8_t inherits = READ_BYTE(frame);

                Value val = peek(vm, 0);
                ObjClass* klass = AS_CLASS(peek(vm, inherits + 1));
                
                insertTable(&klass->fields, fieldName, val);
                pop(vm);
                break;

            }
            case OP_SET_FIELD: {
                Value val = peek(vm, 0);
                ObjString* fieldName = AS_STRING(peek(vm, 1));
                Value setVal = peek(vm, 2);
                
                if (!CHECK_OBJ(setVal)) {
                    msapi_runtimeError(vm, "Attempt to set a non-indexable value");
                    return INTERPRET_RUNTIME_ERROR;
                }

                switch (AS_OBJ(setVal)->type) {
                    case OBJ_INSTANCE: {
                        ObjInstance* instance = AS_INSTANCE(setVal);
                        insertTable(&instance->table, fieldName, val);
                        popn(vm, 3);
                        break;
                    }
                    case OBJ_TABLE: {
                        ObjTable* table = AS_TABLE(setVal);
                        insertTable(&table->table, fieldName, val);
                        popn(vm, 3);
                        break;
                    }
                    default: 
                        msapi_runtimeError(vm, "Attempt to set a non-indexable object");
                        return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_FIELD: {
                ObjString* fieldName = AS_STRING(peek(vm, 0));
                Value getVal = peek(vm, 1);
                
                if (!CHECK_OBJ(getVal)) {
                    msapi_runtimeError(vm, "Attempt to index a non-indexable value");
                    return INTERPRET_RUNTIME_ERROR;
                }

                switch (AS_OBJ(getVal)->type) {
                    case OBJ_INSTANCE: {
                        ObjInstance* instance = AS_INSTANCE(getVal);
                        Value value;
                        bool found = getTable(&instance->table, fieldName, &value);

                        if (!found) {
                            bool found2 = getTable(&instance->klass->methods, fieldName, &value);

                            if (found2) {
                                /* A get index to a method only means they're trying to use it 
                                * outside the class instance, which means we need to wrap it */ 
                                ObjMethod* method = allocateMethod(vm, instance, AS_CLOSURE(value));
                                popn(vm, 2); 
                                push(vm, OBJ(method));
                                break;
                            }
                        }

                        popn(vm, 2); 
                        push(vm, found ? value : NIL());
                        break;
                    }
                    case OBJ_ARRAY: {
                        NativeMethodPtr ptr = NULL;
                        bool found = getPtrTable(&vm->arrayMethods, fieldName, (void*)&ptr);
                        
                        popn(vm, 2);

                        if (found) {
                            push(vm, OBJ(
                                    allocateNativeMethod(vm, fieldName, 
                                        (Obj*)AS_OBJ(getVal), ptr)));
                        } else {
                            push(vm, NIL());
                        }
                        break;
                    }
                    case OBJ_STRING: {
                        NativeMethodPtr ptr = NULL;
                        bool found = getPtrTable(&vm->stringMethods, fieldName, (void*)&ptr);

                        popn(vm, 2);

                        if (found) {
                            push(vm, OBJ(
                                        allocateNativeMethod(vm, fieldName,
                                            (Obj*)AS_OBJ(getVal), ptr)));
                        } else {
                            push(vm, NIL());
                        }
                        break;
                    }
                    case OBJ_TABLE: {
                        Value value;
                        bool foundValue = getTable(&AS_TABLE(getVal)->table, fieldName, &value);

                        if (foundValue) {
                            popn(vm, 2);
                            push(vm, value);
                            break;
                        }

                        NativeMethodPtr ptr = NULL;
                        bool found = getPtrTable(&vm->tableMethods, fieldName, (void*)&ptr); 

                        if (found) {
                            ObjNativeMethod* method = allocateNativeMethod(vm, fieldName,
                                            (Obj*)AS_OBJ(getVal), ptr);
                            popn(vm, 2);
                            push(vm, OBJ(method));
                            break;
                        }

                        bool found_nokey = getTable(&AS_TABLE(getVal)->table, 
                                allocateString(vm, "_nokey", 6), &value);
                        
                        popn(vm, 2);
                        if (found_nokey) {
                            push(vm, value);
                            push(vm, getVal);
                            push(vm, OBJ(fieldName));
                            if (!callValue(vm, value, true, 2)) return INTERPRET_RUNTIME_ERROR;
                            frame = &vm->frames[vm->frameCount - 1];
                            break;
                        }
                        push(vm, NIL());
                        
                        break;
                    
                    }
                    default: 
                        msapi_runtimeError(vm, "Attempt to index a non-indexable object");
                        return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_METHOD: { 
                uint8_t inherits = READ_BYTE(frame);
                ObjClosure* closure = AS_CLOSURE(peek(vm, 0));
                ObjClass* klass = AS_CLASS(peek(vm, inherits + 1));
                
                insertTable(&klass->methods, closure->function->name, OBJ(closure));
                pop(vm);
                break;
            }
            case OP_CLOSE_UPVALUE: {
                closeUpvalues(vm, vm->stackTop - 1);
                pop(vm);
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT(frame));
                ObjClosure* closure = allocateClosure(vm, function, vm->globals);

                for (int i = 0; i < closure->upvalueCount; i++) {
                    bool isLocal = READ_BYTE(frame);
                    uint8_t index = READ_BYTE(frame);

                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(vm, frame->slotPtr + index, index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                push(vm, OBJ(closure));
                break;
            }
            case OP_CLOSURE_LONG: {
                ObjFunction* function = AS_FUNCTION(READ_LONG_CONSTANT(frame));
                ObjClosure* closure = allocateClosure(vm, function, vm->globals);

                for (int i = 0; i < closure->upvalueCount; i++) {
                    bool isLocal = READ_BYTE(frame);
                    uint8_t index = READ_BYTE(frame);

                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(vm, frame->slotPtr + index, index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                push(vm, OBJ(closure));
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t index = READ_BYTE(frame);
                push(vm, *frame->closure->upvalues[index]->value);
                break;
            }
            case OP_ASSIGN_UPVALUE: {
                uint8_t index = READ_BYTE(frame);
                *frame->closure->upvalues[index]->value = pop(vm);
                break;
            }
            case OP_PLUS_ASSIGN_UPVALUE: {
                uint8_t index = READ_BYTE(frame);
                Value increment = pop(vm);
                Value oldValue = *frame->closure->upvalues[index]->value;

                if (CHECK_NUMBER(oldValue) && CHECK_NUMBER(increment)) {
                    /* Number increment */ 
                    *frame->closure->upvalues[index]->value = NATIVE_TO_NUMBER(
                        AS_NUMBER(oldValue) + AS_NUMBER(increment)
                    );
                } else if (CHECK_STRING(oldValue) && CHECK_STRING(increment)) {
                    /* String increment */ 
                    *frame->closure->upvalues[index]->value = OBJ(
                        strConcat(vm, oldValue, increment)
                    );
                } else {
                    msapi_runtimeError(vm, "Error : Can only use '+=' on string on number pairs");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_MINUS_ASSIGN_UPVALUE: {
                uint8_t index = READ_BYTE(frame);
                Value increment = pop(vm);
                Value oldValue = *frame->closure->upvalues[index]->value;

                if (!(CHECK_NUMBER(oldValue) && CHECK_NUMBER(increment))) {
                    msapi_runtimeError(vm, "Error : Can only use '-=' on number pairs");
                    return INTERPRET_RUNTIME_ERROR;
                }

                *frame->closure->upvalues[index]->value = NATIVE_TO_NUMBER(
                    AS_NUMBER(oldValue) - AS_NUMBER(increment)
                );
                break;
            }
            case OP_MUL_ASSIGN_UPVALUE: {
                uint8_t index = READ_BYTE(frame);
                Value increment = pop(vm);
                Value oldValue = *frame->closure->upvalues[index]->value;

                if (!(CHECK_NUMBER(oldValue) && CHECK_NUMBER(increment))) {
                    msapi_runtimeError(vm, "Error : Can only use '*=' on number pairs");
                    return INTERPRET_RUNTIME_ERROR;
                }

                *frame->closure->upvalues[index]->value = NATIVE_TO_NUMBER(
                    AS_NUMBER(oldValue) * AS_NUMBER(increment)
                );
 
                break;
            }
            case OP_DIV_ASSIGN_UPVALUE: {
                uint8_t index = READ_BYTE(frame);
                Value increment = pop(vm);
                Value oldValue = *frame->closure->upvalues[index]->value;

                if (!(CHECK_NUMBER(oldValue) && CHECK_NUMBER(increment))) {
                    msapi_runtimeError(vm, "Error : Can only use '/=' on number pairs");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (AS_NUMBER(increment) == 0) {
                    msapi_runtimeError(vm, "Cannot divide by 0");
                    return INTERPRET_RUNTIME_ERROR;
                }

                *frame->closure->upvalues[index]->value = NATIVE_TO_NUMBER(
                    AS_NUMBER(oldValue) / AS_NUMBER(increment)
                );
 
                break;
            }
            case OP_POW_ASSIGN_UPVALUE: {
                uint8_t index = READ_BYTE(frame);
                Value increment = pop(vm);
                Value oldValue = *frame->closure->upvalues[index]->value;

                if (!(CHECK_NUMBER(oldValue) && CHECK_NUMBER(increment))) {
                    msapi_runtimeError(vm, "Error : Can only use '^=' on number pairs");
                    return INTERPRET_RUNTIME_ERROR;
                }

                *frame->closure->upvalues[index]->value = NATIVE_TO_NUMBER(
                    pow(AS_NUMBER(oldValue), AS_NUMBER(increment))
                );
 
                break;
            }
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
            case OP_TABLE: {
                push(vm, OBJ(allocateTable(vm)));
                break;
            }
            case OP_TABLE_INS: {
                ObjString* key = AS_STRING(READ_CONSTANT(frame));
                Value value = pop(vm);
                ObjTable* table = AS_TABLE(peek(vm, 0));

                insertTable(&table->table, key, value);
                break;
            }
            case OP_TABLE_INS_LONG: {
                ObjString* key = AS_STRING(READ_LONG_CONSTANT(frame));
                Value value = pop(vm);
                ObjTable* table = AS_TABLE(peek(vm, 0));

                insertTable(&table->table, key, value);
                break;
            }
            case OP_ARRAY_INS: {
                Value value = pop(vm);
                Value array = peek(vm, 0);
                writeValueArray(&AS_ARRAY(array)->array, value);
                break;
            }
            case OP_CUSTOM_INDEX_MOD: {
                Value value = pop(vm); 
                Value index = pop(vm); 
                Value valArray = pop(vm);
    
                if (!CHECK_OBJ(valArray)) {
                    msapi_runtimeError(vm, "Attempt to index a non-indexable value");
                    return INTERPRET_RUNTIME_ERROR;
                }

                switch (AS_OBJ(valArray)->type) {
                    case OBJ_ARRAY: {
                        ObjArray* array = AS_ARRAY(valArray);

                        if (!checkCustomIndexArray(vm, array, index)) return INTERPRET_RUNTIME_ERROR;
                        array->array.values[(int)AS_NUMBER(index)] = value;
                        break;
                    }
                    case OBJ_TABLE: {
                        ObjTable* table = AS_TABLE(valArray);
                        
                        if (!CHECK_STRING(index)) {
                            msapi_runtimeError(vm, "Expected table index to be a string");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        insertTable(&table->table, AS_STRING(index), value);
                        break;
                    }
                    default:
                        msapi_runtimeError(vm, "Attempt to index a non-indexable object");
                        return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_CUSTOM_INDEX_PLUS_MOD: {
                Value value = peek(vm, 0); 
                Value index = peek(vm, 1); 
                Value valArray = peek(vm, 2);

                if (!CHECK_OBJ(valArray)) {
                    msapi_runtimeError(vm, "Attempt to index a non-indexable value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                switch (AS_OBJ(valArray)->type) {
                    case OBJ_ARRAY: {
                        ObjArray* array = AS_ARRAY(valArray);
    
                        if (!checkCustomIndexArray(vm, array, index)) return INTERPRET_RUNTIME_ERROR;

                        Value* oldValuePtr = &array->array.values[(unsigned int)AS_NUMBER(index)];
                        Value oldValue = *oldValuePtr;

                        if (CHECK_NUMBER(oldValue) && CHECK_NUMBER(value)) {
                            *oldValuePtr = NATIVE_TO_NUMBER(AS_NUMBER(oldValue) + AS_NUMBER(value));   
                        } else if (CHECK_STRING(oldValue) && CHECK_STRING(value)) {
                            *oldValuePtr = OBJ(strConcat(vm, oldValue, value));
                        } else {
                            msapi_runtimeError(vm, "Attempt to use '+=' on a non number/string value");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        break;
                    }
                    case OBJ_TABLE: {
                        ObjTable* table = AS_TABLE(valArray);

                        if (!CHECK_STRING(index)) {
                            msapi_runtimeError(vm, "Expected table index to be a string");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        Value oldValue = NIL();
                        getTable(&table->table, AS_STRING(index), &oldValue);

                        if (CHECK_NUMBER(oldValue) && CHECK_NUMBER(value)) { 
                            insertTable(&table->table, 
                                        AS_STRING(index), 
                                            NATIVE_TO_NUMBER(
                                                AS_NUMBER(oldValue) - AS_NUMBER(value)));
                        } else if (CHECK_STRING(oldValue) && CHECK_STRING(value)) {
                            insertTable(&table->table, 
                                        AS_STRING(index),
                                            OBJ(strConcat(vm, oldValue, value)));
                        } else {
                            msapi_runtimeError(vm, "Attempt to call '+=' on a non-numeric/string value");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        break;
                    }
                    default:
                        msapi_runtimeError(vm, "Attempt to index a non-indexable object");
                        return INTERPRET_RUNTIME_ERROR;
                } 
                
                popn(vm, 3);
                break;
            }
            case OP_CUSTOM_INDEX_SUB_MOD: {
                Value value = peek(vm, 0); 
                Value index = peek(vm, 1); 
                Value valArray = peek(vm, 2);

                if (!CHECK_OBJ(valArray)) {
                    msapi_runtimeError(vm, "Attempt to index a non-indexable value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                switch (AS_OBJ(valArray)->type) {
                    case OBJ_ARRAY: {
                        ObjArray* array = AS_ARRAY(valArray);
                    
                        if (!checkCustomIndexArray(vm, array, index)) return INTERPRET_RUNTIME_ERROR;

                        Value* oldValuePtr = &array->array.values[(int)AS_NUMBER(index)];
                        Value oldValue = *oldValuePtr;

                        if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                            msapi_runtimeError(vm, "Attempt to call '-=' on a non numeric value");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        *oldValuePtr = NATIVE_TO_NUMBER(AS_NUMBER(oldValue) - AS_NUMBER(value));   
                        break;
                    }
                    case OBJ_TABLE: {
                        ObjTable* table = AS_TABLE(valArray);

                        if (!CHECK_STRING(index)) {
                            msapi_runtimeError(vm, "Expected table index to be a string");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        Value oldValue = NIL();
                        getTable(&table->table, AS_STRING(index), &oldValue);
                        
                        if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                            msapi_runtimeError(vm, "Attempt to call '-=' on a non numeric value");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        insertTable(&table->table, 
                                        AS_STRING(index), 
                                            NATIVE_TO_NUMBER(
                                                AS_NUMBER(oldValue) - AS_NUMBER(value)));
                        break;
                    }
                    default:
                        msapi_runtimeError(vm, "Attempt to index a non-indexable object");
                        return INTERPRET_RUNTIME_ERROR;
                } 
                
                popn(vm, 3);
                break;
 
            }
            case OP_CUSTOM_INDEX_MUL_MOD: {
                Value value = peek(vm, 0); 
                Value index = peek(vm, 1); 
                Value valArray = peek(vm, 2);

                if (!CHECK_OBJ(valArray)) {
                    msapi_runtimeError(vm, "Attempt to index a non-indexable value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                switch (AS_OBJ(valArray)->type) {
                    case OBJ_ARRAY: {
                        ObjArray* array = AS_ARRAY(valArray);
                    
                        if (!checkCustomIndexArray(vm, array, index)) return INTERPRET_RUNTIME_ERROR;

                        Value* oldValuePtr = &array->array.values[(int)AS_NUMBER(index)];
                        Value oldValue = *oldValuePtr;

                        if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                            msapi_runtimeError(vm, "Attempt to call '*=' on a non numeric value");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        *oldValuePtr = NATIVE_TO_NUMBER(AS_NUMBER(oldValue) * AS_NUMBER(value));   
                        break;
                    }
                    case OBJ_TABLE: {
                        ObjTable* table = AS_TABLE(valArray);

                        if (!CHECK_STRING(index)) {
                            msapi_runtimeError(vm, "Expected table index to be a string");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        Value oldValue = NIL();
                        getTable(&table->table, AS_STRING(index), &oldValue);
                        
                        if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                            msapi_runtimeError(vm, "Attempt to call '*=' on a non numeric value");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        insertTable(&table->table, 
                                        AS_STRING(index), 
                                            NATIVE_TO_NUMBER(
                                                AS_NUMBER(oldValue) * AS_NUMBER(value)));
                        break;
                    }
                    default:
                        msapi_runtimeError(vm, "Attempt to index a non-indexable object");
                        return INTERPRET_RUNTIME_ERROR;
                } 
             
            }
            case OP_CUSTOM_INDEX_DIV_MOD: {
                Value value = peek(vm, 0); 
                Value index = peek(vm, 1); 
                Value valArray = peek(vm, 2);

                if (!CHECK_OBJ(valArray)) {
                    msapi_runtimeError(vm, "Attempt to index a non-indexable value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                switch (AS_OBJ(valArray)->type) {
                    case OBJ_ARRAY: {
                        ObjArray* array = AS_ARRAY(valArray);
                    
                        if (!checkCustomIndexArray(vm, array, index)) return INTERPRET_RUNTIME_ERROR;

                        Value* oldValuePtr = &array->array.values[(int)AS_NUMBER(index)];
                        Value oldValue = *oldValuePtr;

                        if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                            msapi_runtimeError(vm, "Attempt to call '/=' on a non numeric value");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        *oldValuePtr = NATIVE_TO_NUMBER(AS_NUMBER(oldValue) / AS_NUMBER(value));   
                        break;
                    }
                    case OBJ_TABLE: {
                        ObjTable* table = AS_TABLE(valArray);

                        if (!CHECK_STRING(index)) {
                            msapi_runtimeError(vm, "Expected table index to be a string");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        Value oldValue = NIL();
                        getTable(&table->table, AS_STRING(index), &oldValue);
                        
                        if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                            msapi_runtimeError(vm, "Attempt to call '/=' on a non numeric value");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        insertTable(&table->table, 
                                        AS_STRING(index), 
                                            NATIVE_TO_NUMBER(
                                                AS_NUMBER(oldValue) / AS_NUMBER(value)));
                        break;
                    }
                    default:
                        msapi_runtimeError(vm, "Attempt to index a non-indexable object");
                        return INTERPRET_RUNTIME_ERROR;
                } 
             
            }
            case OP_CUSTOM_INDEX_POW_MOD: {
                Value value = peek(vm, 0); 
                Value index = peek(vm, 1); 
                Value valArray = peek(vm, 2);

                if (!CHECK_OBJ(valArray)) {
                    msapi_runtimeError(vm, "Attempt to index a non-indexable value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                switch (AS_OBJ(valArray)->type) {
                    case OBJ_ARRAY: {
                        ObjArray* array = AS_ARRAY(valArray);
                    
                        if (!checkCustomIndexArray(vm, array, index)) return INTERPRET_RUNTIME_ERROR;

                        Value* oldValuePtr = &array->array.values[(int)AS_NUMBER(index)];
                        Value oldValue = *oldValuePtr;

                        if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                            msapi_runtimeError(vm, "Attempt to call '^=' on a non numeric value");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        *oldValuePtr = NATIVE_TO_NUMBER(pow(AS_NUMBER(oldValue), AS_NUMBER(value)));   
                        break;
                    }
                    case OBJ_TABLE: {
                        ObjTable* table = AS_TABLE(valArray);

                        if (!CHECK_STRING(index)) {
                            msapi_runtimeError(vm, "Expected table index to be a string");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        Value oldValue = NIL();
                        getTable(&table->table, AS_STRING(index), &oldValue);
                        
                        if (!CHECK_NUMBER(oldValue) || !CHECK_NUMBER(value)) {
                            msapi_runtimeError(vm, "Attempt to call '^=' on a non numeric value");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        insertTable(&table->table, 
                                        AS_STRING(index), 
                                            NATIVE_TO_NUMBER(
                                                pow(AS_NUMBER(oldValue), AS_NUMBER(value))));
                        break;
                    }
                    default:
                        msapi_runtimeError(vm, "Attempt to index a non-indexable object");
                        return INTERPRET_RUNTIME_ERROR;
                } 
             
            }
            case OP_CUSTOM_INDEX_GET: {
                Value index = pop(vm);
                Value valArray = pop(vm);  

                if (!CHECK_OBJ(valArray)) {
                    msapi_runtimeError(vm, "Attempt to index a non-indexable value");
                    return INTERPRET_RUNTIME_ERROR;
                }

                switch (AS_OBJ(valArray)->type) {
                    case OBJ_ARRAY: {
                        ObjArray* array = AS_ARRAY(valArray);

                        if (!checkCustomIndexArray(vm, array, index)) return INTERPRET_RUNTIME_ERROR;

                        push(vm, array->array.values[(int)AS_NUMBER(index)]);
                        break;
                    }
                    case OBJ_STRING: {
                        ObjString* string = AS_STRING(valArray);

                        if (!CHECK_NUMBER(index)) {
                            msapi_runtimeError(vm, "String indexing can only be done with integer indexes");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        double numIndex = AS_NUMBER(index);

                        if (fmod(numIndex, 1) != 0 || numIndex < 0) {
                            msapi_runtimeError(vm, "String Index is expected to be a positive integer, got %g", numIndex);
                            return false;
                        }

                        if (numIndex + 1 > string->length) {
                            msapi_runtimeError(vm, "String Index %g out of range", numIndex);

                            return false;
                        }


                        push(vm, OBJ(allocateString(vm, &string->allocated[(int)numIndex], 1)));
                        break;
                    }
                    case OBJ_TABLE: {
                        ObjTable* table = AS_TABLE(valArray);

                        if (!CHECK_STRING(index)) {
                            msapi_runtimeError(vm, "Attempt to index table with non-string index");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        Value tableVal = NIL();
                        bool found = getTable(&table->table, AS_STRING(index), &tableVal);
                        
                        if (found) {
                            push(vm, tableVal);
                            break;
                        }
                        
                        bool found_nokey = getTable(&table->table, 
                                allocateString(vm, "_nokey", 6), &tableVal);
                        if (found_nokey) {
                            push(vm, tableVal);
                            push(vm, valArray);           /* 'self' */ 
                            push(vm, index);              /* key */
                            if (!callValue(vm, tableVal, true, 2)) return INTERPRET_RUNTIME_ERROR;
                            frame = &vm->frames[vm->frameCount - 1];
                        } else {
                            push(vm, NIL());
                        }
                        break;
                    }
                    default:
                        msapi_runtimeError(vm, "Attempt to index a non-indexable object");
                        return INTERPRET_RUNTIME_ERROR;
                }
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
                /* Reads 16 bit */ 
                frame->ip += READ_LONG_BYTE(frame);
                break;
            }
            case OP_JMP_OR: {
                uint16_t byte = READ_LONG_BYTE(frame);
                if (!isFalsey(peek(vm, 0))) {
                    frame->ip += byte; 
                } else {
                    pop(vm);
                }
                break;
            }
           case OP_JMP_FALSE: {
                /* Reads 16 bit */
                uint16_t byte = READ_LONG_BYTE(frame);
                if (isFalsey(pop(vm))) {
                    frame->ip += byte;
                }

                break;
            }
            case OP_JMP_AND: {
                /* Unlike JMP_FALSE, this doesnt pop the value if its falsey */  
                uint16_t byte = READ_LONG_BYTE(frame); 
                if (isFalsey(peek(vm, 0))) {
                    frame->ip += byte; 
                } else {
                    pop(vm);
                }
                break;
            }
            case OP_JMP_BACK: {
                /* Reads 16 bit */ 
                frame->ip -= READ_LONG_BYTE(frame);
                break;
            }
            case OP_CALL: {
                uint8_t argCount = READ_BYTE(frame); 
                bool shouldReturn = (bool)READ_BYTE(frame);
                Value value = peek(vm, argCount);    // first the function is pushed, the the args, then the call instruction
                if (!callValue(vm, value, shouldReturn, argCount)) return INTERPRET_RUNTIME_ERROR;
                // we update the cache variable 
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_INVOKE: {
                uint8_t argCount = READ_BYTE(frame);
                bool shouldReturn = (bool)READ_BYTE(frame);
                ObjString* string = AS_STRING(pop(vm));
                Value callVal = peek(vm, argCount);
                
                if (!CHECK_OBJ(callVal)) {
                    msapi_runtimeError(vm, "Attempt to invoke a non-indexable value");
                    return INTERPRET_RUNTIME_ERROR;
                }
        
                switch (AS_OBJ(callVal)->type) {
                    case OBJ_INSTANCE: {
                        ObjInstance* ins = AS_INSTANCE(callVal);
                        Value closure;
                
                        if (getTable(&ins->klass->methods, string, &closure)) {
                            // set self
                            vm->stackTop[-argCount - 1] = callVal;
                            if (!callClosure(vm, AS_CLOSURE(closure), shouldReturn, argCount, false)) return INTERPRET_RUNTIME_ERROR;
                        } else if (getTable(&ins->table, string, &closure)) {
                            /* If this is a field, its just a normal callable body */
                            if (!callValue(vm, closure, shouldReturn, argCount)) return INTERPRET_RUNTIME_ERROR;
                        } else {
                            msapi_runtimeError(vm, "Attempt to call a non-callable object");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        break;
                    }
                    case OBJ_ARRAY: {
                        bool result = invokeNativeMethod(vm, string, 
                                callVal.as.obj, argCount, shouldReturn, &vm->arrayMethods);

                        if (!result) return INTERPRET_RUNTIME_ERROR;
                        break;
                    }
                    case OBJ_STRING: {
                        bool result = invokeNativeMethod(vm, string,
                                callVal.as.obj, argCount, shouldReturn, &vm->stringMethods);

                        if (!result) return INTERPRET_RUNTIME_ERROR;
                        break;
                    }
                    case OBJ_DLL_CONTAINER: {
                        bool result = invokeNativeMethod(vm, string,
                                callVal.as.obj, argCount, shouldReturn, &vm->dllMethods);
                        
                        if (!result) return INTERPRET_RUNTIME_ERROR;
                        break;
                    }
                    case OBJ_TABLE: {
                        // search for key 
                        Value value = NIL();
                        bool foundKey = getTable(&AS_TABLE(callVal)->table, string, &value);

                        if (foundKey) {
                            vm->stackTop[-argCount - 1] = value;        // replace table with func
                            if (!callValue(vm, value, shouldReturn, argCount)) return INTERPRET_RUNTIME_ERROR;
                            frame = &vm->frames[vm->frameCount - 1];
                            break;
                        }
                        NativeMethodPtr ptr = NULL;
                        bool foundBoundMethod = getPtrTable(&vm->tableMethods, string, (void*)&ptr);

                        if (!foundBoundMethod) {                        
                            bool found_nokeycall = getTable(&AS_TABLE(callVal)->table, 
                                    allocateString(vm, "_nokeycall", 10), &value);

                            if (found_nokeycall) {
                                ObjArray* array = allocateArray(vm); 

                                for (int i = -argCount; i < 0; i++) {
                                    writeValueArray(&array->array, vm->stackTop[i]);
                                }

                                popn(vm, argCount + 1);
                                push(vm, value); 
                                push(vm, callVal);
                                push(vm, OBJ(string));
                                push(vm, OBJ(array));

                                if (!callValue(vm, value, shouldReturn, 3)) return INTERPRET_RUNTIME_ERROR;
                                frame = &vm->frames[vm->frameCount - 1];
                                break;
                            }
                            
                            msapi_runtimeError(vm, "Attempt to invoke a nil value");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        bool result = (*ptr)(vm, callVal.as.obj, argCount, shouldReturn);
                        if (!result) return INTERPRET_RUNTIME_ERROR;  
                        break;
                    }
                    default: 
                        msapi_runtimeError(vm, "Attempt to invoke a non-indexable object");
                        return INTERPRET_RUNTIME_ERROR;

                }
                frame = &vm->frames[vm->frameCount - 1];
                break;

            }
            case OP_INHERIT: {
                ObjClass* klass = AS_CLASS(peek(vm, 1));
                Value superclass = peek(vm, 0);
                
                if (!CHECK_CLASS(superclass)) {
                    msapi_runtimeError(vm, "Error : Expected superclass to be a class");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                copyTableAll(&AS_CLASS(superclass)->methods, &klass->methods);
                break;
            }
            case OP_GET_SUPER: { 
                ObjClass* super = AS_CLASS(pop(vm));
                ObjInstance* self = AS_INSTANCE(pop(vm));
                ObjString* string = AS_STRING(pop(vm));

                Value field;
                if (getTable(&super->fields, string, &field)) {
                    push(vm, field);
                    break;
                } else if (getTable(&super->methods, string, &field)) {
                    push(vm, OBJ(allocateMethod(vm, self, AS_CLOSURE(field))));
                    break;
                }

                push(vm, NIL());
                break;
            }
            case OP_SUPERCALL: {
                uint8_t argCount = READ_BYTE(frame);
                bool shouldReturn = (bool)READ_BYTE(frame);

                ObjClass* super = AS_CLASS(pop(vm));
                ObjInstance* self = AS_INSTANCE(pop(vm));
                ObjString* string = AS_STRING(peek(vm, argCount));

                Value method;
                if (getTable(&super->methods, string, &method)) {
                    vm->stackTop[-argCount - 1] = OBJ(self);
                    if (!callClosure(vm, AS_CLOSURE(method), shouldReturn, argCount, false)) return INTERPRET_RUNTIME_ERROR;    
                } else if (getTable(&super->fields, string, &method)) {
                    if (!callValue(vm, method, shouldReturn, argCount)) return INTERPRET_RUNTIME_ERROR;
                } else {
                    msapi_runtimeError(vm, "Attempt to call a non-callable object");
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_RET: {
                bool doesReturn = (bool)READ_BYTE(frame);
                bool shouldReturn = frame->shouldReturn;
                Value ret = NIL();
                if (doesReturn) {
                    ret = pop(vm);
                }
                
                /* If function is a coroutine, mark it as dead */
                if (frame->isCoroutine) {
                    AS_COROUTINE(frame->slotPtr[0])->state = CORO_DEAD;
                }

                closeUpvalues(vm, frame->slotPtr);          // move all upvalues to heap
                vm->stackTop = frame->slotPtr;
                
                /* pop the call frame */ 
                vm->frameCount--;
                // Update the frame variable
                frame = &vm->frames[vm->frameCount - 1];
                /* Reset the global environment */
                vm->globals = frame->closure->env;
                if (shouldReturn) {
                    push(vm, ret); 
                }

                break;
            }
            case OP_RETEOF: {
                vm->stackTop = frame->slotPtr;
                vm->frameCount--;
                return INTERPRET_OK;
            }
            case OP_DEFINE_GLOBAL: {
                Value name = READ_CONSTANT(frame);
                insertTable(&vm->globals->table, AS_STRING(name), pop(vm));

                if (vm->currentModule != NULL) {
                    writeObjStringArray(&vm->currentModule->customGlobals, AS_STRING(name));
                }
                break;
            }
            case OP_DEFINE_LONG_GLOBAL: {
                Value name = READ_LONG_CONSTANT(frame);
                insertTable(&vm->globals->table, AS_STRING(name), pop(vm));

                if (vm->currentModule != NULL) {
                    writeObjStringArray(&vm->currentModule->customGlobals, AS_STRING(name));
                }
 
                break;
            }
            case OP_GET_GLOBAL: {
                Value feeder;
                Value name = READ_CONSTANT(frame);
                bool found = getTable(&vm->globals->table, AS_STRING(name), &feeder);

                push(vm, found ? feeder : NIL());
                break;
            }
            case OP_GET_LONG_GLOBAL: {
                Value feeder;
                Value name = READ_LONG_CONSTANT(frame);
                bool found = getTable(&vm->globals->table, AS_STRING(name), &feeder);

                push(vm, found ? feeder : NIL());
                break;
            }
            case OP_ASSIGN_GLOBAL: {
                Value name;
                Value feeder;
                ASSIGN_GLOBAL(vm, frame, name, feeder);
                insertTable(&vm->globals->table, AS_STRING(name), pop(vm));
                break;
            }
            case OP_ASSIGN_LONG_GLOBAL: {
                Value name;
                Value feeder;
                ASSIGN_LONG_GLOBAL(vm, frame, name, feeder);
                insertTable(&vm->globals->table, AS_STRING(name), pop(vm));
                break;
            }
            case OP_PLUS_ASSIGN_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = peek(vm, 0);
                ASSIGN_GLOBAL(vm, frame, name, feeder);
            
                if (CHECK_NUMBER(feeder) && CHECK_NUMBER(increment)) {
                    insertTable(&vm->globals->table, AS_STRING(name), NATIVE_TO_NUMBER(
                            AS_NUMBER(feeder) + AS_NUMBER(increment)
                    ));

                } else if (CHECK_STRING(feeder) && CHECK_STRING(increment)) {
                    insertTable(&vm->globals->table, AS_STRING(name), OBJ(
                                strConcat(vm, feeder, increment)
                    ));
                } else {
                    msapi_runtimeError(vm, "Attempt call '+=' on a non-numeric/string value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                pop(vm);

                break;
            }
            case OP_PLUS_ASSIGN_LONG_GLOBAL: {
                Value name;
                Value feeder;
                Value increment = peek(vm, 0);
                ASSIGN_LONG_GLOBAL(vm, frame, name, feeder);
                
                if (CHECK_NUMBER(feeder) && CHECK_NUMBER(increment)) {
                    insertTable(&vm->globals->table, AS_STRING(name), NATIVE_TO_NUMBER(
                            AS_NUMBER(feeder) + AS_NUMBER(increment)
                    ));

                } else if (CHECK_STRING(feeder) && CHECK_STRING(increment)) {
                    insertTable(&vm->globals->table, AS_STRING(name), OBJ(
                                strConcat(vm, feeder, increment)
                    ));
                } else {
                    msapi_runtimeError(vm, "Attempt call '+=' on a non-numeric/string value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                pop(vm);
    
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
                
                insertTable(&vm->globals->table, AS_STRING(name), NATIVE_TO_NUMBER(
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
                
                insertTable(&vm->globals->table, AS_STRING(name), NATIVE_TO_NUMBER(
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
                
                insertTable(&vm->globals->table, AS_STRING(name), NATIVE_TO_NUMBER(
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
                
                insertTable(&vm->globals->table, AS_STRING(name), NATIVE_TO_NUMBER(
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
                
                if (AS_NUMBER(increment) == 0) {
                    printf("%g\n", AS_NUMBER(increment));
                    msapi_runtimeError(vm, "Cannot divide by 0");
                    return INTERPRET_RUNTIME_ERROR;
                }

                insertTable(&vm->globals->table, AS_STRING(name), NATIVE_TO_NUMBER(
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
                
                if (AS_NUMBER(increment) == 0) {
                    msapi_runtimeError(vm, "Cannot divide by 0");
                    return INTERPRET_RUNTIME_ERROR;
                }


                insertTable(&vm->globals->table, AS_STRING(name), NATIVE_TO_NUMBER(
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
                
                insertTable(&vm->globals->table, AS_STRING(name), NATIVE_TO_NUMBER(
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
                
                insertTable(&vm->globals->table, AS_STRING(name), NATIVE_TO_NUMBER(
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
                Value new = peek(vm, 0);

                if (CHECK_NUMBER(old) && CHECK_NUMBER(new)) {
                    frame->slotPtr[localIndex] = NATIVE_TO_NUMBER(AS_NUMBER(old) + AS_NUMBER(new));
                } else if (CHECK_STRING(old) && CHECK_STRING(new)) {
                    frame->slotPtr[localIndex] = OBJ(strConcat(vm, old, new));                    
                } else {
                    msapi_runtimeError(vm, "Attempt to call '+=' on a non-numeric/string value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                pop(vm);

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
                if (AS_NUMBER(new) == 0) {
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
                    Value string = OBJ(strConcat(vm, peek(vm, 1), peek(vm, 0)));
                    popn(vm, 2);
                    push(vm, string);
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
    vm->running = true;
    _push_(vm, OBJ(function));
    ObjClosure* closure = allocateClosure(vm, function, vm->globals);
    pop(vm);
    _push_(vm, OBJ(closure));
    
    CallFrame newFrame;
    newFrame.ip = function->chunk.code;
    newFrame.closure = closure;
    newFrame.slotPtr = vm->stackTop - 1;
    newFrame.shouldReturn = false;

    vm->frames[vm->frameCount] = newFrame;
    vm->frameCount++;

    InterpretResult result = run(vm); 
    // it gets popped by RETEOF

    vm->running = false;
    return result;
}
