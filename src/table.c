#include "../includes/table.h"
#include "../includes/memory.h"
#include "../includes/object.h"
#include "../includes/value.h"
#include <string.h>
#include <stdint.h>

void initTable(Table* table) {
    /* Set default fields */
    table->capacity = 0;
    table->count = 0;
    table->entries = NULL;
}

static Entry* probeEntrySlot(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->obj.hash & (capacity - 1);
    for (;;) {
        Entry* entry = &entries[index];
        Entry* tombstone = NULL;
        /* NOTE : We can compare 2 keys directly because strings are interned */
        
        if (entry->key == NULL) {
            if (!(CHECK_NIL(entry->value))) {
                // Tombstone
                if (tombstone == NULL) tombstone = entry;
            } else {
                // truly empty entry / previous tombstone 

                return tombstone == NULL ? entry : tombstone;
            }
        } else if (entry->key == key) {
            /* We found its own key */
            return entry;
        }
        
        /* the entry wasnt empty, so continue the linear search */
        index = (index + 1) & (capacity - 1);
    }
    
}

static void adjustCapacity(Table* table, int capacity) {
    /* The entries are positioned relative to the older capacity, to make sure 
     * the probe sequence remains problem free and returns the current result
     * after changing the capacity, they have to be reinserted completely */  
    Entry* entries = ALLOCATE_ARRAY(Entry, capacity);
    table->count = 0;
    /* Reset all slots back to NULL */ 
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL();
    }
    
    /* Copy all valid entries */
    for (int i = 0; i < table->capacity; i++) {
        Entry currentEntry = table->entries[i];
        // Discard all non tombstone entries
        // Only copy real entries 
        if (currentEntry.key != NULL) {
            Entry* entry = probeEntrySlot(entries, capacity, currentEntry.key);
            entry->key = currentEntry.key;
            entry->value = currentEntry.value;
            table->count++;
        }
    }

    table->entries = entries;
    table->capacity = capacity;
}

bool insertTable(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * MAX_LOAD_FACTOR) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }
    
    
    Entry* entry = probeEntrySlot(table->entries, table->capacity, key);
    bool isNewEntry = entry->key == NULL;
    Value oldValue = entry->value;
    entry->key = key;
    entry->value = value;
    if (isNewEntry && CHECK_NIL(oldValue)) table->count++;
    return isNewEntry;
}


bool getTable(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;
    Entry* entry = probeEntrySlot(table->entries, table->capacity, key);
    
    if (entry->key == NULL) { 
        return false;
    }
    *value = entry->value;
    return true;
}

void copyTableAll(Table* from, Table* to) {
    for (int i = 0 ; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];

        if (entry->key != NULL) {
            insertTable(to, entry->key, entry->value);
        }
    }
}

/* This function only checks if the key exists */

ObjString* findStringTable(Table* table, char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;
    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            if (CHECK_NIL(entry->value)) {
                return NULL;
            }
        } else if (entry->key->length == length && 
                   entry->key->obj.hash == hash &&
                   memcmp(&entry->key->allocated, chars, length) == 0) {
            return entry->key;
        }
        index = (index + 1) & (table->capacity - 1);    
    }

}

bool deleteTable(Table* table, ObjString* key) {
    if (table->count == 0) return false;

    Entry* entry = probeEntrySlot(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    /* Place a tombstone */ 
    entry->key = NULL;
    entry->value = NATIVE_TO_BOOLEAN(false);

    return true;
}

uint32_t hash_string(const char* string, int length) {
    uint32_t hash;

    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)string[i];
        hash *= 16777619;
    }

    return hash;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->count);    /* Free the array */
    initTable(table);                                   /* Reset */
}
