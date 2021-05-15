#ifndef ms_chunk_h
#define ms_chunk_h
#include "../includes/common.h"
#include "../includes/value.h"

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_POW,
    OP_NEGATE,
    OP_NOT,
    OP_GREATER,
    OP_GREATER_EQ,
    OP_LESSER,
    OP_LESSER_EQ,
    OP_EQUAL,
    OP_NOT_EQ,
    OP_INCR_POST,
    OP_INCR_PRE,
    OP_DECR_POST,
    OP_DECR_PRE,
    OP_AND,
    OP_OR,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_RET,
    OP_CONST,
    OP_CONST_LONG,
    OP_EOF
} OPCODE;                                       /* Enum which defines opcodes */

typedef struct {
    int capacity;                               /* The capacity of the dynamic array */
    int elem_count;                             /* Number of elements in the dynamic array */
    int ins_count;                              /* Number of instructions */
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

