#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "dd_string.h"
#include "sym_table.h"

SymTable *SymTable_create(SymTable *parent, size_t scope) {
    SymTable *table = (SymTable *)malloc(sizeof(SymTable));
    table->cap = TABLE_MIN_CAP;
    table->load = 0;
    table->syms = (SymEntry *)calloc(table->cap, sizeof(SymEntry));
    table->parent = parent;
    table->scope = scope;
    return table;
}

SymTable *SymTable_destroy(SymTable *table) {
    if (table == NULL) return NULL;
    SymTable *parent = NULL;
    parent = table->parent;
    free(table->syms);
    free(table);
    table = NULL;

    return parent;
}

static void SymTable_insert_raw(SymTable *table, SymEntry se, size_t hash) {
    size_t idx = hash % table->cap;

    while (table->syms[idx].status == STE_OCCUPIED) {
        SymEntry *table_ent = &table->syms[idx];
        if (table_ent->type == se.type) {
                if (table_ent->hash == hash &&
                    strcmp(table_ent->key->cstr, se.key->cstr) == 0)
                {
                    switch (table_ent->type) {
                    case SYMTYPE_MAPPING:
                        table_ent->as.mapping.name = se.as.mapping.name;
                        return;
                    case SYMTYPE_LABEL:
                        table_ent->as.lbl.status = se.as.lbl.status;
                        return;
                    case SYMTYPE_CASE_LABEL:
                        table_ent->as.case_lbl.val = se.as.case_lbl.val;
                        return;
                    case SYMTYPE_SYMBOL:
                        table_ent->as.sym = se.as.sym;
                        return;
                    }
                }
        }
        idx = (idx + 1) % table->cap;
    }

    table->syms[idx] = se;
    table->load += 1;
}

static void SymTable_resize(SymTable *table) {
        size_t old_cap = table->cap;
        size_t new_cap = old_cap + (old_cap >> 1);
        SymEntry *old_entries = table->syms;
        table->syms = (SymEntry *)calloc(new_cap, sizeof(SymEntry));
        table->cap = new_cap;
        for (size_t se_idx = 0; se_idx < old_cap; se_idx++) {
            if (old_entries[se_idx].status == STE_OCCUPIED) {
                SymTable_insert_raw(table,
                    old_entries[se_idx],
                    old_entries[se_idx].hash);
            }
        }
        free(old_entries);
}

void SymTable_insert_mapping(
    SymTable *table,
    Position pos,
    const String *key,
    const String *value,
    LinkageKind linkage
) {
    if ((float)(table->load + 1) / table->cap > TABLE_LOAD_FACTOR) {
        SymTable_resize(table);
    }

    size_t hash = fnv1a_hash(key->cstr);
    SymEntry se = (SymEntry){
        .hash = hash, .status = STE_OCCUPIED, .type = SYMTYPE_MAPPING, .key = key,
        .pos = pos, .as.mapping = { .name = value, .scope = table->scope, .linkage = linkage }
    };
    SymTable_insert_raw(table, se, hash);
}

void SymTable_insert_label(SymTable *table, Position pos, const String *txt, LabelStatus status) {
    if ((float)(table->load + 1) / table->cap > TABLE_LOAD_FACTOR) {
        SymTable_resize(table);
    }

    size_t hash = fnv1a_hash(txt->cstr);
    SymEntry se = (SymEntry){
        .hash = hash, .status = STE_OCCUPIED, .type = SYMTYPE_LABEL, .key = txt, .pos = pos,
        .as.lbl.status = status
    };
    SymTable_insert_raw(table, se, hash);
}

void SymTable_insert_case_label(SymTable *table, Position pos, const String *lbl, int val) {
    if ((float)(table->load+1) / table->cap > TABLE_LOAD_FACTOR)
        SymTable_resize(table);

    size_t hash = fnv1a_hash(lbl->cstr);
    SymEntry se = (SymEntry){
        .hash = hash, .status = STE_OCCUPIED, .type = SYMTYPE_CASE_LABEL, .key = lbl, .pos = pos,
        .as.case_lbl.val = val
    };
    SymTable_insert_raw(table, se, hash);
}

void SymTable_insert_static_var_symbol(
    SymTable *table,
    Position pos,
    const String *key,
    const String *origin_name,
    StaticAttr attr
) {
    if ((float)(table->load + 1) / table->cap > TABLE_LOAD_FACTOR) {
        SymTable_resize(table);
    }

    size_t hash = fnv1a_hash(key->cstr);
    SymEntry se = (SymEntry){
        .hash = hash, .status = STE_OCCUPIED, .type = SYMTYPE_SYMBOL, .key = key, .pos = pos,
        .as.sym = {
            .type = SYMBOL_TYPE_STATIC,
            .origin_name = origin_name,
            .as.static_var = { .type = DATATYPE_INT, .attr = attr }
        }
    };
    SymTable_insert_raw(table, se, hash);
}

void SymTable_insert_auto_var_symbol(
    SymTable *table,
    Position pos,
    const String *key,
    const String *origin_name
) {
    if ((float)(table->load + 1) / table->cap > TABLE_LOAD_FACTOR) {
        SymTable_resize(table);
    }

    size_t hash = fnv1a_hash(key->cstr);
    SymEntry se = (SymEntry){
        .hash = hash, .status = STE_OCCUPIED, .type = SYMTYPE_SYMBOL, .key = key, .pos = pos,
        .as.sym = {
            .type = SYMBOL_TYPE_AUTO,
            .origin_name = origin_name,
            .as.auto_var = {
                .attr = { .linkage = LINKAGE_NONE },
                .type = DATATYPE_INT
            }
        }
    };
    SymTable_insert_raw(table, se, hash);
}

void SymTable_insert_fn_symbol(
    SymTable *table,
    Position pos,
    const String *key,
    const String *origin_name,
    size_t param_cnt,
    FnAttr attr
) {
    if ((float)(table->load + 1) / table->cap > TABLE_LOAD_FACTOR) {
        SymTable_resize(table);
    }

    size_t hash = fnv1a_hash(key->cstr);
    SymEntry se = (SymEntry){
        .hash = hash, .status = STE_OCCUPIED, .type = SYMTYPE_SYMBOL, .key = key, .pos = pos,
        .as.sym = {
            .type = SYMBOL_TYPE_FN,
            .origin_name = origin_name,
            .as.fn = { .type.arity = param_cnt, .attr = attr }
        }
    };
    SymTable_insert_raw(table, se, hash);
}

bool SymTable_contains(SymTable *table, char *key, SymType type) {
    return SymTable_get(table, key, type) != NULL;
}

static SymEntry *SymTable_get_raw(SymTable *table, char *key, size_t hash, SymType type) {
    size_t idx = hash % table->cap;
    size_t start_idx = idx;

    while (table->syms[idx].status == STE_OCCUPIED) {
        if (table->syms[idx].type == type &&
            table->syms[idx].hash == hash &&
            strcmp(table->syms[idx].key->cstr, key) == 0)
        {
            return &table->syms[idx];
        }
        idx = (idx + 1) % table->cap;
        if (start_idx == idx) break;
    }

    return NULL;
}

SymEntry *SymTable_get(SymTable *table, char *key, SymType type) {
    size_t hash = fnv1a_hash(key);
    SymEntry *res = SymTable_get_raw(table, key, hash, type);

    if (res == NULL && table->parent != NULL)
        return SymTable_get(table->parent, key, type);

    return res;
}

SymEntry *SymTable_scope_get(SymTable *table, char *key, SymType type) {
    size_t hash = fnv1a_hash(key);
    SymEntry *res = SymTable_get_raw(table, key, hash, type);

    return res;
}

bool SymTable_scope_contains(SymTable *table, char *key, SymType type) {
    return SymTable_scope_get(table, key, type) != NULL;
}

void SymTable_print(SymTable *table) {
    if (table == NULL) {
        puts("table is NULL");
        return;
    }

    printf("SymTable scope: %ld\n", table->scope);

    for (size_t idx = 0; idx < table->cap; idx++) {
        if (table->syms[idx].status == STE_OCCUPIED) {
            SymEntry se = table->syms[idx];
            switch (se.type) {
            case SYMTYPE_MAPPING:
                printf("[%03ld] -> Mapping (%s, %s), 0x%016lX, %s\n", idx,
                    table->syms[idx].key->cstr,
                    table->syms[idx].as.mapping.name->cstr, table->syms[idx].hash,
                    table->syms[idx].as.mapping.linkage == LINKAGE_INTERNAL ? 
                    "internal" : "external");
                break;
            case SYMTYPE_LABEL:
                printf("[%03ld] -> Label (%s, %d), 0x%016lX\n", idx,
                    table->syms[idx].key->cstr,
                    table->syms[idx].as.lbl.status, table->syms[idx].hash);
                break;
            case SYMTYPE_SYMBOL:
                printf("[%03ld] -> Symbol `%s`, 0x%016lX, ", idx,
                    table->syms[idx].key->cstr, table->syms[idx].hash);
                switch (table->syms[idx].as.sym.type) {
                case SYMBOL_TYPE_AUTO:
                    printf("(automatic) int, No linkage\n");
                    break;
                case SYMBOL_TYPE_STATIC:
                    printf("(static) int, ");
                    switch(table->syms[idx].as.sym.as.static_var.attr.linkage) {
                    case LINKAGE_NONE:
                        printf("No linkage, ");
                        break;
                    case LINKAGE_EXTERNAL:
                        printf("External linkage, ");
                        break;
                    case LINKAGE_INTERNAL:
                        printf("Internal linkage, ");
                        break;
                    }
                    switch(table->syms[idx].as.sym.as.static_var.attr.initial_value.kind) {
                    case IV_NO_INITIALIZER:
                        printf("No Initializer\n");
                        break;
                    case IV_INITIAL:
                        printf("Initial(%d)\n",
                            table->syms[idx].as.sym.as.static_var.attr.initial_value.as.initial);
                        break;
                    case IV_TENTATIVE:
                        printf("Tentative\n");
                        break;
                    }
                    break;
                case SYMBOL_TYPE_FN:
                    printf("function(%lu), ", table->syms[idx].as.sym.as.fn.type.arity);
                    switch (table->syms[idx].as.sym.as.fn.attr.linkage) {
                    case LINKAGE_NONE:
                        printf("No linkage\n");
                        break;
                    case LINKAGE_EXTERNAL:
                        printf("External linkage\n");
                        break;
                    case LINKAGE_INTERNAL:
                        printf("Internal linkage\n");
                        break;
                    }
                    break;
                }
                break;
            }
        }
    }

    if (table->parent != NULL) SymTable_print(table->parent);
}

void SymEntry_print(SymEntry *entry) {
    switch (entry->type) {
    case SYMTYPE_MAPPING:
        printf("Mapping (%s, %s), 0x%016lX, %s\n", entry->key->cstr, entry->as.mapping.name->cstr,
            entry->hash, entry->as.mapping.linkage == LINKAGE_INTERNAL ? "internal" : "external");
        break;
    case SYMTYPE_LABEL:
        printf("Label (%s, %d), 0x%016lX\n", entry->key->cstr, entry->as.lbl.status, entry->hash);
        break;
    case SYMTYPE_SYMBOL:
        printf("Symbol `%s`, 0x%016lX, ",
            entry->key->cstr, entry->hash);
        switch (entry->as.sym.type) {
        case SYMBOL_TYPE_AUTO:
            printf("int\n");
            break;
        case SYMBOL_TYPE_STATIC:
            printf("static int\n");
            break;
        case SYMBOL_TYPE_FN:
            printf("function(%lu)\n", entry->as.sym.as.fn.type.arity);
            break;
        }
        break;
    }
}
