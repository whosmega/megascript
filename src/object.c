#include "../includes/common.h"
#include <stdint.h>
#include <string.h>
#include "../includes/memory.h"
#include "../includes/object.h"
#include "../includes/value.h"
#include "../includes/vm.h"
#include "../includes/table.h"
#include "../includes/debug.h"
#include "../includes/lib_ssocket.h"

#ifdef _WIN32
#include "../includes/win_dlfnc.h"
#else 
#include <dlfcn.h>
#include <unistd.h>
#endif

/*
    allocateObject allocates the given size which can be the size of any of its subsidaries (ObjString, ect) 
    It casts it to an Obj* because every subsidary of Obj has the first struct field as an Obj which allows such 
    casts. They can be casted back to the respective type by directly casting the Obj* back to its subsidary type 
    for example : ObjString* string = (ObjString*)someObjPointer

*/

static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
    Obj* obj = (Obj*)reallocate(vm, NULL, 0, size);
    obj->type = type;
    obj->next = vm->ObjHead;
    obj->isMarked = false;
    vm->ObjHead = obj;

    #ifdef DEBUG_LOG_MEMORY
        printf("%p allocating %zu bytes for type %d\n", (void*)obj, size, type);
    #endif

    return obj;
}

/*
    allocateRawString takes the advantage of flexible array members, and allocates the size
    of the string while the object is made,
*/

ObjString* allocateRawString(VM* vm, int length) {
    ObjString* stringObj = (ObjString*)allocateObject(vm, sizeof(*stringObj) + sizeof(char) * (length + 1), OBJ_STRING);
    stringObj->length = length;
    stringObj->allocated[length] = '\0';                /* Handles null byte */
    return stringObj;
}

/*
    allocateString lets allocateRawString create an ObjectString template, and then it fills up the characters 
    for use

    default behaviour is to copy the string given to it, and allocate it inside the ObjString
    Used by the compiler to allocate the string while compiling which belongs to the source and cannot be freed
*/

static inline ObjString* strIntern(VM* vm, char* chars, uint32_t hash, int length) {
    return findStringTable(&vm->strings, chars, length, hash);
}

ObjString* allocateString(VM* vm, const char* chars, int length) {
    uint32_t hash = hash_string(chars, length);
    ObjString* interned = strIntern(vm, (char*)chars, hash, length);
    if (interned != NULL) {
        return interned; 
    }
    ObjString* stringObj = allocateRawString(vm, length);       /* Make room for the null byte */
    memcpy(stringObj->allocated, chars, length);
    stringObj->obj.hash = hash;
    
    insertTable(&vm->strings, stringObj, NIL());
    return stringObj;
}


/*
 * Used to allocate strings at runtime which dont have a source
 * Doesn't expect a null terminated string
*/

ObjString* allocateUnsourcedString(VM* vm, const char* chars, int length) {
    ObjString* stringObj = allocateString(vm, chars, length);
    FREE_ARRAY(char, (char*)chars, length);
    return stringObj;
}

/* 
    utility function to concatenate 2 string values 

    This functions allocates a larger block on the heap which has spae
    for both the strings combined, null terminator isnt neccessary.
    The characters are then copied from the strings to the block. 
    Then the control is passed to allocateUnsourcedString, which manages string interning 
    and returns the allocated ObjString whilst freeing the character block 
    previously allocated 
*/

ObjString* strConcat(VM* vm, Value val1, Value val2) {
    ObjString* str1 = AS_STRING(val1);
    ObjString* str2 = AS_STRING(val2);

    int length = str1->length + str2->length;
    
    char* chars = ALLOCATE(vm, char, length);

    memcpy(chars, &str1->allocated, str1->length);
    memcpy(chars + str1->length, &str2->allocated, str2->length);

    return allocateUnsourcedString(vm, chars, length);
}
        

ObjArray* allocateArray(VM* vm) {
    ObjArray* array = (ObjArray*)allocateObject(vm, sizeof(ObjArray), OBJ_ARRAY);
    initValueArray(&array->array);
    return array; 
}

ObjFunction* allocateFunction(VM* vm, ObjString* name, int arity) {
    ObjFunction* func = (ObjFunction*)allocateObject(vm, sizeof(ObjFunction), OBJ_FUNCTION);
    func->arity = arity;
    func->name = name;
    func->variadic = false;
    func->upvalueCount = 0;
    initChunk(&func->chunk);

    return func;
}

ObjFunction* newFunction(VM* vm, const char* name, int arity) {
    ObjString* nameObj = allocateString(vm, name, strlen(name));
    return allocateFunction(vm, nameObj, arity);
}

ObjFunction* newFunctionFromSource(VM* vm, const char* start, int length, int arity) {
    ObjString* nameObj = allocateString(vm, start, length);
    return allocateFunction(vm, nameObj, arity);
}

ObjNativeFunction* allocateNativeFunction(VM* vm, ObjString* name, NativeFuncPtr funcPtr) {
    ObjNativeFunction* native = (ObjNativeFunction*)allocateObject(vm, sizeof(ObjNativeFunction), OBJ_NATIVE_FUNCTION);

    native->funcPtr = funcPtr;
    native->name = name;

    return native;
}

ObjClosure* allocateClosure(VM* vm, ObjFunction* function, ObjTable* env) {
    ObjUpvalue** upvalues = ALLOCATE(vm, ObjUpvalue*, function->upvalueCount);
    ObjClosure* closure = (ObjClosure*)allocateObject(vm, sizeof(ObjClosure), OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    closure->env = vm->globals;

    for (int i = 0; i < closure->upvalueCount; i++) {
        closure->upvalues[i] = NULL;
    }

    return closure;
}

ObjUpvalue* allocateUpvalue(VM* vm, Value* value) {
    ObjUpvalue* upvalue = (ObjUpvalue*)allocateObject(vm, sizeof(ObjUpvalue), OBJ_UPVALUE);
    upvalue->value = value;
    upvalue->next = NULL;
    return upvalue; 
}

ObjClass* allocateClass(VM* vm, ObjString* name) {
    ObjClass* klass = (ObjClass*)allocateObject(vm, sizeof(ObjClass), OBJ_CLASS);
    klass->name = name;
    initTable(&klass->fields);
    initTable(&klass->methods);
    return klass;
}

ObjInstance* allocateInstance(VM* vm, ObjClass* klass) {
    ObjInstance* instance = (ObjInstance*)allocateObject(vm, sizeof(ObjInstance), OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->table);
    copyTableAll(&klass->fields, &instance->table);
    return instance;
}

ObjMethod* allocateMethod(VM* vm, ObjInstance* instance, ObjClosure* closure) {
    ObjMethod* method = (ObjMethod*)allocateObject(vm, sizeof(ObjMethod), OBJ_METHOD);
    method->closure = closure; 
    method->self = instance;
    
    return method;
}

ObjTable* allocateTable(VM* vm) {
    ObjTable* table = (ObjTable*)allocateObject(vm, sizeof(ObjTable), OBJ_TABLE);
    initTable(&table->table);

    return table;
}

ObjNativeMethod* allocateNativeMethod(VM* vm, ObjString* name, Obj* self, 
        NativeMethodPtr methodPtr) {
    ObjNativeMethod* method = (ObjNativeMethod*)allocateObject(vm, 
            sizeof(ObjNativeMethod), OBJ_NATIVE_METHOD);

    method->name = name;
    method->function = methodPtr;
    method->self = self;

    return method;
}

ObjDllContainer* allocateDllContainer(VM* vm, ObjString* fileName, void* handle) {
    ObjDllContainer* object = (ObjDllContainer*)allocateObject(vm,
            sizeof(ObjDllContainer), OBJ_DLL_CONTAINER);

    object->fileName = fileName;
    object->handle = handle;
    object->closed = false;

    return object;
}

ObjSocket* allocateSocket(VM* vm, int sockfd) {
    ObjSocket* socket = (ObjSocket*)allocateObject(vm,
            sizeof(ObjSocket), OBJ_SOCKET);

    socket->sockfd = sockfd;
    socket->closed = false;
    
    return socket;
}

ObjSSocket* allocateSSocket(VM* vm, SSOCKET* ssocket) {
    ObjSSocket* sock = (ObjSSocket*)allocateObject(vm,
            sizeof(ObjSSocket), OBJ_SSOCKET);

    sock->ssocket = ssocket;
    sock->closed = false;

    return sock;
}

ObjCoroutine* allocateCoroutine(VM* vm, ObjClosure* closure) {
    ObjCoroutine* coro = (ObjCoroutine*)allocateObject(vm,
            sizeof(ObjCoroutine), OBJ_COROUTINE);

    coro->closure = closure;
    coro->state = CORO_YIELDING;
    coro->frames = NULL;
    coro->stack = NULL;
    coro->frameCount = 0;
    coro->stackSize = 0;
    coro->shouldReturn = false;

    return coro;
}

void printObject(Value value) {
    switch (AS_OBJ(value)->type) {
        case OBJ_STRING:
            printf("%s", AS_NATIVE_STRING(value));
            break;
        case OBJ_ARRAY: {
            ObjArray* array = AS_ARRAY(value);
            printf("[");
            for (int i = 0; i < array->array.count; i++) {
                printValue(array->array.values[i]);
                if (i != array->array.count - 1) {
                    printf(", ");
                }
            }
            printf("]");
            break;
        }
        case OBJ_FUNCTION:
            printf("Raw Function <%s>", AS_FUNCTION(value)->name->allocated);
            break;
        case OBJ_CLOSURE:
            printf("Function <%s>", AS_CLOSURE(value)->function->name->allocated);
            break;
        case OBJ_NATIVE_FUNCTION:
            printf("Native Function <%s>", AS_NATIVE_FUNCTION(value)->name->allocated);
            break;
        case OBJ_UPVALUE:
            printf("Upvalue");
            break;
        case OBJ_CLASS:
            printf("Class <%s>", AS_CLASS(value)->name->allocated);
            break;
        case OBJ_INSTANCE:
            printf("Instance <%s>", AS_INSTANCE(value)->klass->name->allocated);
            break;
        case OBJ_METHOD:
            printf("Method <%s>", AS_METHOD(value)->closure->function->name->allocated);
            break;
        case OBJ_TABLE: {
            ObjTable* table = AS_TABLE(value);
        
            bool first = false;
            printf("{");
            for (int i = 0; i < table->table.capacity; i++) {
                Entry entry = table->table.entries[i];

                if (entry.key != NULL) {
                    if (!first) {
                        first = true;
                    } else {
                        printf(", ");
                    }
                    printf("\"");
                    printObject(OBJ(entry.key));
                    printf("\"");
                    printf(" : ");
                    printValue(entry.value);
                }
            }
            printf("}");
            break;
        }
        case OBJ_NATIVE_METHOD:
            printf("Native Method <%s>", AS_NATIVE_METHOD(value)->name->allocated);
            break;
        case OBJ_DLL_CONTAINER:
            printf("Dll Object <%s>", AS_DLL_CONTAINER(value)->fileName->allocated);
            break;
        case OBJ_SOCKET:
            printf("Socket <%d>", AS_SOCKET(value)->sockfd);
            break;
        case OBJ_SSOCKET:
            printf("Secure Socket <>");
            break;
        case OBJ_COROUTINE:
            printf("Coroutine");
            break;
        default: return;
    }
}

void freeObject(VM* vm, Obj* obj) {
    #if defined(DEBUG_LOG_GC) || defined(DEBUG_LOG_MEMORY)
        printf("%p freeing object ", (void*)obj);
        printValue(OBJ(obj));
        printf("\n");
    #endif
    switch (obj->type) {
        case OBJ_STRING: {
            /* Character array will automatically be freed because its 
             * allocated inside the struct due to being a flexible member */
            ObjString* stringObj = (ObjString*)obj;
            reallocate(vm, stringObj, sizeof(*stringObj), 0);
            break;
        }
        case OBJ_ARRAY: {
            /* Free the value array it contains, and the object itself */ 
            ObjArray* arrayObj = (ObjArray*)obj;
            freeValueArray(&arrayObj->array);
            reallocate(vm, arrayObj, sizeof(ObjArray), 0);
            break;
        } 
        case OBJ_FUNCTION: {
            /* Free the chunk, and then the pointer */ 
            // NOTE : The name gets freed individually 
            ObjFunction* funcObj = (ObjFunction*)obj;
            freeChunk(&funcObj->chunk);
            reallocate(vm, funcObj, sizeof(ObjFunction), 0);
            break;
        }
        case OBJ_NATIVE_FUNCTION: {
            ObjNativeFunction* native = (ObjNativeFunction*)obj;
            reallocate(vm, native, sizeof(ObjNativeFunction), 0);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)obj;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            reallocate(vm, closure, sizeof(ObjClosure), 0);
            break;
        }
        case OBJ_UPVALUE: {
            ObjUpvalue* upvalue = (ObjUpvalue*)obj;
            reallocate(vm, upvalue, sizeof(ObjUpvalue), 0);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)obj;
            freeTable(&klass->methods);
            freeTable(&klass->fields); 
            reallocate(vm, klass, sizeof(ObjClass), 0);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)obj;
            freeTable(&instance->table);
            reallocate(vm, instance, sizeof(ObjInstance), 0);
            break;
        }
        case OBJ_METHOD: {
            reallocate(vm, (ObjMethod*)obj, sizeof(ObjMethod), 0);
            break;
        }
        case OBJ_TABLE: {
            ObjTable* table = (ObjTable*)obj;
            freeTable(&table->table);
            reallocate(vm, table, sizeof(ObjTable), 0);
            break;
        }
        case OBJ_NATIVE_METHOD: {
            reallocate(vm, (ObjNativeMethod*)obj, sizeof(ObjNativeMethod), 0);
            break;
        }
        case OBJ_DLL_CONTAINER: {
            ObjDllContainer* container = (ObjDllContainer*)obj;

            if (!container->closed) {
                dlclose(container->handle);
            }

            reallocate(vm, container, sizeof(ObjDllContainer), 0);
            break;
        }
        case OBJ_SOCKET: {
            ObjSocket* socket = (ObjSocket*)obj; 

            if (!socket->closed) {
                close(socket->sockfd);

                printf("Warning : Unclosed Socket '%d'\n", socket->sockfd);
            }
            reallocate(vm, socket, sizeof(ObjSocket), 0);
            break;
        }
        case OBJ_SSOCKET: {
            ObjSSocket* ssocket = (ObjSSocket*)obj;


            if (!ssocket->closed) {
                BIO_free_all(ssocket->ssocket->ssl_bio);
                SSL_CTX_free(ssocket->ssocket->ssl_ctx);

                reallocateArray(ssocket->ssocket, sizeof(SSOCKET), 0);
                printf("Warning : Unclosed Secure Socket");
            }
            
            reallocate(vm, ssocket, sizeof(ObjSSocket), 0);
            break;
        }
        case OBJ_COROUTINE: {
            ObjCoroutine* coro = (ObjCoroutine*)obj;
            
            if (coro->frames != NULL) {
                /* If a previous state was stored, we free the frames */
                reallocate(vm, coro->frames, sizeof(CallFrame) * coro->frameCount, 0);
            }

            if (coro->stack != NULL) {
                reallocate(vm, coro->stack, sizeof(Value) * coro->stackSize, 0);
            }
             
            reallocate(vm, coro, sizeof(ObjCoroutine), 0);
            break;
        }
        default: return;
    }
}

void freeObjects(VM* vm) {
    Obj* object = vm->ObjHead;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(vm, object);
        object = next;
    }

    vm->ObjHead = NULL;
}
