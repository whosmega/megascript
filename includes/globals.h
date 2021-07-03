#ifndef ms_globals_h
#define ms_globals_h
#include "../includes/vm.h"
#include "../includes/value.h"
#include "../includes/object.h"

void injectGlobals(VM* vm);

bool msglobal_print(VM* vm, int argCount, int returnCount);
bool msglobal_str(VM* vm, int argCount, int returnCount);
bool msglobal_num(VM* vm, int argCount, int returnCount);
bool msglobal_clock(VM* vm, int argCount, int returnCount);
bool msglobal_type(VM* vm, int argCount, int returnCount);
bool msglobal_input(VM* vm, int argCount, int returnCount);

#endif
