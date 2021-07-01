#ifndef ms_table_h
#define ms_table_h

#include "../includes/value.h"
#include "../includes/common.h"

#define MAX_LOAD_FACTOR 0.6


typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int capacity;
    int count;
    Entry* entries;
} Table;

typedef struct {
    ObjString* key;
    void* value;
    bool tombstone;
} PtrEntry;

typedef struct {
    int capacity;
    int count;
    PtrEntry* entries;
} PtrTable;

void initPtrTable(PtrTable* table);
bool insertPtrTable(PtrTable* table, ObjString* key, void* value);
bool getPtrTable(PtrTable* table, ObjString* key, void** value);
void freePtrTable(PtrTable* table);

void initTable(Table* table);
bool insertTable(Table* table, ObjString* key, Value value);
bool deleteTable(Table* table, ObjString* key);
void copyTableAll(Table* from, Table* to);
bool getTable(Table* table, ObjString* key, Value* value);          /* Value is the output paramater */

ObjString* findStringTable(Table* table, char* chars, int length, uint32_t hash);   /* Used for string interning */
void freeTable(Table* table);
uint32_t hash_string(const char* string, int length);
#endif
