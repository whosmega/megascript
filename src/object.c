#include "../includes/common.h"
#include <string.h>
#include "../includes/memory.h"
#include "../includes/object.h"
#include "../includes/value.h"
#include "../includes/vm.h"

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
    stringObj->chars = &stringObj->allocated[0];
    return stringObj;
}

/*
    allocateString lets allocateRawString create an ObjectString template, and then it fills up the characters 
    for use
*/

ObjString* allocateString(VM* vm, const char* chars, int length) {
    ObjString* stringObj = allocateRawString(vm, length);       /* Make room for the null byte */
    memcpy(stringObj->allocated, chars, length);
    return stringObj;
}

/* 
    utility function to concatenate 2 string values 
*/

ObjString* strConcat(VM* vm, Value val1, Value val2) {
    ObjString* str1 = AS_STRING(val1);
    ObjString* str2 = AS_STRING(val2);

    int length = str1->length + str2->length;
    ObjString* conc = allocateRawString(vm, length);

    memcpy(conc->allocated, str1->chars, str1->length);
    memcpy(conc->allocated + str1->length, str2->chars, str2->length);
    
    return conc;
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
            stringObj->chars = NULL;
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