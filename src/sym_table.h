#ifndef SYM_TABLE_H
#define SYM_TABLE_H

#include "common.h"
#include "dd_string.h"

#define TABLE_MIN_CAP 16
#define TABLE_LOAD_FACTOR 0.7

typedef enum symType_ {
    SYMTYPE_MAPPING,
    SYMTYPE_LABEL,
    SYMTYPE_CASE_LABEL,
    SYMTYPE_SYMBOL
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

typedef enum {
    SCOPE_CURRENT,  // Only the current scope
    SCOPE_ALL       // All scopes
} ScopeStatus;

typedef enum {
    LINKAGE_NONE,
    LINKAGE_INTERNAL,
    LINKAGE_EXTERNAL
} LinkageKind;

typedef enum {
    SYMBOL_TYPE_AUTO,   // Automatic storage duration
    SYMBOL_TYPE_STATIC, // Static storage duration
    SYMBOL_TYPE_FN
} SymbolTypeKind;

typedef enum {
    DATATYPE_INT
} DataType;

typedef enum {
    IV_NO_INITIALIZER,
    IV_TENTATIVE,
    IV_INITIAL
} InitValueKind;

typedef struct fnType_ {
    size_t arity;
} FnType;

typedef struct fnAttrs_ {
    LinkageKind linkage;
    bool defined;
} FnAttr;

typedef struct initVal_ {
    InitValueKind kind;
    union {
        int initial;
    } as;
} InitVal;

typedef struct staticAttrs_ {
    LinkageKind linkage;
    InitVal initial_value;
} StaticAttr;

typedef struct localAttrs_ {
    LinkageKind linkage;
} LocalAttr;

typedef struct symbol_ {
    SymbolTypeKind type;
    const String *origin_name;
    union {
        struct { FnType type; FnAttr attr; } fn;
        struct { DataType type; StaticAttr attr; } static_var;
        struct { DataType type; LocalAttr attr; } auto_var;
    } as;
} Symbol;

typedef struct symEntry_ {
    size_t hash;
    const String *key;
    SymType type;
    STEStatus status;
    Position pos;
    union {
        struct { const String *name; size_t scope; LinkageKind linkage; } mapping;
        struct { LabelStatus status; } lbl;
        struct { int val; } case_lbl;
        Symbol sym;
    } as;
} SymEntry;

typedef struct symTable_ *SymTable_ty;
typedef struct symTable_ {
    SymEntry *syms;
    size_t load;
    size_t cap;

    size_t scope;
    SymTable_ty parent;
} SymTable;

SymTable *SymTable_create(SymTable *parent, size_t scope);
/* Destroy the current table.
If the table has a parent, returns a pointer to the parent.
Otherwise, if there is no parent or the passed table is NULL, returns NULL. */
SymTable *SymTable_destroy(SymTable *table);
void SymTable_insert_mapping(SymTable *table, Position pos, const String *key, const String *value,
    LinkageKind linkage);
void SymTable_insert_label(SymTable *table, Position pos, const String *txt, LabelStatus status);
void SymTable_insert_case_label(SymTable *table, Position pos, const String *lbl, int val);
void SymTable_insert_auto_var_symbol(SymTable *table, Position pos, const String *key,
    const String *origin_name);
void SymTable_insert_static_var_symbol(SymTable *table, Position pos, const String *key,
    const String *origin_name, StaticAttr attr);
void SymTable_insert_fn_symbol(SymTable *table, Position pos, const String *key,
    const String *origin_name, size_t param_cnt, FnAttr attr);
bool SymTable_contains(SymTable *table, char *key, SymType type);
SymEntry *SymTable_get(SymTable *table, char *key, SymType type);
bool SymTable_scope_contains(SymTable *table, char *key, SymType type);
SymEntry *SymTable_scope_get(SymTable *table, char *key, SymType type);
void SymTable_print(SymTable *table);
void SymEntry_print(SymEntry *entry);

typedef enum LabelKind_ {
    AND_FALSE,
    AND_END,
    OR_TRUE,
    OR_END,
    IF_ELSE,
    IF_END,
    TERN_ELSE,
    TERN_END,
    STMT_LABEL,
    LOOP_WHILE,
    LOOP_DOWHILE,
    LOOP_FOR,
    SWITCH,
    LABEL_KIND_LENGTH
} LabelKind;

static String label_kind_table[LABEL_KIND_LENGTH] = {
    (String){ .cstr = (char*)"and_false",       .len = 9 },
    (String){ .cstr = (char*)"and_end",         .len = 7 },
    (String){ .cstr = (char*)"or_true",         .len = 7 },
    (String){ .cstr = (char*)"or_end",          .len = 6 },
    (String){ .cstr = (char*)"if_else",         .len = 7 },
    (String){ .cstr = (char*)"if_end",          .len = 6 },
    (String){ .cstr = (char*)"tern_else",       .len = 9 },
    (String){ .cstr = (char*)"tern_end",        .len = 8 },
    (String){ .cstr = (char *)"stmt",           .len = 4 },
    (String){ .cstr = (char*)"loop_while",      .len = 10 },
    (String){ .cstr = (char*)"loop_do_while",  .len = 13 },
    (String){ .cstr = (char*)"loop_for",        .len = 8 },
    (String){ .cstr = (char*)"switch",          .len = 6 },
};

#endif // SYM_TABLE_H
