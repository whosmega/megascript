#ifndef ms_msapi_h 
#define ms_msapi_h

#include "../includes/vm.h"
/*          API             */ 
void msapi_runtimeError(VM* vm, const char* format, ...); 
bool msapi_isFalsey(Value value);
bool msapi_isEqual(Value value1, Value value2);
void msapi_push(VM* vm, Value value);
Value msapi_pop(VM* vm);
Value msapi_peek(VM* vm, unsigned int index);
Value* msapi_peekptr(VM* vm, unsigned int index);
void msapi_pushn(VM* vm, Value value, unsigned int count);
void msapi_popn(VM* vm, unsigned int count); 
Value msapi_getArg(VM* vm, int number, int argCount);

#endif
