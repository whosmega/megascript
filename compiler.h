#ifndef ls_compiler_h
#define ls_compiler_h
#include <string.h>
#include "vm.h"
#include "chunk.h"

void compile(const char* source, Chunk* chunk); 

#endif
