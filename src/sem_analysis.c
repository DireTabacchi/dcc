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

// TODO: implement with interned strings

static String *create_unique_varname(CompDriver *cd, String varname) {
    int digit_len = integer_len(cd->parser.var_count);

    String uvar_string = String_init_length(varname.len + digit_len + 1);
    snprintf(uvar_string.cstr, uvar_string.len+1, "%s.%ld", varname.cstr, cd->parser.var_count);
    cd->parser.var_count += 1;
    String *uvar_name = (String *)StrInterner_intern(&cd->str_table, uvar_string);
    String_free(&uvar_string);
    return uvar_name;
}

static void resolve_expression(CompDriver *cd, Expr *expr) {
    if (expr == NULL) return;

    switch (expr->kind) {
    case EXPR_INVALID:
    case EXPR_CONSTANT:
        break;
    case EXPR_ASSIGN:
        if (expr->as.assign.lhs->kind != EXPR_VAR) {
            err_assign_invalid_lvalue(&cd->errors, cd->tokenizer.src_path, expr->pos);
        }
        resolve_expression(cd, expr->as.assign.lhs);
        resolve_expression(cd, expr->as.assign.rhs);
        break;
    case EXPR_VAR: {
        if (SymTable_contains(&cd->parser.syms, expr->as.var->cstr)) {
            String *canon_name = SymTable_get(&cd->parser.syms, expr->as.var->cstr);
            expr->as.var = canon_name;
        } else {
            err_undeclared_variable(&cd->errors, cd->tokenizer.src_path, expr->pos, *expr->as.var);
        }
        break;
    }
    case EXPR_UNARY:
        resolve_expression(cd, expr->as.unary.expr);
        break;
    case EXPR_BINARY:
        resolve_expression(cd, expr->as.binary.left);
        resolve_expression(cd, expr->as.binary.right);
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
        if (SymTable_contains(&cd->parser.syms, var_decl->as.loc_var.identifier->cstr)) {
            err_redeclared_variable(&cd->errors, cd->tokenizer.src_path,
                var_decl->pos, *var_decl->as.loc_var.identifier);
        }
        String *new_name = create_unique_varname(cd, *var_decl->as.loc_var.identifier);
        SymTable_insert(&cd->parser.syms, (String *)var_decl->as.loc_var.identifier, new_name);
        var_decl->as.loc_var.identifier = new_name;
        if (var_decl->as.loc_var.init != NULL) {
            resolve_expression(cd, var_decl->as.loc_var.init);
        }
        break;
    }
    }
}

static void resolve_statement(CompDriver *cd, BlockItem *item) {
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
    }
}

void sem_analyze(CompDriver *cd) {
    SymTable table = {0};
    SymTable_init(&table);
    Function *func = cd->parser.program->func;
    for (size_t block_idx = 0; block_idx < func->block.len; block_idx++) {
        BlockItem *item = &func->block.items[block_idx];
        switch (item->kind) {
        case BLOCKITEM_INVALID:
            break;
        case BLOCKITEM_DECLARATION:
            resolve_declaration(cd, item);
            break;
        case BLOCKITEM_STATEMENT:
            resolve_statement(cd, item);
            break;
        }
    }
    SymTable_print(&table);
    SymTable_deinit(&table);
}
