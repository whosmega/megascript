#include "../includes/gcollect.h"
#include "../includes/debug.h"
#include "../includes/memory.h"

void collectGarbage(VM* vm) {
    /* We dont run our gc during compilation time */ 
    if (!vm->running) return;
    #ifdef DEBUG_LOG_MEMORY
        printf("=== GC BEGIN ===\n");
    #endif

    #if defined(DEBUG_LOG_GC) || defined(DEBUG_LOG_MEMORY)
        size_t before = vm->bytesAllocated;
    #endif
    
    /* Marking phase 
     *
     * We go through every single place which is reachable 
     * by the user's code, such as stack, as well as upvalues via closures, ect 
     * We mark the particular heap allocated value of type obj if we find it to be 
     * reachable */ 
    markRoots(vm);

    /* Tracing phase 
     *
     * We trace the roots and add their inner reachable objects to 
     * a grey list, which gets re visited to "blacken" the inner objects to mark 
     * them as reachable too */ 
    traceObjects(vm);
    
    /* Remove weak references from string intern table */ 
    clearTableWeakref(vm, &vm->strings);

    /* Sweep phase 
     *
     * We iterate over all the objects, free the unmarked ones, and reset the mark */ 
    sweep(vm);
    vm->nextGC = vm->bytesAllocated * 2; 
    #if defined(DEBUG_LOG_MEMORY) || defined(DEBUG_LOG_GC)
        printf("Collected %zu bytes (from %zu to %zu) next at %zu\n", 
                before - vm->bytesAllocated, before, vm->bytesAllocated, 
                vm->nextGC);
    #endif 
    #ifdef DEBUG_LOG_MEMORY
        printf("=== GC END ===\n");
    #endif
}
void markObject(VM* vm, Obj* obj) {
    /* 
     * In some cases, it might not be an object at all, so we 
     * make sure we return */ 
    if (obj == NULL) {
        return;
    }
    if (obj->isMarked) {
        return;
    }
    obj->isMarked = true;
    
    #ifdef DEBUG_LOG_MEMORY
        printf("%p marked ", (void*)obj);
        printValue(OBJ(obj));
        printf("\n");
    #endif
    
    if (vm->greyCapacity < vm->greyCount + 1) {
        vm->greyCapacity = GROW_CAPACITY(vm->greyCapacity);
        vm->greyStack = reallocateArray(vm->greyStack, vm->greyCount * sizeof(Obj*), vm->greyCapacity * sizeof(Obj*));
    }

    vm->greyStack[vm->greyCount++] = obj;
}

void markValue(VM* vm, Value value) {
    /* 
     * We only mark the heap allocated type of objects */
    if (CHECK_OBJ(value)) markObject(vm, AS_OBJ(value));
}

void markTable(VM* vm, Table* table) {
    /* We iterate through the hash table and mark the key as well as its actual value */ 
    for (int i = 0; i < table->capacity; i++) {
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

    /* Mark the running functions in the call stack */ 
    for (int i = 0; i < vm->frameCount; i++) {
        CallFrame frame = vm->frames[i];
        markObject(vm, (Obj*)frame.closure);
    }

    /* Mark open upvalues */
    for (ObjUpvalue* upvalue = vm->UpvalueHead; upvalue != NULL; upvalue=upvalue->next) {
        markObject(vm, (Obj*)upvalue);
    }
}

void markArray(VM* vm, ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(vm, array->values[i]);
    }
}

void blackenObject(VM* vm, Obj* obj) {
    #ifdef DEBUG_LOG_MEMORY
        printf("%p blacken ", (void*)obj);
        printValue(OBJ(obj));
        printf("\n");
    #endif 

    switch (obj->type) {
        case OBJ_CLOSURE: {
            /* Open upvalues directly accessible by closures have already 
             * been marked and are roots */ 
            ObjClosure* closure = (ObjClosure*)obj;
            markObject(vm, (Obj*)closure->function);

            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject(vm, (Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_ARRAY:
            markArray(vm, &(((ObjArray*)obj)->array));
            break;
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)obj;
            markObject(vm, (Obj*)function->name);
            markArray(vm, &function->chunk.constants);
            break;
        }
        case OBJ_NATIVE_FUNCTION: 
            markObject(vm, &((ObjNativeFunction*)obj)->name->obj);
            break;
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)obj;
            markObject(vm, &klass->name->obj); 
            markTable(vm, &klass->fields);
            markTable(vm, &klass->methods);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)obj;
            markObject(vm, &instance->klass->obj);
            markTable(vm, &instance->table);
            break;
        }
        case OBJ_METHOD: {
            ObjMethod* method = (ObjMethod*)obj; 
            markObject(vm, &method->closure->obj);
            markObject(vm, &method->self->obj);
            break;
        }
        case OBJ_TABLE: {
            ObjTable* table = (ObjTable*)obj;
            markTable(vm, &table->table);
            break;
        }
        case OBJ_NATIVE_METHOD: {
            ObjNativeMethod* nativeMethod = (ObjNativeMethod*)obj;
            markObject(vm, nativeMethod->self);
            markObject(vm, &nativeMethod->name->obj);
            break;
        }
        default: return;
    }
}

void traceObjects(VM* vm) {
    while (vm->greyCount > 0) {
        Obj* object = vm->greyStack[--vm->greyCount];
        blackenObject(vm, object);
    }
}

void clearTableWeakref(VM* vm, Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
    
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            deleteTable(table, entry->key);
        }
    }
}

void sweep(VM* vm) {
    Obj* object = vm->ObjHead;
    Obj* prev = NULL;

    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            prev = object;
            object = object->next;
        } else {
            Obj* garbage = object;
            object = object->next;
            if (prev == NULL) {
                vm->ObjHead = object;
            } else {
                prev->next = object;
            }

            freeObject(vm, garbage);
        }
    }
}
