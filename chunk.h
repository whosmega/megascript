#ifndef ms_chunk_h
#define ms_chunk_h
#include "common.h"
#include "value.h"

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_POW,
    OP_NEGATE,
        
    OP_RET,
    OP_CONST,
    OP_CONST_LONG,
    OP_EOF
} OPCODE;                                       /* Enum which defines opcodes */

typedef struct {
    int capacity;                               /* The capacity of the dynamic array */
    int elem_count;                             /* Number of elements in the dynamic array */
    uint8_t* code;                              /* 8-bit unsigned int dynamic array for storing opcodes */
    ValueArray constants;                       /* Constant pool */
    int* lines;                                 /* Stores lines for debugging */
} Chunk;

#define CONSTANT_MAX 65535

void initChunk(Chunk* chunk);                   /* FUnction to initialize an empty chunk */
void writeChunk(Chunk* chunk, uint8_t byte, int line);     /* Function to write 1 opcode to a chunk */
void freeChunk(Chunk* chunk);                   /* Function to free the chunk and all its contents */
int writeConstant(Chunk* chunk, Value constant, int line);  /* Add a new constant to the constant pool of this chunk */

#endif

