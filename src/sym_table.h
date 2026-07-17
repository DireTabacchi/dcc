#ifndef SYM_TABLE_H
#define SYM_TABLE_H

#include "dd_string.h"

#define TABLE_MIN_CAP 16
#define TABLE_LOAD_FACTOR 0.7

typedef enum {
    STE_EMPTY,
    STE_OCCUPIED,
    STE_DELETED
} STEStatus;

typedef struct symEntry_ {
    size_t hash;
    String *key;
    String *value;
    STEStatus status;
} SymEntry;

typedef struct symTable_ {
    SymEntry *syms;
    size_t load;
    size_t cap;
} SymTable;

void SymTable_init(SymTable *table);
void SymTable_deinit(SymTable *table);
void SymTable_insert(SymTable *table, String *key, String *value);
bool SymTable_contains(SymTable *table, char *key);
String *SymTable_get(SymTable *table, char *key);
void SymTable_print(SymTable *table);

#endif // SYM_TABLE_H
