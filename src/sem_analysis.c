#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "common.h"
#include "dcc_error.h"
#include "dd_string.h"
#include "comp_driver.h"
#include "sym_table.h"
#include "sem_analysis.h"

// Forward Declarations
static void resolve_block(CompDriver *cd, Block *block);

void Sema_init(Sema *sa) {
    sa->lbl_table = SymTable_create(NULL, 0);
    sa->var_table = SymTable_create(NULL, 0);
}

void Sema_deinit(Sema *sa) {
    SymTable_destroy(sa->var_table);
    SymTable_destroy(sa->lbl_table);
}

static String *create_unique_varname(CompDriver *cd, String varname) {
    int digit_len = integer_len(cd->uid_count);

    String uvar_string = String_init_length(varname.len + digit_len + 1);
    snprintf(uvar_string.cstr, uvar_string.len+1, "%s.%ld", varname.cstr, cd->uid_count);
    cd->uid_count += 1;
    String *uvar_name = (String *)StrInterner_intern(&cd->str_table, uvar_string);
    String_free(&uvar_string);
    return uvar_name;
}

// Determine whether the expression resolves to an lvalue
static bool resolve_lvalue(Expr *expr) {
    switch (expr->kind) {
    case EXPR_VAR:
        return true;
    default:
        return false;
    }
}

static void resolve_expression(CompDriver *cd, Expr *expr) {
    if (expr == NULL) return;

    switch (expr->kind) {
    case EXPR_INVALID:
    case EXPR_CONSTANT:
        break;
    case EXPR_ASSIGN:
        if (!resolve_lvalue(expr->as.assign.lhs)) {
            err_assign_invalid_lvalue(&cd->errors, cd->tokenizer.src_path, expr->pos);
        }
        resolve_expression(cd, expr->as.assign.lhs);
        resolve_expression(cd, expr->as.assign.rhs);
        break;
    case EXPR_VAR: {
        if (SymTable_contains(cd->sema.var_table, expr->as.var->cstr, SYMTYPE_MAPPING)) {
            SymEntry *canon_name =
                SymTable_get(cd->sema.var_table, expr->as.var->cstr, SYMTYPE_MAPPING);
            expr->as.var = canon_name->as.mapping.name;
        } else {
            err_undeclared_variable(&cd->errors, cd->tokenizer.src_path, expr->pos, *expr->as.var);
        }
        break;
    }
    case EXPR_UNARY: {
        switch (expr->as.unary.op) {
        case UNARY_COMPLEMENT:
        case UNARY_NEGATE:
        case UNARY_NOT:
            break;
        case UNARY_PRE_DECR:
        case UNARY_POST_DECR:
            if (!resolve_lvalue(expr->as.unary.expr)) {
                err_decr_not_lvalue(cd, expr->pos);
            }
            break;
        case UNARY_PRE_INCR:
        case UNARY_POST_INCR:
            if (!resolve_lvalue(expr->as.unary.expr)) {
                err_incr_not_lvalue(cd, expr->pos);
            }
        case UNARY_INVALID:
            break;
        }
        resolve_expression(cd, expr->as.unary.expr);
        break;
    }
    case EXPR_BINARY:
        resolve_expression(cd, expr->as.binary.left);
        resolve_expression(cd, expr->as.binary.right);
        break;

    case EXPR_TERNARY:
        resolve_expression(cd, expr->as.ternary.cond);
        resolve_expression(cd, expr->as.ternary.then_expr);
        resolve_expression(cd, expr->as.ternary.else_expr);
        break;
    }
}

static void resolve_declaration(CompDriver *cd, BlockItem *item) {
    if (item == NULL) return;
    if (item->kind != BLOCKITEM_DECLARATION) return;
    
    switch (item->as.declaration->kind) {
    case DECL_INVALID:
        break;
    case DECL_LCL_VAR: {
        Decl *var_decl = item->as.declaration;
        if (SymTable_contains(cd->sema.var_table,
                var_decl->as.loc_var.identifier->cstr, 
                SYMTYPE_MAPPING))
        {
            SymEntry *sym = SymTable_get(cd->sema.var_table,
                var_decl->as.loc_var.identifier->cstr,
                SYMTYPE_MAPPING);
            if (sym->as.mapping.scope == cd->sema.var_table->scope)
                err_redeclared_variable(&cd->errors, cd->tokenizer.src_path,
                    var_decl->pos, *var_decl->as.loc_var.identifier);
        }
        String *new_name = create_unique_varname(cd, *var_decl->as.loc_var.identifier);
        SymTable_insert_mapping(cd->sema.var_table, var_decl->pos,
            (String *)var_decl->as.loc_var.identifier, new_name);
        var_decl->as.loc_var.identifier = new_name;
        if (var_decl->as.loc_var.init != NULL) {
            resolve_expression(cd, var_decl->as.loc_var.init);
        }
        break;
    }
    }
}

static void resolve_statement(CompDriver *cd, Stmt *stmt) {
    if (stmt == NULL) return;

    switch (stmt->kind) {
    case STMT_INVALID:
    case STMT_NULL:
        break;
    case STMT_RET:
        resolve_expression(cd, stmt->as.ret);
        break;
    case STMT_EXPR:
        resolve_expression(cd, stmt->as.expr);
        break;
    case STMT_IF:
        resolve_expression(cd, stmt->as.if_stmt.cond);
        resolve_statement(cd, stmt->as.if_stmt.then_stmt);
        resolve_statement(cd, stmt->as.if_stmt.else_stmt);
        break;
    case STMT_LABELED:
        if (SymTable_contains(cd->sema.lbl_table,
                stmt->as.labeled_stmt.lbl->cstr, SYMTYPE_LABEL)
        ) {
            SymEntry *ent = SymTable_get(cd->sema.lbl_table,
                stmt->as.labeled_stmt.lbl->cstr, SYMTYPE_LABEL);
            if (ent->as.lbl.status == LABEL_REFERENCED) {
                ent->as.lbl.status = LABEL_DEFINED;
            } else {
                err_duplicate_label(cd, stmt->pos, ent);
            }
        } else {
            SymTable_insert_label(cd->sema.lbl_table, stmt->pos,
                stmt->as.labeled_stmt.lbl, LABEL_DEFINED);
        }
        resolve_statement(cd, stmt->as.labeled_stmt.stmt);
        break;
    case STMT_GOTO:
        if (!SymTable_contains(cd->sema.lbl_table, stmt->as.goto_stmt->cstr, SYMTYPE_LABEL)) {
            SymTable_insert_label(cd->sema.lbl_table, stmt->pos,
                stmt->as.labeled_stmt.lbl, LABEL_REFERENCED);
        }
        break;

    case STMT_COMPOUND:
        cd->sema.var_table = SymTable_create(cd->sema.var_table, cd->sema.var_table->scope+1);
        resolve_block(cd, stmt->as.compound_stmt);
        cd->sema.var_table = SymTable_destroy(cd->sema.var_table);
        break;
    }
}

static void resolve_blockitem_statement(CompDriver *cd, BlockItem *item) {
    if (item == NULL) return;
    if (item->kind != BLOCKITEM_STATEMENT) return;

    switch (item->as.statement->kind) {
    case STMT_INVALID:
    case STMT_NULL:
        break;
    case STMT_RET:
        resolve_expression(cd, item->as.statement->as.ret);
        break;
    case STMT_EXPR:
        resolve_expression(cd, item->as.statement->as.expr);
        break;
    case STMT_IF:
        resolve_expression(cd, item->as.statement->as.if_stmt.cond);
        resolve_statement(cd, item->as.statement->as.if_stmt.then_stmt);
        resolve_statement(cd, item->as.statement->as.if_stmt.else_stmt);
        break;
    case STMT_LABELED:
    case STMT_GOTO:
        resolve_statement(cd, item->as.statement);
        break;
    case STMT_COMPOUND:
        resolve_statement(cd, item->as.statement);
        break;
    }
}

static void resolve_block(CompDriver *cd, Block *block) {
    for (size_t block_idx = 0; block_idx < block->len; block_idx++) {
        BlockItem *item = &block->items[block_idx];
        switch (item->kind) {
        case BLOCKITEM_INVALID:
            break;
        case BLOCKITEM_DECLARATION:
            resolve_declaration(cd, item);
            break;
        case BLOCKITEM_STATEMENT:
            resolve_blockitem_statement(cd, item);
            break;
        }
    }
}

static void resolve_labels(CompDriver *cd) {
    SymTable *lbl_table = cd->sema.lbl_table;
    for (size_t lbl_idx = 0; lbl_idx < lbl_table->cap; lbl_idx++) {
        if (lbl_table->syms[lbl_idx].status == STE_OCCUPIED &&
            lbl_table->syms[lbl_idx].type == SYMTYPE_LABEL
        ) {
            puts("Found a lbl");
            SymEntry lbl = lbl_table->syms[lbl_idx];
            if (lbl.as.lbl.status == LABEL_REFERENCED) {
                puts("lbl is in error");
                err_undefined_label(cd, lbl.pos, lbl.key);
            }
        }
    }
}

void sem_analyze(CompDriver *cd) {
    Function *func = cd->parser.program->func;
    resolve_block(cd, func->block);
    resolve_labels(cd);
}
