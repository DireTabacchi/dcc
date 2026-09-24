#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "common.h"
#include "dcc_error.h"
#include "dd_string.h"
#include "comp_driver.h"
#include "sym_table.h"
#include "sema.h"

// Forward Declarations
static void resolve_block(CompDriver *cd, Block *block);

void Sema_init(Sema *sa) {
    sa->lbl_table = SymTable_create(NULL, 0);
    sa->ident_table = SymTable_create(NULL, 0);
    sa->symbol_table = SymTable_create(NULL, 0);
    sa->lbl_count = 0;
}

void Sema_deinit(Sema *sa) {
    SymTable_destroy(sa->symbol_table);
    SymTable_destroy(sa->ident_table);
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
        if (SymTable_contains(cd->sema.ident_table, expr->as.var.name->cstr, SYMTYPE_MAPPING)) {
            SymEntry *canon_name =
                SymTable_get(cd->sema.ident_table, expr->as.var.name->cstr, SYMTYPE_MAPPING);
            expr->as.var.name = canon_name->as.mapping.name;
        } else {
            err_undeclared_variable(&cd->errors, cd->tokenizer.src_path, expr->pos,
                *expr->as.var.name);
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

    case EXPR_FN_CALL: {
        if (SymTable_contains(cd->sema.ident_table, expr->as.fn_call.ident->cstr, SYMTYPE_MAPPING)
        ) {
            SymEntry *sym = SymTable_get(cd->sema.ident_table, expr->as.fn_call.ident->cstr,
                SYMTYPE_MAPPING);
            if (sym == NULL) puts("sym was NULL");
            expr->as.fn_call.ident = sym->as.mapping.name;
            if (expr->as.fn_call.args == NULL) break;
            for (size_t a_idx = 0; a_idx < expr->as.fn_call.args->len; a_idx++) {
                resolve_expression(cd, expr->as.fn_call.args->exprs[a_idx]);
            }
        } else {
            err_undeclared_function(&cd->errors, cd->tokenizer.src_path, expr->pos,
                expr->as.fn_call.ident);
        }
        break;
    }
    }
}

// Resolve a declaration (file scope functions and block scope variables and functions)
static void resolve_declaration(CompDriver *cd, Decl *decl) {
    if (decl == NULL) return;
    
    switch (decl->kind) {
    case DECL_INVALID:
        break;
    case DECL_VARIABLE: {
        if (SymTable_contains(cd->sema.ident_table,
                decl->as.variable.identifier->cstr, 
                SYMTYPE_MAPPING)
        ) {
            SymEntry *sym = SymTable_get(cd->sema.ident_table,
                decl->as.variable.identifier->cstr,
                SYMTYPE_MAPPING);
            if (sym->as.mapping.scope == cd->sema.ident_table->scope) {
                if (!(sym->as.mapping.linkage == LINKAGE_EXTERNAL &&
                    decl->as.variable.sc == SC_EXTERN)
                ) {
                    err_conflicting_var_decl(&cd->errors, cd->tokenizer.src_path, decl->pos,
                        decl->as.variable.identifier, sym->pos);
                    break;
                }
            }
        }

        LinkageKind lk;
        switch (decl->as.variable.sc) {
        case SC_NONE:
        case SC_STATIC:
            lk = LINKAGE_NONE;
            break;
        case SC_EXTERN:
            lk = LINKAGE_EXTERNAL;
            break;
        }

        if (decl->as.variable.sc == SC_EXTERN) {
            SymTable_insert_mapping(cd->sema.ident_table, decl->pos, decl->as.variable.identifier,
                decl->as.variable.identifier, lk);
            break;
        }
        const String *new_name = create_unique_varname(cd, *decl->as.variable.identifier);
        SymTable_insert_mapping(cd->sema.ident_table, decl->pos,
            (String *)decl->as.variable.identifier, new_name, lk);
        decl->as.variable.identifier = new_name;
        if (decl->as.variable.init != NULL) {
            resolve_expression(cd, decl->as.variable.init);
        }
        break;
    }
    case DECL_FUNCTION: {
        if (SymTable_contains(cd->sema.ident_table, decl->as.fn.name->cstr, SYMTYPE_MAPPING)) {
            SymEntry *prev_sym =
                SymTable_get(cd->sema.ident_table, decl->as.fn.name->cstr, SYMTYPE_MAPPING);
            if (prev_sym->as.mapping.scope == cd->sema.ident_table->scope &&
                prev_sym->as.mapping.linkage == LINKAGE_INTERNAL
            ) {
                err_var_redeclared_as_fn(&cd->errors, cd->tokenizer.src_path, decl->pos,
                    decl->as.fn.name);
            }
        }
        SymTable_insert_mapping(cd->sema.ident_table, decl->pos, decl->as.fn.name, decl->as.fn.name,
            LINKAGE_EXTERNAL);

        // Resolve param list
        cd->sema.ident_table = SymTable_create(cd->sema.ident_table, cd->sema.ident_table->scope+1);
        for (size_t p_idx = 0; p_idx < decl->as.fn.params->len; p_idx++) {
            const String *param = decl->as.fn.params->params[p_idx];
            if (SymTable_contains(cd->sema.ident_table, param->cstr, SYMTYPE_MAPPING)) {
                SymEntry *sym = SymTable_get(cd->sema.ident_table, param->cstr, SYMTYPE_MAPPING);
                if (sym->as.mapping.scope == cd->sema.ident_table->scope) {
                    err_redefined_param(&cd->errors, cd->tokenizer.src_path, decl->pos, sym->key,
                        decl->as.fn.name);
                }
            }
            const String *new_name = create_unique_varname(cd, *param);
            SymTable_insert_mapping(cd->sema.ident_table, decl->pos, param, new_name,
                LINKAGE_NONE);
            decl->as.fn.params->params[p_idx] = new_name;
        }

        if (decl->as.fn.body != NULL) {
            resolve_block(cd, decl->as.fn.body);
        }
        //SymTable_print(cd->sema.ident_table);
        cd->sema.ident_table = SymTable_destroy(cd->sema.ident_table);
        break;
    }
    }
}

// Resolve a file scope declaration
static void resolve_file_declaration(CompDriver *cd, Decl *decl) {
    if (decl == NULL) return;
    //if (decl->kind != BLOCKITEM_DECLARATION) return;
    
    switch (decl->kind) {
    case DECL_INVALID:
        break;
    case DECL_VARIABLE: {
        SymTable_insert_mapping(cd->sema.ident_table, decl->pos,
            (String *)decl->as.variable.identifier, (String *)decl->as.variable.identifier,
            LINKAGE_EXTERNAL);
        break;
    }
    case DECL_FUNCTION: {
        resolve_declaration(cd, decl);
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
        cd->sema.ident_table = SymTable_create(cd->sema.ident_table, cd->sema.ident_table->scope+1);
        resolve_block(cd, stmt->as.compound_stmt);
        cd->sema.ident_table = SymTable_destroy(cd->sema.ident_table);
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
        cd->sema.ident_table = SymTable_create(cd->sema.ident_table, cd->sema.ident_table->scope+1);
        resolve_for_init(cd, &stmt->as.for_stmt.init);
        resolve_optional_expression(cd, stmt->as.for_stmt.cond);
        resolve_optional_expression(cd, stmt->as.for_stmt.post);
        resolve_statement(cd, stmt->as.for_stmt.body);
        cd->sema.ident_table = SymTable_destroy(cd->sema.ident_table);
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
            //puts("Found a lbl");
            SymEntry lbl = lbl_table->syms[lbl_idx];
            if (lbl.as.lbl.status == LABEL_REFERENCED) {
                //puts("lbl is in error");
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
label_loop_statement(CompDriver *cd, Stmt *stmt, LoopSwitchStatus lss, const String *lbl) {
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
    for (size_t d_idx = 0; d_idx < cd->parser.ast_tu->decls.len; d_idx++) {
        Decl *decl = cd->parser.ast_tu->decls.decls[d_idx];
        if (decl->kind == DECL_FUNCTION && decl->as.fn.body != NULL) {
            for (size_t block_idx = 0; block_idx < decl->as.fn.body->len; block_idx++) {
                BlockItem *item = &decl->as.fn.body->items[block_idx];
                if (item->kind == BLOCKITEM_STATEMENT) {
                    label_loop_statement(cd, item->as.statement, LSS_NONE, NULL);
                }
            }
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
    for (size_t d_idx = 0; d_idx < cd->parser.ast_tu->decls.len; d_idx++) {
        Decl *decl = cd->parser.ast_tu->decls.decls[d_idx];
        if (decl->kind == DECL_FUNCTION && decl->as.fn.body != NULL) {
            for (size_t block_idx = 0; block_idx < decl->as.fn.body->len; block_idx++) {
                BlockItem *item = &decl->as.fn.body->items[block_idx];
                if (item->kind == BLOCKITEM_STATEMENT) {
                    label_switch_statement(cd, item->as.statement, LSS_NONE, NULL);
                }
            }
        }
    }
}

// typecheck_*

static void typecheck_block(CompDriver *cd, Block *block);

static void typecheck_expression(CompDriver *cd, Expr *expr) {
    if (expr == NULL) return;

    switch (expr->kind) {
    case EXPR_INVALID:
    case EXPR_CONSTANT:
        break;
    case EXPR_FN_CALL: {
        SymEntry *sym =
            SymTable_get(cd->sema.symbol_table, expr->as.fn_call.ident->cstr, SYMTYPE_SYMBOL);
        if (sym == NULL) break;
        if (sym->as.sym.type == SYMBOL_TYPE_AUTO || sym->as.sym.type == SYMBOL_TYPE_STATIC) {
            err_object_not_function(&cd->errors, cd->tokenizer.src_path, expr->pos,
                sym->as.sym.origin_name, sym->pos);
            break;
        }
        if ((expr->as.fn_call.args == NULL && sym->as.sym.as.fn.type.arity > 0) || 
            (expr->as.fn_call.args != NULL &&
                sym->as.sym.as.fn.type.arity > expr->as.fn_call.args->len)
        ) {
            err_too_few_args(&cd->errors, cd->tokenizer.src_path, expr->pos, expr->as.fn_call.ident,
                sym->as.sym.as.fn.type.arity, expr->as.fn_call.args->len);
        } else if (expr->as.fn_call.args != NULL &&
            sym->as.sym.as.fn.type.arity < expr->as.fn_call.args->len) {
            err_too_many_args(&cd->errors, cd->tokenizer.src_path, expr->pos, expr->as.fn_call.ident,
                sym->as.sym.as.fn.type.arity, expr->as.fn_call.args->len);
        }
        if (expr->as.fn_call.args != NULL)
            for (size_t a_idx = 0; a_idx < expr->as.fn_call.args->len; a_idx++) {
                typecheck_expression(cd, expr->as.fn_call.args->exprs[a_idx]);
            }
        break;
    }
    case EXPR_VAR: {
        SymEntry *sym =
            SymTable_get(cd->sema.symbol_table, expr->as.var.name->cstr, SYMTYPE_SYMBOL);
        if (sym == NULL) break;
        if (sym->as.sym.type != SYMBOL_TYPE_AUTO && sym->as.sym.type != SYMBOL_TYPE_STATIC) {
            err_object_not_variable(&cd->errors, cd->tokenizer.src_path, expr->pos,
                sym->as.sym.origin_name);
        }
        break;
    }
    case EXPR_UNARY:
        typecheck_expression(cd, expr->as.unary.expr);
        break;
    case EXPR_BINARY:
        typecheck_expression(cd, expr->as.binary.left);
        typecheck_expression(cd, expr->as.binary.right);
        break;
    case EXPR_ASSIGN: {
        if (expr->as.assign.lhs->kind == EXPR_VAR) {
            Expr *lhs_var = expr->as.assign.lhs;
            if (SymTable_contains(cd->sema.symbol_table,
                    lhs_var->as.var.name->cstr, SYMTYPE_SYMBOL)
            ) {
                SymEntry *lhs_sym =
                    SymTable_get(cd->sema.symbol_table, lhs_var->as.var.name->cstr, SYMTYPE_SYMBOL);
                if (lhs_sym->as.sym.type == SYMBOL_TYPE_FN) {
                    err_assign_to_fn(&cd->errors, cd->tokenizer.src_path, expr->pos,
                        lhs_sym->as.sym.origin_name);

                } else if (lhs_sym->as.sym.type == SYMBOL_TYPE_AUTO ||
                    lhs_sym->as.sym.type == SYMBOL_TYPE_STATIC
                ) {
                    if (expr->as.assign.rhs->kind == EXPR_VAR) {
                        Expr *rhs_var = expr->as.assign.rhs;
                        if (SymTable_contains(cd->sema.symbol_table, rhs_var->as.var.name->cstr,
                                SYMTYPE_SYMBOL)
                        ) {
                            SymEntry *rhs_sym =
                                SymTable_get(cd->sema.symbol_table, rhs_var->as.var.name->cstr,
                                    SYMTYPE_SYMBOL);
                            if (rhs_sym->as.sym.type == SYMBOL_TYPE_FN) {
                                err_assign_fn_to_var(&cd->errors, cd->tokenizer.src_path, expr->pos,
                                    rhs_sym->as.sym.origin_name);
                            }
                        }
                    }
                }
            }
        }
        break;
    }
    case EXPR_TERNARY:
        typecheck_expression(cd, expr->as.ternary.cond);
        typecheck_expression(cd, expr->as.ternary.then_expr);
        typecheck_expression(cd, expr->as.ternary.else_expr);
        break;
    }

}

static void typecheck_local_var_declaration(CompDriver *cd, Decl *var_decl) {
    if (var_decl->as.variable.sc == SC_EXTERN) {
        if (var_decl->as.variable.init != NULL) {
            err_extern_var_initializer(&cd->errors, cd->tokenizer.src_path, var_decl->pos,
                var_decl->as.variable.origin_name);
            return;
        }
        if (SymTable_contains(cd->sema.symbol_table, var_decl->as.variable.identifier->cstr,
                SYMTYPE_SYMBOL)
        ) {
            SymEntry *old_decl = SymTable_get(cd->sema.symbol_table,
                var_decl->as.variable.identifier->cstr, SYMTYPE_SYMBOL);
            if (old_decl->as.sym.type == SYMBOL_TYPE_FN) {
                err_fn_redeclared_as_var(&cd->errors, cd->tokenizer.src_path, var_decl->pos,
                    var_decl->as.variable.identifier, old_decl->pos);
                return;
            }
        } else {
            StaticAttr attr = (StaticAttr){
                .linkage = LINKAGE_EXTERNAL,
                .initial_value = (InitVal){
                    .kind = IV_NO_INITIALIZER
                }
            };
            SymTable_insert_static_var_symbol(cd->sema.symbol_table, var_decl->pos,
                var_decl->as.variable.identifier, var_decl->as.variable.origin_name, attr);
        }
    } else if (var_decl->as.variable.sc == SC_STATIC) {
        InitVal init = {0};
        if (var_decl->as.variable.init == NULL) {
            init = (InitVal){ .kind = IV_INITIAL, .as.initial = 0 };
        } else if (var_decl->as.variable.init->kind == EXPR_CONSTANT) {
            init = (InitVal){
                .kind = IV_INITIAL,
                .as.initial = var_decl->as.variable.init->as.constant
            };
        } else {
            err_lcl_var_stc_non_const_init(&cd->errors, cd->tokenizer.src_path, var_decl->pos,
                var_decl->as.variable.origin_name);
            return;
        }
        StaticAttr attr = (StaticAttr){ .linkage = LINKAGE_NONE, .initial_value = init};
        SymTable_insert_static_var_symbol(cd->sema.symbol_table, var_decl->pos,
            var_decl->as.variable.identifier, var_decl->as.variable.identifier, attr);
    } else {
        SymTable_insert_auto_var_symbol(cd->sema.symbol_table, var_decl->pos,
            var_decl->as.variable.identifier, var_decl->as.variable.origin_name);
        if (var_decl->as.variable.init != NULL) {
            typecheck_expression(cd, var_decl->as.variable.init);
        }
    }
}

static void typecheck_fn_declaration(CompDriver *cd, Decl *fn_decl) {
    FnType fn_type = (FnType){ .arity = fn_decl->as.fn.params->len };
    bool has_body = fn_decl->as.fn.body != NULL;
    bool prev_defined = false;
    LinkageKind lk = fn_decl->as.fn.sc == SC_STATIC ? LINKAGE_INTERNAL : LINKAGE_EXTERNAL;

    if (SymTable_contains(cd->sema.symbol_table, fn_decl->as.fn.name->cstr, SYMTYPE_SYMBOL)) {
        SymEntry *prev_decl = SymTable_get(cd->sema.symbol_table, fn_decl->as.fn.name->cstr,
            SYMTYPE_SYMBOL);
        if (prev_decl->as.sym.type != SYMBOL_TYPE_FN ||
            (prev_decl->as.sym.type == SYMBOL_TYPE_FN &&
             prev_decl->as.sym.as.fn.type.arity != fn_type.arity)
        ) {
            err_incompatible_fn_types(&cd->errors, cd->tokenizer.src_path, fn_decl->pos,
                fn_decl->as.fn.name, prev_decl->pos);
            return;
        }
        prev_defined = prev_decl->as.sym.as.fn.attr.defined;
        if (prev_defined && has_body) {
            err_fn_redefinition(&cd->errors, cd->tokenizer.src_path, fn_decl->pos,
                fn_decl->as.fn.name, prev_decl->pos);
            return;
        }

        if (prev_decl->as.sym.as.fn.attr.linkage == LINKAGE_EXTERNAL &&
            fn_decl->as.fn.sc == SC_STATIC
        ) {
            err_static_fn_non_static(&cd->errors, cd->tokenizer.src_path, fn_decl->pos,
                fn_decl->as.fn.name, prev_decl->pos);
            return;
        }

        lk = prev_decl->as.sym.as.fn.attr.linkage;
    }

    FnAttr attrs = (FnAttr){ .defined = prev_defined || has_body, .linkage = lk };

    SymTable_insert_fn_symbol(cd->sema.symbol_table, fn_decl->pos, fn_decl->as.fn.name,
        fn_decl->as.fn.name, fn_type.arity, attrs);

    if (has_body) {
        for (size_t p_idx = 0; p_idx < fn_decl->as.fn.params->len; p_idx++) {
            SymTable_insert_auto_var_symbol(cd->sema.symbol_table, fn_decl->pos,
                fn_decl->as.fn.params->params[p_idx], fn_decl->as.fn.params->params[p_idx]);
        }
        typecheck_block(cd, fn_decl->as.fn.body);
    }
}

static void typecheck_file_var_decl(CompDriver *cd, Decl *var_decl) {
    if (var_decl == NULL) return; // error?

    InitVal init_val = {0};
    if (var_decl->as.variable.init != NULL) {
        Expr *init = var_decl->as.variable.init;
        if (init->kind == EXPR_CONSTANT) {
            init_val.kind = IV_INITIAL;
            init_val.as.initial = init->as.constant;
        } else {
            err_file_var_non_const_init(&cd->errors, cd->tokenizer.src_path, var_decl->pos,
                var_decl->as.variable.identifier);
            return;
        }
    } else {
        if (var_decl->as.variable.sc == SC_EXTERN) {
            init_val.kind = IV_NO_INITIALIZER;
        } else {
            init_val.kind = IV_TENTATIVE;
        }
    }

    LinkageKind lk = var_decl->as.variable.sc == SC_STATIC ? LINKAGE_INTERNAL : LINKAGE_EXTERNAL;

    if (SymTable_contains(cd->sema.symbol_table, var_decl->as.variable.identifier->cstr,
            SYMTYPE_SYMBOL)
    ) {
        SymEntry *old_decl = SymTable_get(cd->sema.symbol_table,
            var_decl->as.variable.identifier->cstr, SYMTYPE_SYMBOL);
        if (old_decl->as.sym.type == SYMBOL_TYPE_FN) {
            err_fn_redeclared_as_var(&cd->errors, cd->tokenizer.src_path, var_decl->pos,
                var_decl->as.variable.origin_name, old_decl->pos);
            return;
        }

        if (var_decl->as.variable.sc == SC_EXTERN) {
            lk = old_decl->as.sym.as.static_var.attr.linkage;
        } else if (old_decl->as.sym.as.static_var.attr.linkage != lk) {
            err_conflicting_var_linkage(&cd->errors, cd->tokenizer.src_path, var_decl->pos,
                var_decl->as.variable.identifier, old_decl->pos);
            return;
        }

        if (old_decl->as.sym.as.static_var.attr.initial_value.kind == IV_INITIAL) {
            if (init_val.kind == IV_INITIAL) {
                err_conflicting_file_defs(&cd->errors, cd->tokenizer.src_path, var_decl->pos, 
                    var_decl->as.variable.identifier, old_decl->pos);
                return;
            } else {
                init_val = old_decl->as.sym.as.static_var.attr.initial_value;
            }
        } else if (init_val.kind != IV_INITIAL &&
            old_decl->as.sym.as.static_var.attr.initial_value.kind == IV_TENTATIVE
        ) {
            init_val.kind = IV_TENTATIVE;
        }
    }

    StaticAttr attr = (StaticAttr){ .linkage = lk, .initial_value = init_val };
    SymTable_insert_static_var_symbol(cd->sema.symbol_table, var_decl->pos,
        var_decl->as.variable.identifier, var_decl->as.variable.identifier, attr);
}

static void typecheck_block_declaration(CompDriver *cd, Decl *decl) {
    if (decl == NULL) return;

    switch (decl->kind) {
    case DECL_INVALID:
        break;
    case DECL_VARIABLE:
        typecheck_local_var_declaration(cd, decl);
        break;
    case DECL_FUNCTION:
        if (decl->as.fn.body != NULL) {
            err_nested_fn_definition(&cd->errors, cd->tokenizer.src_path, decl->pos);
            break;
        }
        if (decl->as.fn.sc == SC_STATIC) {
            err_block_static_fn_decl(&cd->errors, cd->tokenizer.src_path, decl->pos,
                decl->as.fn.name);
            break;
        }
        typecheck_fn_declaration(cd, decl);
        break;
    }
}

static void typecheck_statement(CompDriver *cd, Stmt *stmt) {
    if (stmt == NULL) return;

    switch (stmt->kind) {
    case STMT_INVALID:
        break;
    case STMT_RET:
        typecheck_expression(cd, stmt->as.ret);
        break;
    case STMT_EXPR:
        typecheck_expression(cd, stmt->as.expr);
        break;
    case STMT_NULL:
    case STMT_GOTO:
    case STMT_BREAK:
    case STMT_CONTINUE:
        /* Nothing to check */
        break;
    case STMT_IF:
        typecheck_expression(cd, stmt->as.if_stmt.cond);
        typecheck_statement(cd, stmt->as.if_stmt.then_stmt);
        if (stmt->as.if_stmt.else_stmt != NULL) typecheck_statement(cd, stmt->as.if_stmt.else_stmt);
        break;
    case STMT_LABELED:
        typecheck_statement(cd, stmt->as.labeled_stmt.stmt);
        break;
    case STMT_COMPOUND:
        typecheck_block(cd, stmt->as.compound_stmt);
        break;
    case STMT_WHILE:
        typecheck_expression(cd, stmt->as.while_stmt.cond);
        typecheck_statement(cd, stmt->as.while_stmt.body);
        break;
    case STMT_DOWHILE:
        typecheck_expression(cd, stmt->as.do_while_stmt.cond);
        typecheck_statement(cd, stmt->as.do_while_stmt.body);
        break;
    case STMT_FOR:
        if (stmt->as.for_stmt.init.kind == FOR_INIT_DECL) {
            if (stmt->as.for_stmt.init.as.decl->kind == DECL_VARIABLE &&
                stmt->as.for_stmt.init.as.decl->as.variable.sc != SC_NONE
            ) {
                err_storage_in_for_init(&cd->errors, cd->tokenizer.src_path,
                    stmt->as.for_stmt.init.as.decl->pos,
                    stmt->as.for_stmt.init.as.decl->as.variable.origin_name);
            } else if (stmt->as.for_stmt.init.as.decl->kind == DECL_FUNCTION) {
                puts("ERROR: function cannot be declared in the initializer section of a for loop");
            } else {
                typecheck_local_var_declaration(cd, stmt->as.for_stmt.init.as.decl);
            }
        } else if (stmt->as.for_stmt.init.kind == FOR_INIT_EXP) {
            typecheck_expression(cd, stmt->as.for_stmt.init.as.exp);
        }
        typecheck_expression(cd, stmt->as.for_stmt.cond);
        typecheck_expression(cd, stmt->as.for_stmt.post);
        typecheck_statement(cd, stmt->as.for_stmt.body);
        break;
    case STMT_SWITCH:
        typecheck_expression(cd, stmt->as.switch_stmt.ctrl_expr);
        typecheck_statement(cd, stmt->as.switch_stmt.body);
        break;
    case STMT_CASE:
        typecheck_expression(cd, stmt->as.case_stmt.lbl_expr);
        typecheck_statement(cd, stmt->as.case_stmt.stmt);
        break;
    case STMT_DEFAULT:
        typecheck_statement(cd, stmt->as.default_stmt.stmt);
        break;
    }
}

static void typecheck_block(CompDriver *cd, Block *block) {
    if (block == NULL) return;
    if (block->len <= 0) return;

    for (size_t b_idx = 0; b_idx < block->len; b_idx++) {
        BlockItem *item = &block->items[b_idx];
        switch (item->kind) {
        case BLOCKITEM_INVALID:
            break;
        case BLOCKITEM_DECLARATION:
            typecheck_block_declaration(cd, item->as.declaration);
            break;
        case BLOCKITEM_STATEMENT:
            typecheck_statement(cd, item->as.statement);
            break;
        }
    }
}

static void typecheck_tu(CompDriver *cd) {
    for (size_t d_idx = 0; d_idx < cd->parser.ast_tu->decls.len; d_idx++) {
        Decl *decl = cd->parser.ast_tu->decls.decls[d_idx];
        switch (decl->kind) {
        case DECL_INVALID:
            break;
        case DECL_VARIABLE:
            typecheck_file_var_decl(cd, decl);
            break;
        case DECL_FUNCTION:
            typecheck_fn_declaration(cd, decl);
            break;
        }
    }

    //printf("After typechecking...\n");
    //SymTable_print(cd->sema.symbol_table);
}

void sem_analyze(CompDriver *cd) {
    for (size_t d_idx = 0; d_idx < cd->parser.ast_tu->decls.len; d_idx++) {
        cd->sema.lbl_table = SymTable_create(cd->sema.lbl_table, 0);
        resolve_file_declaration(cd, cd->parser.ast_tu->decls.decls[d_idx]);
        resolve_labels(cd);
        cd->sema.lbl_table = SymTable_destroy(cd->sema.lbl_table);
    }
    label_loop_statements(cd);
    label_switch_statements(cd);
    //Parser_print_ast(&cd->parser);
    typecheck_tu(cd);
    //SymTable_print(cd->sema.symbol_table);
}
