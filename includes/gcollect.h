#ifndef ms_gcollect_h
#define ms_gcollect_h
#include "../includes/vm.h"
#include "../includes/value.h"
#include "../includes/object.h"

void collectGarbage(VM* vm);
void markRoots(VM* vm);
void markValue(VM* vm, Value val);
void markObject(VM* vm, Obj* obj);
void markTable(VM* vm, Table* table);

#endif 
