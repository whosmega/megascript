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
    OP_DEFINE_GLOBAL,
    OP_DEFINE_LONG_GLOBAL,
    OP_GET_GLOBAL,
    OP_GET_LONG_GLOBAL,
    OP_ASSIGN_GLOBAL,
    OP_ASSIGN_LONG_GLOBAL,
    OP_PLUS_ASSIGN_GLOBAL,
    OP_PLUS_ASSIGN_LONG_GLOBAL,
    OP_SUB_ASSIGN_GLOBAL,
    OP_SUB_ASSIGN_LONG_GLOBAL,
    OP_MUL_ASSIGN_GLOBAL,
    OP_MUL_ASSIGN_LONG_GLOBAL,
    OP_DIV_ASSIGN_GLOBAL,
    OP_DIV_ASSIGN_LONG_GLOBAL,
    OP_POW_ASSIGN_GLOBAL,
    OP_POW_ASSIGN_LONG_GLOBAL,
    OP_ASSIGN_LOCAL,
    OP_PLUS_ASSIGN_LOCAL,
    OP_MINUS_ASSIGN_LOCAL,
    OP_MUL_ASSIGN_LOCAL,
    OP_DIV_ASSIGN_LOCAL,
    OP_POW_ASSIGN_LOCAL,
    OP_GET_LOCAL,
    OP_POP,
    OP_POPN,
    OP_JMP,
    OP_JMP_FALSE,
    OP_JMP_LONG,
    OP_JMP_FALSE_LONG,
    OP_JMP_BACK,
    OP_JMP_BACK_LONG,                           
    OP_ARRAY,                                   /* push empty array */
    OP_ARRINIT,                                 /* push array with 'n' number of elements 
                                                   popped from stack in order */ 
                                                   
    OP_ARRAY_INS,                               /* Insert new element to the last slot
                                                   Doesnt pop off the array */
    OP_ARRAY_PINS,                              /* same as above but pops the array */
    OP_ARRAY_MOD,                               /* modify element at the given index */
    OP_ARRAY_GET,                               /* push element at the given index */ 

    OP_PRINT,                                   /* printing opcode for debugging only */ 
    OP_EOF                                      /* end of file */
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
void writeLongByte(Chunk* chunk, uint16_t byte, int line);
void writeLongByteAt(Chunk* chunk, uint16_t byte, unsigned int index, int line);
void freeChunk(Chunk* chunk);                   /* Function to free the chunk and all its contents */
int writeConstant(Chunk* chunk, Value value, int line);  /* Add a new constant to the constant pool of this chunk */
int makeConstant(Chunk* chunk, Value value);

#endif

