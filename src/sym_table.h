#ifndef SYM_TABLE_H
#define SYM_TABLE_H

#include "common.h"
#include "dd_string.h"

#define TABLE_MIN_CAP 16
#define TABLE_LOAD_FACTOR 0.7

typedef enum symType_ {
    SYMTYPE_MAPPING,
    SYMTYPE_LABEL
} SymType;

typedef enum {
    STE_EMPTY,
    STE_OCCUPIED,
    STE_DELETED
} STEStatus;

typedef enum {
    LABEL_REFERENCED,
    LABEL_DEFINED
} LabelStatus;

typedef struct symEntry_ {
    size_t hash;
    const String *key;
    SymType type;
    STEStatus status;
    Position pos;
    union {
        struct { const String *name; } mapping;
        struct { LabelStatus status; } lbl;
    } as;
} SymEntry;

typedef struct symTable_ {
    SymEntry *syms;
    size_t load;
    size_t cap;
} SymTable;

void SymTable_init(SymTable *table);
void SymTable_deinit(SymTable *table);
void SymTable_insert_mapping(SymTable *table, Position pos, const String *key, const String *value);
void SymTable_insert_label(SymTable *table, Position pos, const String *txt, LabelStatus status);
bool SymTable_contains(SymTable *table, char *key, SymType type);
SymEntry *SymTable_get(SymTable *table, char *key, SymType type);
void SymTable_print(SymTable *table);

#endif // SYM_TABLE_H
