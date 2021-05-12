#include <stdlib.h>
#include "chunk.h"
#include "memory.h"
#include "value.h"

void initChunk(Chunk* chunk) {
    chunk->capacity = 0;        /* Set base capacity to 0 */
    chunk->elem_count = 0;      /* Set default element count as 0 */
    chunk->code = NULL;         /* Set it to point to null */
    chunk->lines = NULL;
    initValueArray(&chunk->constants);                     /* Initialise the constant array */
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->elem_count + 1) {         /* If there isnt enough capacity for the
                                                              incoming byte */
        int old = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old);               /* Update capacity */
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old, chunk->capacity); /* Grow array preprocessor */  
        chunk->lines = GROW_ARRAY(int, chunk->lines, old, chunk->capacity);                                
    }
    
    chunk->code[chunk->elem_count] = byte;                  /* Append Byte */
    chunk->lines[chunk->elem_count] = line;                 /* Add the line */
    chunk->elem_count++;                                    /* Increment count */
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);       /* Free the code array */
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);                      /* Free the constant array we initialised earlier */
    initChunk(chunk);                                       /* Re-Initialize the chunk */
}

int writeConstant(Chunk* chunk, Value value, int line) {
    writeValueArray(&chunk->constants, value);  
    int index = chunk->constants.count - 1;

    if (index <= (1 << 8) - 1) {
        /* if index fits in the 8-bit uint range then use normal loader */
        writeChunk(chunk, OP_CONST, line);
        writeChunk(chunk, index, line);
    } else if (index <= CONSTANT_MAX - 1){
        /* Otherwise load the long constant bytecode */
        /* and split the 16 bit index into 2 8-bit values to write them */
        writeChunk(chunk, OP_CONST_LONG, line);
        writeChunk(chunk, (uint8_t)(((uint16_t)index >> 0) & 0xFF), line);
        writeChunk(chunk, (uint8_t)(((uint16_t)index >> 8) & 0XFF), line);
    }

    return index;
}






