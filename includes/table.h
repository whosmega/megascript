#ifndef ms_table_h
#define ms_table_h

#include "../includes/value.h"
#include "../includes/common.h"

#define MAX_LOAD_FACTOR 0.6

typedef struct {
    Value key;
    Value value;
} ComplexEntry;

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int capacity;
    int count;
    ComplexEntry* entries;
} ComplexTable;

typedef struct {
    int capacity;
    int count;
    Entry* entries;
} Table;

void initTable(Table* table);
bool insertTable(Table* table, ObjString* key, Value value);
bool deleteTable(Table* table, ObjString* key);
void copyTableAll(Table* from, Table* to);
bool getTable(Table* table, ObjString* key, Value* value);          /* Value is the output paramater */
ObjString* findStringTable(Table* table, char* chars, int length, uint32_t hash);   /* Used for string interning */
void freeTable(Table* table);
uint32_t hash_string(const char* string, int length);
#endif
