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
    sa->lbl_count = 0;
}

void Sema_deinit(Sema *sa) {
    SymTable_destroy(sa->var_table);
    SymTable_destroy(sa->lbl_table);
}

static const String *create_unique_varname(CompDriver *cd, String varname) {
    int digit_len = integer_len(cd->uid_count);

    String uvar_string = String_init_length(varname.len + digit_len + 1);
    snprintf(uvar_string.cstr, uvar_string.len+1, "%s.%ld", varname.cstr, cd->uid_count);
    cd->uid_count += 1;
    String *uvar_name = (String *)StrInterner_intern(&cd->str_table, uvar_string);
    String_free(&uvar_string);
    return uvar_name;
}

static const String *create_label(CompDriver *cd, LabelKind kind) {
    String label_kind = label_kind_table[kind];
    int digit_len = integer_len(cd->sema.lbl_count);

    // label length: func_name.len + (1) "." + label_kind.len + (1) "." + digit_len
    String label = String_init_length(1 + label_kind.len + digit_len);
    snprintf(label.cstr, label.len+1, "%s.%ld", label_kind.cstr, cd->sema.lbl_count);
    cd->sema.lbl_count += 1;

    const String *canon_label = StrInterner_intern(&cd->str_table, label);
    String_free(&label);
    return canon_label;
}

static const String *create_case_label(CompDriver *cd, const String *switch_lbl, int lbl) {
    int digit_len = integer_len(lbl);

    // label length: switch_lbl.len + (1) "." + digit_len
    String label = String_init_length(1 + switch_lbl->len + digit_len);
    snprintf(label.cstr, label.len+1, "%s.%d", switch_lbl->cstr, lbl);

    const String *canon_label = StrInterner_intern(&cd->str_table, label);
    String_free(&label);
    return canon_label;
}

static const String *create_default_label(CompDriver *cd, const String *switch_lbl) {
    // label length: switch_lbl.len + (2) ".d"
    String label = String_init_length(2 + switch_lbl->len);
    snprintf(label.cstr, label.len+1, "%s.d", switch_lbl->cstr);

    const String *canon_label = StrInterner_intern(&cd->str_table, label);
    String_free(&label);
    return canon_label;
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

static void resolve_declaration(CompDriver *cd, Decl *decl) {
    if (decl == NULL) return;
    //if (decl->kind != BLOCKITEM_DECLARATION) return;
    
    switch (decl->kind) {
    case DECL_INVALID:
        break;
    case DECL_LCL_VAR: {
        if (SymTable_contains(cd->sema.var_table,
                decl->as.loc_var.identifier->cstr, 
                SYMTYPE_MAPPING))
        {
            SymEntry *sym = SymTable_get(cd->sema.var_table,
                decl->as.loc_var.identifier->cstr,
                SYMTYPE_MAPPING);
            if (sym->as.mapping.scope == cd->sema.var_table->scope)
                err_redeclared_variable(&cd->errors, cd->tokenizer.src_path,
                    decl->pos, *decl->as.loc_var.identifier);
        }
        const String *new_name = create_unique_varname(cd, *decl->as.loc_var.identifier);
        SymTable_insert_mapping(cd->sema.var_table, decl->pos,
            (String *)decl->as.loc_var.identifier, new_name);
        decl->as.loc_var.identifier = new_name;
        if (decl->as.loc_var.init != NULL) {
            resolve_expression(cd, decl->as.loc_var.init);
        }
        break;
    }
    }
}

static void resolve_optional_expression(CompDriver *cd, Expr *expr) {
    if (expr == NULL) return;

    resolve_expression(cd, expr);
}

static void resolve_for_init(CompDriver *cd, ForInit *fi) {
    switch (fi->kind) {
    case FOR_INIT_INVALID:
        break;
    case FOR_INIT_NULL:
    case FOR_INIT_EXP:
        resolve_optional_expression(cd, fi->as.exp);
        break;
    case FOR_INIT_DECL:
        resolve_declaration(cd, fi->as.decl);
        break;
    }
}

static void resolve_statement(CompDriver *cd, Stmt *stmt) {
    if (stmt == NULL) return;

    switch (stmt->kind) {
    case STMT_INVALID:
    case STMT_NULL:
    case STMT_BREAK:
    case STMT_CONTINUE:
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

    case STMT_WHILE:
        resolve_expression(cd, stmt->as.while_stmt.cond);
        resolve_statement(cd, stmt->as.while_stmt.body);
        break;

    case STMT_DOWHILE:
        resolve_statement(cd, stmt->as.do_while_stmt.body);
        resolve_expression(cd, stmt->as.do_while_stmt.cond);
        break;

    case STMT_FOR:
        cd->sema.var_table = SymTable_create(cd->sema.var_table, cd->sema.var_table->scope+1);
        resolve_for_init(cd, &stmt->as.for_stmt.init);
        resolve_optional_expression(cd, stmt->as.for_stmt.cond);
        resolve_optional_expression(cd, stmt->as.for_stmt.post);
        resolve_statement(cd, stmt->as.for_stmt.body);
        cd->sema.var_table = SymTable_destroy(cd->sema.var_table);
        break;

    case STMT_SWITCH:
        resolve_expression(cd, stmt->as.switch_stmt.ctrl_expr);
        resolve_statement(cd, stmt->as.switch_stmt.body);
        break;

    case STMT_CASE:
        resolve_statement(cd, stmt->as.case_stmt.stmt);
        break;
        
    case STMT_DEFAULT:
        resolve_statement(cd, stmt->as.default_stmt.stmt);
        break;
    }

}

static void resolve_blockitem_statement(CompDriver *cd, BlockItem *item) {
    if (item == NULL) return;
    if (item->kind != BLOCKITEM_STATEMENT) return;

    switch (item->as.statement->kind) {
    case STMT_INVALID:
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_NULL:
        break;
    case STMT_RET:
        resolve_expression(cd, item->as.statement->as.ret);
        break;
    case STMT_EXPR:
        resolve_expression(cd, item->as.statement->as.expr);
        break;
    case STMT_IF:
    case STMT_LABELED:
    case STMT_GOTO:
    case STMT_COMPOUND:
    case STMT_WHILE:
    case STMT_DOWHILE:
    case STMT_FOR:
    case STMT_SWITCH:
    case STMT_CASE:
    case STMT_DEFAULT:
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
            resolve_declaration(cd, item->as.declaration);
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

typedef enum loopSwitchStatus {
    LSS_NONE,
    LSS_LOOP,
    LSS_SWITCH
} LoopSwitchStatus;

static void
label_loop_statement( CompDriver *cd, Stmt *stmt, LoopSwitchStatus lss, const String *lbl) {
    switch (stmt->kind) {
    case STMT_INVALID:
    case STMT_RET:
    case STMT_EXPR:
    case STMT_NULL:
    case STMT_GOTO:
        break;
    case STMT_IF:
        label_loop_statement(cd, stmt->as.if_stmt.then_stmt, lss, lbl);
        if (stmt->as.if_stmt.else_stmt != NULL)
            label_loop_statement(cd, stmt->as.if_stmt.else_stmt, lss, lbl);
        break;
    case STMT_LABELED:
        label_loop_statement(cd, stmt->as.labeled_stmt.stmt, lss, lbl);
        break;
    case STMT_COMPOUND: {
        Block *comp_stmt = stmt->as.compound_stmt;
        for (size_t blck_idx = 0; blck_idx < comp_stmt->len; blck_idx++) {
            BlockItem *item = &comp_stmt->items[blck_idx];
            if (item->kind == BLOCKITEM_STATEMENT)
                label_loop_statement(cd, item->as.statement, lss, lbl);
        }
        break;
    }
    case STMT_BREAK:
        if (lss == LSS_SWITCH || lss == LSS_NONE) {
            break;
        }
        stmt->as.break_stmt = lbl;
        break;
    case STMT_CONTINUE:
        if (lss == LSS_NONE || lbl == NULL) {
            err_continue_not_in_loop(cd, stmt->pos);
        }
        stmt->as.continue_stmt = lbl;
        break;
    case STMT_WHILE: {
        const String *new_lbl = create_label(cd, LOOP_WHILE);
        label_loop_statement(cd, stmt->as.while_stmt.body, LSS_LOOP, new_lbl);
        stmt->as.while_stmt.lbl = new_lbl;
        break;
    }
    case STMT_DOWHILE: {
        const String *new_lbl = create_label(cd, LOOP_DOWHILE);
        label_loop_statement(cd, stmt->as.do_while_stmt.body, LSS_LOOP, new_lbl);
        stmt->as.do_while_stmt.lbl = new_lbl;
        break;
    }
    case STMT_FOR: {
        const String *new_lbl = create_label(cd, LOOP_FOR);
        label_loop_statement(cd, stmt->as.for_stmt.body, LSS_LOOP, new_lbl);
        stmt->as.for_stmt.lbl = new_lbl;
        break;
    }
    case STMT_SWITCH:
        label_loop_statement(cd, stmt->as.switch_stmt.body, LSS_SWITCH, lbl);
        break;
    case STMT_CASE:
        label_loop_statement(cd, stmt->as.case_stmt.stmt, lss, lbl);
        break;
    case STMT_DEFAULT:
        label_loop_statement(cd, stmt->as.default_stmt.stmt, lss, lbl);
        break;
    }
}

static void label_loop_statements(CompDriver *cd) {
    for (size_t block_idx = 0; block_idx < cd->parser.program->func->block->len; block_idx++) {
        BlockItem *item = &cd->parser.program->func->block->items[block_idx];
        if (item->kind == BLOCKITEM_STATEMENT) {
            label_loop_statement(cd, item->as.statement, LSS_NONE, NULL);
        }
    }
}

static void
label_switch_statement(CompDriver *cd, Stmt *stmt, LoopSwitchStatus lss, const String *lbl) {
    switch (stmt->kind) {
    case STMT_INVALID:
    case STMT_RET:
    case STMT_EXPR:
    case STMT_NULL:
    case STMT_GOTO:
        break;
    case STMT_BREAK:
        if (lss == LSS_NONE && lbl == NULL) {
            err_break_not_in_loop_switch(cd, stmt->pos);
        } else if (lss == LSS_LOOP || stmt->as.break_stmt != NULL) {
            break;
        }
        stmt->as.break_stmt = lbl;
        break;
    case STMT_CONTINUE:
        break;
    case STMT_IF:
        label_switch_statement(cd, stmt->as.if_stmt.then_stmt, lss, lbl);
        if (stmt->as.if_stmt.else_stmt != NULL)
            label_switch_statement(cd, stmt->as.if_stmt.else_stmt, lss, lbl);
        break;
    case STMT_LABELED:
        label_switch_statement(cd, stmt->as.labeled_stmt.stmt, lss, lbl);
        break;
    case STMT_COMPOUND: {
        Block *comp_stmt = stmt->as.compound_stmt;
        for (size_t blck_idx = 0; blck_idx < comp_stmt->len; blck_idx++) {
            BlockItem *item = &comp_stmt->items[blck_idx];
            if (item->kind == BLOCKITEM_STATEMENT)
                label_switch_statement(cd, item->as.statement, lss, lbl);
        }
        break;
    }
    case STMT_WHILE:
        label_switch_statement(cd, stmt->as.while_stmt.body, LSS_LOOP, lbl);
        break;
    case STMT_DOWHILE:
        label_switch_statement(cd, stmt->as.do_while_stmt.body, LSS_LOOP, lbl);
        break;
    case STMT_FOR:
        label_switch_statement(cd, stmt->as.for_stmt.body, LSS_LOOP, lbl);
        break;
    case STMT_SWITCH: {
        cd->sema.lbl_table = SymTable_create(cd->sema.lbl_table, cd->sema.lbl_table->scope+1);
        const String *new_lbl = create_label(cd, SWITCH);
        label_switch_statement(cd, stmt->as.switch_stmt.body, LSS_SWITCH, new_lbl);
        stmt->as.switch_stmt.lbl = new_lbl;
        stmt->as.switch_stmt.cases = cd->sema.lbl_table;
        cd->sema.lbl_table = cd->sema.lbl_table->parent;
        stmt->as.switch_stmt.cases->parent = NULL;
        //cd->sema.lbl_table = SymTable_destroy(cd->sema.lbl_table);
        break;
    }
    case STMT_CASE: {
        if (lss == LSS_NONE || lbl == NULL) {
            err_case_not_in_switch(cd, stmt->pos);
            break;
        } else if (stmt->as.case_stmt.lbl_expr->kind != EXPR_CONSTANT) {
            err_case_lbl_not_constant(cd, stmt->pos);
            break;
        }
        const String *case_lbl =
            create_case_label(cd, lbl, stmt->as.case_stmt.lbl_expr->as.constant);
        if (SymTable_scope_contains(cd->sema.lbl_table, case_lbl->cstr, SYMTYPE_CASE_LABEL)) {
            err_duplicate_case(cd, stmt->pos, stmt->as.case_stmt.lbl_expr->as.constant);
            break;
        }
        SymTable_insert_case_label(cd->sema.lbl_table,
            stmt->pos, case_lbl, stmt->as.case_stmt.lbl_expr->as.constant);
        stmt->as.case_stmt.lbl = case_lbl;
        label_switch_statement(cd, stmt->as.case_stmt.stmt, lss, lbl);
        break;
    }
    case STMT_DEFAULT: {
        if (lss == LSS_NONE || lbl == NULL) {
            err_default_not_in_switch(cd, stmt->pos);
            break;
        }
        const String *case_lbl = create_default_label(cd, lbl);
        if (SymTable_scope_contains(cd->sema.lbl_table, case_lbl->cstr, SYMTYPE_LABEL)) {
            err_duplicate_default(cd, stmt->pos);
            break;
        }
        stmt->as.default_stmt.lbl = case_lbl;
        SymTable_insert_label(cd->sema.lbl_table, stmt->pos, case_lbl, LABEL_DEFINED);
        label_switch_statement(cd, stmt->as.default_stmt.stmt, lss, lbl);
        break;
    }
    }
}

static void label_switch_statements(CompDriver *cd) {
    for (size_t block_idx = 0; block_idx < cd->parser.program->func->block->len; block_idx++) {
        BlockItem *item = &cd->parser.program->func->block->items[block_idx];
        if (item->kind == BLOCKITEM_STATEMENT) {
            label_switch_statement(cd, item->as.statement, LSS_NONE, NULL);
        }
    }
}

void sem_analyze(CompDriver *cd) {
    Function *func = cd->parser.program->func;
    resolve_block(cd, func->block);
    resolve_labels(cd);
    label_loop_statements(cd);
    label_switch_statements(cd);
}
