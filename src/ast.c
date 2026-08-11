#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include "dd_string.h"

#include "ast.h"
#include "sym_table.h"

// How many spaces to include per indent level. Used in AstNode_print
#define SPACES_PER_INDENT 2

Block *Block_create() {
    Block *block = (Block *)malloc(sizeof(Block));
    block->cap = 2;
    block->len = 0;
    block->items = (BlockItem *)calloc(block->cap, sizeof(BlockItem));
    return block;
}

void Block_destroy(Block *block) {
    if (block == NULL) return;
    if (block->items == NULL) goto freeblock;

    for (size_t b_idx = 0; b_idx < block->len; b_idx++) {
        switch (block->items[b_idx].kind) {
        case BLOCKITEM_INVALID:
            break;
        case BLOCKITEM_STATEMENT:
            Stmt_destroy(block->items[b_idx].as.statement);
            break;
        case BLOCKITEM_DECLARATION:
            Decl_destroy(block->items[b_idx].as.declaration);
            break;
        }
    }

    free(block->items);
freeblock:
    free(block);
    block = NULL;
}

void Block_append(Block *block, BlockItem item) {
    if (block == NULL) return;
    if (block->items == NULL) return;

    if (block->len == block->cap) {
        size_t old_cap = block->cap;
        size_t new_cap = old_cap / 2 + old_cap;
        BlockItem *new_items = calloc(new_cap, sizeof(BlockItem));
        memcpy(new_items, block->items, old_cap*sizeof(BlockItem));
        free(block->items);
        block->items = new_items;
        block->cap = new_cap;
    }

    memcpy(&block->items[block->len], &item, sizeof(BlockItem));
    block->len += 1;
}

Expr *Expr_create(ExpressionKind kind) {
    Expr *e = malloc(sizeof(Expr));
    e->kind = kind;
    return e;
}

void Expr_destroy(Expr *expr) {
    if (expr == NULL) return;

    switch (expr->kind) {
    case EXPR_INVALID:
        free(expr);
        break;
    case EXPR_CONSTANT:
        free(expr);
        break;
    case EXPR_UNARY:
        Expr_destroy(expr->as.unary.expr);
        free(expr);
        break;
    case EXPR_BINARY:
        Expr_destroy(expr->as.binary.left);
        Expr_destroy(expr->as.binary.right);
        free(expr);
        break;
    case EXPR_VAR:
        //String_free(&expr->as.var);
        free(expr);
        break;
    case EXPR_ASSIGN:
        Expr_destroy(expr->as.assign.lhs);
        Expr_destroy(expr->as.assign.rhs);
        free(expr);
        break;

    case EXPR_TERNARY:
        Expr_destroy(expr->as.ternary.cond);
        Expr_destroy(expr->as.ternary.then_expr);
        Expr_destroy(expr->as.ternary.else_expr);
        free(expr);
        break;
    }
}

Stmt *Stmt_create(StatementKind kind) {
    Stmt *s = (Stmt *)malloc(sizeof(Stmt));
    s->kind = kind;
    return s;
}

void Stmt_destroy(Stmt *stmt) {
    if (stmt == NULL) return;

    switch (stmt->kind) {
    case STMT_INVALID:
    case STMT_NULL:
        free(stmt);
        break;
    case STMT_RET:
        Expr_destroy(stmt->as.ret);
        free(stmt);
        break;
    case STMT_EXPR:
        Expr_destroy(stmt->as.expr);
        free(stmt);
        break;
    case STMT_IF:
        Expr_destroy(stmt->as.if_stmt.cond);
        Stmt_destroy(stmt->as.if_stmt.then_stmt);
        Stmt_destroy(stmt->as.if_stmt.else_stmt);
        free(stmt);
        break;
    case STMT_LABELED:
        Stmt_destroy(stmt->as.labeled_stmt.stmt);
        free(stmt);
        break;
    case STMT_GOTO:
        free(stmt);
        break;
    case STMT_COMPOUND:
        Block_destroy(stmt->as.compound_stmt);
        free(stmt);
        break;
    case STMT_BREAK:
    case STMT_CONTINUE:
        free(stmt);
        break;
    case STMT_WHILE:
        Expr_destroy(stmt->as.while_stmt.cond);
        Stmt_destroy(stmt->as.while_stmt.body);
        free(stmt);
        break;
    case STMT_DOWHILE:
        Stmt_destroy(stmt->as.do_while_stmt.body);
        Expr_destroy(stmt->as.do_while_stmt.cond);
        free(stmt);
        break;
    case STMT_FOR:
        if (stmt->as.for_stmt.init.kind == FOR_INIT_DECL) {
            Decl_destroy(stmt->as.for_stmt.init.as.decl);
        }
        else if (stmt->as.for_stmt.init.kind == FOR_INIT_EXP) {
            Expr_destroy(stmt->as.for_stmt.init.as.exp);
        }

        Expr_destroy(stmt->as.for_stmt.cond);
        Expr_destroy(stmt->as.for_stmt.post);
        Stmt_destroy(stmt->as.for_stmt.body);
        free(stmt);
        break;

    case STMT_SWITCH:
        Stmt_destroy(stmt->as.switch_stmt.body);
        Expr_destroy(stmt->as.switch_stmt.ctrl_expr);
        SymTable_destroy(stmt->as.switch_stmt.cases);
        free(stmt);
        break;

    case STMT_CASE:
        Expr_destroy(stmt->as.case_stmt.lbl_expr);
        Stmt_destroy(stmt->as.case_stmt.stmt);
        free(stmt);
        break;

    case STMT_DEFAULT:
        Stmt_destroy(stmt->as.default_stmt.stmt);
        free(stmt);
        break;
    }
}

Decl *Decl_create(DeclarationKind kind) {
    Decl *d = malloc(sizeof(Decl));
    d->kind = kind;
    return d;
}

void Decl_destroy(Decl *decl) {
    if (decl == NULL) return;

    switch (decl->kind) {
        case DECL_INVALID:
            free(decl);
            break;
        case DECL_LCL_VAR:
            Expr_destroy(decl->as.loc_var.init);
            free(decl);
            break;
    }
}

Function *Function_create() {
    Function *func = (Function *)malloc(sizeof(Function));
    func->name = NULL;
    func->block = NULL;
    //func->block = Block_create();
    return func;
}

void Function_destroy(Function *func) {
    if (func == NULL) return;
    Block_destroy(func->block);
    free(func);
}

// TODO: Function list?
void Program_init(AstProgram *prog) {
    prog->func = NULL;
}

void Program_deinit(AstProgram *prog) {
    Function_destroy(prog->func);
}

/*
    AST printing functions
*/

char *binary_op_names[19] = {
    (char *)"UNKNOWN",
    (char *)"Add",
    (char *)"Subtract",
    (char *)"Multiply",
    (char *)"Divide",
    (char *)"Remainder",

    (char *)"(B)AND",
    (char *)"(B)OR",
    (char *)"(B)XOR",
    (char *)"Left Shift",
    (char *)"Right Shift",

    (char *)"(L)AND",
    (char *)"(L)OR",
    (char *)"Equal",
    (char *)"Not Equal",
    (char *)"Lt",
    (char *)"Lte",
    (char *)"Gt",
    (char *)"Gte",
};

char *assign_op_names[12] = {
    (char *)"UNKNOWN",
    (char *)"Assign Simple",
    (char *)"Assign Sum",
    (char *)"Assign Difference",
    (char *)"Assign Product",
    (char *)"Assign Quotient",
    (char *)"Assign Remainder",

    (char *)"Assign Bitwise AND",
    (char *)"Assign Bitwise OR",
    (char *)"Assign Bitwise XOR",
    (char *)"Assign Bitwise Left Shift",
    (char *)"Assign Bitwise Right Shift"
};

static void Block_print(Block *block, int indent_lvl);

void Expr_unary_print(Expr *expr, int indent_lvl) {
    if (expr == NULL) return;

    int spaces = indent_lvl * SPACES_PER_INDENT;

    switch (expr->as.unary.op) {
    case UNARY_INVALID:
        printf("%2$*1$s=%3$s\n%5$*4$s=(\n", spaces+2, "op", "Invalid", spaces+3, "exp");
        break;
    case UNARY_NEGATE:
        printf("%2$*1$s=%3$s\n%5$*4$s=(\n", spaces+2, "op", "Negate", spaces+3, "exp");
        break;
    case UNARY_COMPLEMENT:
        printf("%2$*1$s=%3$s\n%5$*4$s=(\n", spaces+2, "op", "Complement", spaces+3, "exp");
        break;
    case UNARY_NOT:
        printf("%2$*1$s=%3$s\n%5$*4$s=(\n", spaces+2, "op", "Not", spaces+3, "exp");
        break;
    case UNARY_PRE_INCR:
        printf("%2$*1$s=%3$s\n%5$*4$s=(\n", spaces+2, "op", "Prefix Increment", spaces+3, "exp");
        break;
    case UNARY_PRE_DECR:
        printf("%2$*1$s=%3$s\n%5$*4$s=(\n", spaces+2, "op", "Prefix Decrement", spaces+3, "exp");
        break;
    case UNARY_POST_INCR:
        printf("%2$*1$s=%3$s\n%5$*4$s=(\n", spaces+2, "op", "Postfix Increment", spaces+3, "exp");
        break;
    case UNARY_POST_DECR:
        printf("%2$*1$s=%3$s\n%5$*4$s=(\n", spaces+2, "op", "Postfix Decrement", spaces+3, "exp");
        break;
    }
    Expr_print(expr->as.unary.expr, indent_lvl+1);
    printf("%2$*1$c\n", spaces+1, ')');
}

void Expr_print(Expr *expr, int indent_lvl) {
    if (expr == NULL) return;

    int spaces = indent_lvl * SPACES_PER_INDENT;

    switch(expr->kind) {
    case EXPR_INVALID:
        printf("%2$*1$s\n", spaces+18, "INVALID_EXPRESSION");
        break;

    case EXPR_CONSTANT:
        printf("%2$*1$s(%3$d)\n", spaces+8, "Constant", expr->as.constant);
        break;

    case EXPR_VAR:
        printf("%2$*1$s(%3$s)\n", spaces+3, "Var", expr->as.var->cstr);
        break;

    case EXPR_ASSIGN:
        printf("%2$*1$s\n", spaces+7, "Assign(");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s=%3$s\n", spaces+2, "op", assign_op_names[expr->as.assign.op]);
        printf("%2$*1$s=\n", spaces+3, "lhs");
        Expr_print(expr->as.assign.lhs, indent_lvl+1);
        printf("%2$*1$s=\n", spaces+3, "rhs");
        Expr_print(expr->as.assign.rhs, indent_lvl+1);
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case EXPR_UNARY:
        printf("%2$*1$s\n", spaces+6, "Unary(");
        Expr_unary_print(expr, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case EXPR_BINARY:
        printf("%2$*1$s\n", spaces+7, "Binary(");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s=%3$s\n%5$*4$s=(\n",
            spaces+2, "op",
            binary_op_names[expr->as.binary.op],
            spaces+4,
            "left");
        Expr_print(expr->as.binary.left, indent_lvl+1);
        printf("%2$*1$c\n%4$*3$s=(\n", spaces+1, ')', spaces+5, "right");
        Expr_print(expr->as.binary.right, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case EXPR_TERNARY:
        printf("%2$*1$s(\n", spaces+7, "Ternary");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s=(\n", spaces+4, "cond");
        Expr_print(expr->as.ternary.cond, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        printf("%2$*1$s=(\n", spaces+4, "then");
        Expr_print(expr->as.ternary.then_expr, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        printf("%2$*1$s=(\n", spaces+4, "else");
        Expr_print(expr->as.ternary.else_expr, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    }
}

void Stmt_print(Stmt *stmt, int indent_lvl) {
    if (stmt == NULL) return;

    int spaces = indent_lvl * SPACES_PER_INDENT;

    switch (stmt->kind) {
    case STMT_INVALID:
        printf("%2$*1$s\n", spaces+17, "INVALID_STATEMENT");
        break;
    case STMT_RET:
        printf("%2$*1$s(\n", spaces+6, "Return");
        Expr_print(stmt->as.ret, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    case STMT_EXPR:
        printf("%2$*1$s(\n", spaces+20, "Expression Statement");
        Expr_print(stmt->as.expr, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case STMT_IF:
        printf("%2$*1$s(\n", spaces+12, "If Statement");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s=(\n", spaces+4, "cond");
        Expr_print(stmt->as.if_stmt.cond, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        printf("%2$*1$s=(\n", spaces+4, "then");
        Stmt_print(stmt->as.if_stmt.then_stmt, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        if (stmt->as.if_stmt.else_stmt != NULL) {
            printf("%2$*1$s=(\n", spaces+4, "else");
            Stmt_print(stmt->as.if_stmt.else_stmt, indent_lvl+1);
            printf("%2$*1$c\n", spaces+1, ')');
        }
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case STMT_LABELED:
        printf("%2$*1$s(\n", spaces+17, "Labeled Statement");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s=%3$s\n", spaces+3, "lbl", stmt->as.labeled_stmt.lbl->cstr);
        printf("%2$*1$s=(\n", spaces+4, "stmt");
        Stmt_print(stmt->as.labeled_stmt.stmt, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case STMT_GOTO:
        printf("%2$*1$s(\n", spaces+14, "Goto Statement");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s=%3$s\n", spaces+3, "lbl", stmt->as.goto_stmt->cstr);
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case STMT_COMPOUND:
        printf("%2$*1$s(\n", spaces+18, "Compound Statement");
        Block_print(stmt->as.compound_stmt, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case STMT_BREAK:
        printf("%2$*1$s", spaces+15, "Break Statement");
        if (stmt->as.break_stmt != NULL)
            printf("(%s)\n", stmt->as.break_stmt->cstr);
        else
            printf("\n");
        break;

    case STMT_CONTINUE:
        printf("%2$*1$s", spaces+18, "Continue Statement");
        if (stmt->as.continue_stmt != NULL)
            printf("(%s)\n", stmt->as.continue_stmt->cstr);
        else
            printf("\n");
        break;

    case STMT_WHILE:
        printf("%2$*1$s(\n", spaces+15, "While Statement");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        if (stmt->as.while_stmt.lbl != NULL)
            printf("%2$*1$s=`%3$s`\n", spaces+3, "lbl", stmt->as.while_stmt.lbl->cstr);
        printf("%2$*1$s=(\n", spaces+4, "cond");
        Expr_print(stmt->as.while_stmt.cond, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        printf("%2$*1$s=(\n", spaces+4, "body");
        Stmt_print(stmt->as.while_stmt.body, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case STMT_DOWHILE:
        printf("%2$*1$s(\n", spaces+18, "Do-While Statement");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        if (stmt->as.do_while_stmt.lbl != NULL)
            printf("%2$*1$s=`%3$s`\n", spaces+3, "lbl", stmt->as.do_while_stmt.lbl->cstr);
        printf("%2$*1$s=(\n", spaces+4, "body");
        Stmt_print(stmt->as.do_while_stmt.body, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        printf("%2$*1$s=(\n", spaces+4, "cond");
        Expr_print(stmt->as.do_while_stmt.cond, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case STMT_FOR:
        printf("%2$*1$s(\n", spaces+13, "For Statement");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        if (stmt->as.for_stmt.lbl != NULL)
            printf("%2$*1$s=`%3$s`\n", spaces+3, "lbl", stmt->as.for_stmt.lbl->cstr);

        if (stmt->as.for_stmt.init.kind == FOR_INIT_DECL) {
            printf("%2$*1$s=(\n", spaces+4, "init");
            Decl_print(stmt->as.for_stmt.init.as.decl, indent_lvl+1);
            printf("%2$*1$c\n", spaces+1, ')');
        } else if (stmt->as.for_stmt.init.kind == FOR_INIT_EXP) {
            printf("%2$*1$s=(\n", spaces+4, "init");
            Expr_print(stmt->as.for_stmt.init.as.exp, indent_lvl+1);
            printf("%2$*1$c\n", spaces+1, ')');
        }
        if (stmt->as.for_stmt.cond != NULL) {
            printf("%2$*1$s=(\n", spaces+4, "cond");
            Expr_print(stmt->as.for_stmt.cond, indent_lvl+1);
            printf("%2$*1$c\n", spaces+1, ')');
        }
        if (stmt->as.for_stmt.post != NULL) {
            printf("%2$*1$s=(\n", spaces+4, "post");
            Expr_print(stmt->as.for_stmt.cond, indent_lvl+1);
            printf("%2$*1$c\n", spaces+1, ')');
        }
        printf("%2$*1$s=(\n", spaces+4, "body");
        Stmt_print(stmt->as.for_stmt.body, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case STMT_SWITCH:
        printf("%2$*1$s(\n", spaces+16, "Switch Statement");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s=(\n", spaces+4, "ctrl");
        Expr_print(stmt->as.switch_stmt.ctrl_expr, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        if (stmt->as.switch_stmt.cases != NULL) {
            printf("%2$*1$s=(\n", spaces+5, "cases");
            indent_lvl += 1;
            spaces = indent_lvl * SPACES_PER_INDENT;
            for (size_t cidx = 0; cidx < stmt->as.switch_stmt.cases->cap; cidx++) {
                SymEntry *case_lbl = &stmt->as.switch_stmt.cases->syms[cidx];
                if (case_lbl->status == STE_OCCUPIED) {
                    printf("%2$*1$c%3$s`\n", spaces+1, '`', case_lbl->key->cstr);
                }
            }
            indent_lvl -= 1;
            spaces = indent_lvl * SPACES_PER_INDENT;
            printf("%2$*1$c\n", spaces+1, ')');
        }
        printf("%2$*1$s=(\n", spaces+4, "body");
        Stmt_print(stmt->as.switch_stmt.body, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case STMT_CASE:
        printf("%2$*1$s(\n", spaces+14, "Case Statement");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s=(\n", spaces+8, "lbl_expr");
        Expr_print(stmt->as.case_stmt.lbl_expr, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        if (stmt->as.case_stmt.lbl != NULL) {
            printf("%2$*1$s=`%3$s`\n", spaces+3, "lbl", stmt->as.case_stmt.lbl->cstr);
        }
        printf("%2$*1$s=(\n", spaces+4, "stmt");
        Stmt_print(stmt->as.case_stmt.stmt, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case STMT_DEFAULT:
        printf("%2$*1$s(\n", spaces+17, "Default Statement");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        if (stmt->as.default_stmt.lbl != NULL) {
            printf("%2$*1$s=`%3$s`\n", spaces+3, "lbl", stmt->as.default_stmt.lbl->cstr);
        }
        printf("%2$*1$s=(\n", spaces+4, "stmt");
        Stmt_print(stmt->as.default_stmt.stmt, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case STMT_NULL:
        printf("%2$*1$s\n", spaces+14, "Null Statement");
        break;
    }
}

void Decl_print(Decl *decl, int indent_lvl) {
    if (decl == NULL) return;

    int spaces = indent_lvl * SPACES_PER_INDENT;

    switch (decl->kind) {
    case DECL_INVALID:
        printf("%2$*1$s\n", spaces+12, "INVALID DECL");
        break;
    case DECL_LCL_VAR:
        printf("%2$*1$s(\n", spaces+14, "Local Var Decl");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s=%3$s\n", spaces+4, "name", decl->as.loc_var.identifier->cstr);
        printf("%2$*1$s=(%3$s", spaces+4, "init", (decl->as.loc_var.init == NULL) ? "NULL\n" : "\n");
        if (decl->as.loc_var.init != NULL) {
            Expr_print(decl->as.loc_var.init, indent_lvl+1);
        }
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    }

}

static void BlockItem_print(BlockItem *item, int indent_lvl) {
    if (item == NULL) return;

    int spaces = indent_lvl * SPACES_PER_INDENT;

    switch (item->kind) {
    case BLOCKITEM_INVALID:
        printf("%2$*1$s\n", spaces+17, "INVALID BLOCKITEM");
        break;
    case BLOCKITEM_STATEMENT:
        printf("%2$*1$s(\n", spaces+9, "Statement");
        Stmt_print(item->as.statement, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    case BLOCKITEM_DECLARATION:
        printf("%2$*1$s(\n", spaces+11, "Declaration");
        Decl_print(item->as.declaration, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    }
}

static void Block_print(Block *block, int indent_lvl) {
    if (block == NULL) return;
    for (size_t idx = 0; idx < block->len; idx++) {
        BlockItem_print(&block->items[idx], indent_lvl);
    }
}

void Function_print(Function *func, int indent_lvl) {
    if (func == NULL) return;

    int spaces = indent_lvl * SPACES_PER_INDENT;

    printf("%2$*1$s\n", spaces+9, "Function(");
    indent_lvl += 1;
    spaces = indent_lvl * SPACES_PER_INDENT;
    printf("%2$*1$s=\"%3$s\"\n%5$*4$s=(\n",
        spaces+4, "name", func->name->cstr, spaces+4, "body");
    for (size_t b_idx = 0; b_idx < func->block->len; b_idx++) {
        BlockItem_print(&func->block->items[b_idx], indent_lvl+1);
    }
    printf("%2$*1$c\n", spaces+1, ')');
    indent_lvl -= 1;
    spaces = indent_lvl * SPACES_PER_INDENT;
    printf("%2$*1$c\n", spaces+1, ')');
}

void Program_print(AstProgram *prog) {
    if (prog == NULL) return;
    if (prog->func == NULL) {
        printf("NULL PROGRAM\n");
        return;
    }

    printf("Program(\n");
    Function_print(prog->func, 1);
    printf(")\n");
}
