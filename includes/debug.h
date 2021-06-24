#ifndef ms_debug_h
#define ms_debug_h
#include "../includes/chunk.h"

/* Traces execution for every bytecode instruction it executes 
 * and prints the stack */

// #define DEBUG_TRACE_EXECUTION

/* Prints the bytecode which the compiler compiled 
 * along with operands, useful for 
 * testing the compiler */

// #define DEBUG_PRINT_BYTECODE

/* Stresses the garbage collector by running it for every allocation 
 * regardless of the threshold meeting, useful for debugging gc */

// #define DEBUG_STRESS_GC

/* Logs memory operations like allocation and freeing, as well as 
 * GC Statistics for various phases */
// #define DEBUG_LOG_MEMORY

/* Logs only garbage collection stats */ 
// #define DEBUG_LOG_GC

void dissembleChunk(int scope, Chunk* chunk, const char* name);        /* Dissembles an entire given chunk with name */
int dissembleInstruction(Chunk* chunk, int offset);         /* Helper function to dissemle a single instruction 
                                                               which takes an offset (index of instruction) and 
                                                               returns the offset of the next instruction back */

int simpleInstruction(const char* insName, int offset);                 /* Prints and return offset + 1 */
int longConstantInstruction(const char* insName, Chunk* chunk, int offset);
int constantInstruction(const char* insName, Chunk* chunk, int offset);

#endif
