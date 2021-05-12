#ifndef ms_debug_h
#define ms_debug_h
#include "chunk.h"

// #define DEBUG_TRACE_EXECUTION

void dissembleChunk(Chunk* chunk, const char* name);        /* Dissembles an entire given chunk with name */
int dissembleInstruction(Chunk* chunk, int offset);         /* Helper function to dissemle a single instruction 
                                                               which takes an offset (index of instruction) and 
                                                               returns the offset of the next instruction back */

int simpleInstruction(const char* insName, int offset);                 /* Prints and return offset + 1 */
int longConstantInstruction(const char* insName, Chunk* chunk, int offset);
int constantInstruction(const char* insName, Chunk* chunk, int offset);

#endif
