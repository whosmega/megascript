#ifndef ms_gcollect_h
#define ms_gcollect_h
#include "../includes/vm.h"
#include "../includes/value.h"
#include "../includes/object.h"

void collectGarbage(VM* vm);
void markRoots(VM* vm);
void markArray(VM* vm, ValueArray* array);
void markValue(VM* vm, Value val);
void markObject(VM* vm, Obj* obj);
void markTable(VM* vm, Table* table);

void sweep(VM* vm);
void clearTableWeakref(VM* vm, Table* table);
void traceObjects(VM* vm);

#endif 
