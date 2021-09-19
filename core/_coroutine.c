#include "../includes/msapi.h"
#include "../includes/object.h"
#include "../includes/memory.h"
#include "../includes/vm.h"
#include <stdio.h>
#include <string.h>

static ObjUpvalue* unplugUpvalues(VM* vm, Value* slot) {
    /* This function removes the upvalues which are located at or after 
     * the slot given as an argument from the open upvalue array, terminates the 
     * 'next' pointer of the last upvalue with NULL, and returns the first one */

    ObjUpvalue* first = NULL;
    ObjUpvalue* previous = NULL;

    if (vm->UpvalueHead == NULL || vm->UpvalueHead->value < slot) {
        /* If the latest (or the first in order) is before the slot specified, 
         * we dont have any upvalues to handle */
        return NULL;
    }

    first = vm->UpvalueHead;
    while (vm->UpvalueHead != NULL && (vm->UpvalueHead->value >= slot)) {
        previous = vm->UpvalueHead;
        vm->UpvalueHead = vm->UpvalueHead->next; 
    } 
    previous->next = NULL;

    return first;
}

bool create(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 1) {
        msapi_runtimeError(vm, "Insufficient number of arguments, expected 1, got 0");
        return false;
    }

    Value closure = msapi_getArg(vm, 1, argCount);

    if (!CHECK_CLOSURE(closure)) {
        msapi_runtimeError(vm, "Expected a function to create coroutine");
        return false;
    }

    ObjClosure* closurE = AS_CLOSURE(closure);
    ObjCoroutine* coro = allocateCoroutine(vm, closurE);
    
    msapi_popn(vm, argCount + 1);

    if (shouldReturn) {
        msapi_push(vm, OBJ(coro));
    }

    return true;
}

bool resume(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 1) {
        msapi_runtimeError(vm, "Insufficient argument count, expected 1 got 0");
        return false;
    }
    
    Value coroVal = msapi_getArg(vm, 1, argCount);
    if (!CHECK_COROUTINE(coroVal)) {
        msapi_runtimeError(vm, "Expected a coroutine object");
        return false;
    }

    Value arg = NIL();
    if (argCount >= 2) {
        arg = msapi_getArg(vm, 2, argCount);
    }
    
    ObjCoroutine* coro = AS_COROUTINE(coroVal);
    if (coro->state == CORO_DEAD) {
        msapi_runtimeError(vm, "Cannot resume dead coroutine");
        return false;
    } else if (coro->state == CORO_RUNNING) {
        msapi_runtimeError(vm, "Coroutine is already running");
        return false;
    }

    if (coro->frames == NULL) {
        /* No saved state yet 
         * args and coroutine already pushed */
        Value args[argCount - 1];
        
        for (int i = 1; i < argCount; i++) {
            args[i - 1] = msapi_getArg(vm, i + 1, argCount);
        }

        coro->state = CORO_RUNNING;
        msapi_popn(vm, argCount + 1);
        msapi_push(vm, coroVal);

        for (int i = 0; i < argCount - 1; i++) {
            msapi_push(vm, args[i]);
        }
       
        coro->shouldReturn = shouldReturn;
        msapi_callClosure(vm, coro->closure, shouldReturn, argCount - 1, true);
    } else {
        /* Previous state must be loaded in */
        Value retVal = NIL();
        if (argCount >= 2) {
            retVal = msapi_getArg(vm, 2, argCount);
        }

        /* First check if there is enough space on the stack to
         * actually load the coroutine stack in, aswell as the callframe save */

        if (vm->frameCount + coro->frameCount > FRAME_MAX) {
            msapi_runtimeError(vm, "Callframe Stack Overflow while resuming coroutine");
            return false;
        }

        if (((int)(vm->stackTop - vm->stack)) + coro->stackSize > STACK_MAX) {
            msapi_runtimeError(vm, "Stack Overflow while resuming coroutine");
            return false;
        }
            
        msapi_popn(vm, argCount + 1);

        /* Load in the stack */
        memcpy(vm->stackTop, coro->stack, sizeof(Value) * coro->stackSize);
        vm->stackTop += coro->stackSize;

        /* Load in the call frames */
        CallFrame* frameTop = &vm->frames[vm->frameCount];
        memcpy(frameTop, coro->frames, sizeof(CallFrame) * coro->frameCount);
        vm->frameCount += coro->frameCount;
        
        /* Correct the frame slot pointers to point on the real stack instead */
        for (int i = 0; i < coro->frameCount; i++) {
            CallFrame* frame = &frameTop[i];
            frame->slotPtr = frameTop->slotPtr + (frame->slotPtr - coro->frames->slotPtr);
        }

        /* Put the upvalues back in the upvalue array */
        if (coro->upvalues != NULL) {
            ObjUpvalue* head = vm->UpvalueHead;
            ObjUpvalue* last = NULL;

            for (ObjUpvalue* upvalue = coro->upvalues; upvalue != NULL; upvalue = upvalue->next) {
                /* Correct it's value pointer to point to the real stack */
                upvalue->value = frameTop->slotPtr + (upvalue->value - coro->stack);
                last = upvalue;
            }

            last->next = head;
            vm->UpvalueHead = coro->upvalues;
        }

        /* Do the cleanup of the old state */
        reallocate(vm, coro->frames, sizeof(CallFrame) * coro->frameCount, 0);
        reallocate(vm, coro->stack, sizeof(Value) * coro->stackSize, 0);
        coro->stack = NULL;
        coro->upvalues = NULL;
        coro->frames = NULL;
        coro->frameCount = 0;
        coro->stackSize = 0;
        coro->state = CORO_RUNNING;

        if (coro->shouldReturn) {
            msapi_push(vm, arg);
        }

        coro->shouldReturn = shouldReturn;
    }
    
    return true;
}

bool yield(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    /* Yields the last coroutine call */
    CallFrame* lastCoroutine = NULL;
    int saveCount = 0;
    bool returns = false;
    Value returnValue = NIL();

    if (argCount >= 1) {
        returnValue = msapi_getArg(vm, 1, argCount);
        returns = true;
    }

    msapi_popn(vm, argCount + 1);
    for (int i = vm->frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm->frames[i];
        saveCount += 1;
        if (frame->isCoroutine) {
            lastCoroutine = frame;
            break;
        }
    }

    if (lastCoroutine == NULL) {
        msapi_runtimeError(vm, "Not in a coroutine");
        return false;
    }
    
    ObjCoroutine* coroutine = AS_COROUTINE(lastCoroutine->slotPtr[0]);
    int stackSize = (int)(vm->stackTop - lastCoroutine->slotPtr);

    /* We push before allocating to make sure the collector doesnt free
     * our return value which has already been popped from the stack */

    msapi_push(vm, returnValue);   
    CallFrame* frameStorage = reallocate(vm, NULL, 0, sizeof(CallFrame) * saveCount);
    Value* stack = reallocate(vm, NULL, 0, sizeof(Value) * stackSize);
    msapi_pop(vm);
    
    /* Copy the callframes and the stack */    
    memcpy(frameStorage, lastCoroutine, sizeof(CallFrame) * saveCount);
    memcpy(stack, lastCoroutine->slotPtr, sizeof(Value) * stackSize);
    /* Unplug the upvalues from the upvalue array */

    ObjUpvalue* unpluggedUpvalues = unplugUpvalues(vm, &lastCoroutine->slotPtr[0]);
 
    /* Re-adjust the duplicate callframe slot pointers to point to the duplicate stack */
    for (int i = 0; i < saveCount; i++) {
        CallFrame* dupFrame = &frameStorage[i];
        CallFrame* originalFrame = &lastCoroutine[i];
        dupFrame->slotPtr = frameStorage->slotPtr + (originalFrame->slotPtr - lastCoroutine->slotPtr);
    }
    /* Re-adjust the upvalues to point to the values in the fake stack by 
     * calculating the offsets */
    if (unpluggedUpvalues != NULL) {
        /* All offsets are relative to the origin */
        Value* origin = &lastCoroutine->slotPtr[0];
        Value* newOrigin = stack;
        for (ObjUpvalue* upvalue = unpluggedUpvalues; upvalue != NULL; upvalue = upvalue->next) {
            int offset = (int)(upvalue->value - origin);
            upvalue->value = newOrigin + offset;
        }
    }

    coroutine->upvalues = unpluggedUpvalues;
    coroutine->frameCount = saveCount;
    coroutine->frames = frameStorage;
    coroutine->stackSize = stackSize;
    coroutine->stack = stack;
    coroutine->state = CORO_YIELDING;
    
    vm->frameCount -= saveCount;
    vm->stackTop = lastCoroutine->slotPtr;
    
    if (coroutine->shouldReturn) {
        msapi_push(vm, returnValue);
    }
    
    /* We save the yield function's should return state, which can be used by resume */
    coroutine->shouldReturn = shouldReturn;
    return true;
}

bool state(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 1) {
        msapi_runtimeError(vm, "Expected 1 argument, got 0");
        return false;
    }

    Value coroutineVal = msapi_getArg(vm, 1, argCount);

    if (!CHECK_COROUTINE(coroutineVal)) {
        msapi_runtimeError(vm, "Expected a coroutine object");
        return false;
    }

    ObjCoroutine* coroutine = AS_COROUTINE(coroutineVal);
    Value returnValue = NIL();

    switch (coroutine->state) {
        case CORO_RUNNING: returnValue = NATIVE_TO_NUMBER(0); break;
        case CORO_YIELDING: returnValue = NATIVE_TO_NUMBER(1); break;
        case CORO_DEAD: returnValue = NATIVE_TO_NUMBER(2); break;
    }
    
    msapi_popn(vm, argCount + 1);

    if (shouldReturn) {
        msapi_push(vm, returnValue);
    }

    return true;
}
