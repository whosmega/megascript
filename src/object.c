#include "../includes/common.h"
#include <stdint.h>
#include <string.h>
#include "../includes/memory.h"
#include "../includes/object.h"
#include "../includes/value.h"
#include "../includes/vm.h"
#include "../includes/table.h"

/*
    allocateObject allocates the given size which can be the size of any of its subsidaries (ObjString, ect) 
    It casts it to an Obj* because every subsidary of Obj has the first struct field as an Obj which allows such 
    casts. They can be casted back to the respective type by directly casting the Obj* back to its subsidary type 
    for example : ObjString* string = (ObjString*)someObjPointer

*/

static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
    Obj* obj = (Obj*)reallocate(NULL, 0, size);
    obj->type = type;
    obj->next = vm->ObjHead;
    vm->ObjHead = obj;
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
    
    char* chars = ALLOCATE(char, length);

    memcpy(chars, &str1->allocated, str1->length);
    memcpy(chars + str1->length, &str2->allocated, str2->length);

    return allocateUnsourcedString(vm, chars, length);
}

void printObject(Value value) {
    switch (AS_OBJ(value)->type) {
        case OBJ_STRING:
            printf("%s", AS_NATIVE_STRING(value));
            break;
        default: return;
    }
}

static void freeObject(Obj* obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            /* Character array will automatically be freed because its 
             * allocated inside the struct due to being a flexible member */
            ObjString* stringObj = (ObjString*)obj;
            reallocate(stringObj, sizeof(*stringObj), 0);
            break;
        }
        default: return;
    }
}

void freeObjects(VM* vm) {
    Obj* object = vm->ObjHead;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }

    vm->ObjHead = NULL;
}
