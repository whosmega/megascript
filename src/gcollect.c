#include "../includes/gcollect.h"
#include "../includes/debug.h"

void collectGarbage(VM* vm) {
    #ifdef DEBUG_LOG_MEMORY
        printf("=== GC BEGIN ===\n");
    #endif
    
    /* Marking phase 
     *
     * We go through every single place which is reachable 
     * by the user's code, such as stack, as well as upvalues via closures, ect 
     * We mark the particular heap allocated value of type obj if we find it to be 
     * reachable */ 
    markRoots(vm);

    #ifdef DEBUG_LOG_MEMORY
        printf("=== GC END ===\n");
    #endif
}
void markObject(VM* vm, Obj* obj) {
    /* 
     * In some cases, it might not be an object at all, so we 
     * make sure we return */ 
    if (obj == NULL) return;
    obj->isMarked = true;

    #ifdef DEBUG_LOG_MEMORY
        printf("%p marked object ", (void*)obj);
        printObject(OBJ(obj));
        printf("\n");
    #endif
}

void markValue(VM* vm, Value value) {
    /* 
     * We only mark the heap allocated type of objects */
    if (CHECK_OBJ(value)) markObject(vm, AS_OBJ(value));
}

void markTable(VM* vm, Table* table) {
    /* We iterate through the hash table and mark the key as well as its actual value */ 
    for (int i = 0; i < table->count; i++) {
        Entry* entry = &table->entries[i];
        markObject(vm, &entry->key->obj);
        markValue(vm, entry->value);
    }
}

void markRoots(VM* vm) {
    /* Mark stack variables 
     *
     * Stack top is always pointing to the address just above the 
     * actual top, so we dont include the stack top pointer, but whats below it 
     */ 

    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(vm, *slot);
    }

    /* Mark hash tables such as the VMs global environment */ 
    markTable(vm, &vm->globals);
}
