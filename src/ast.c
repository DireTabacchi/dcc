#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include "dd_string.h"

#include "ast.h"

// How many spaces to include per indent level. Used in AstNode_print
#define SPACES_PER_INDENT 4

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
        free(stmt);
        break;
    case STMT_RET:
        Expr_destroy(stmt->as.ret);
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
            String_free(&decl->as.loc_var.identifier);
            Expr_destroy(decl->as.loc_var.init);
            free(decl);
            break;
    }
}

void Block_init(Block *block) {
    block->cap = 2;
    block->len = 0;
    block->items = (BlockItem *)calloc(block->cap, sizeof(BlockItem));
}

void Block_deinit(Block *block) {
    if (block == NULL) return;
    if (block->items == NULL) return;

    for (size_t b_idx = 0; b_idx < block->len; b_idx++) {
        switch (block->items[b_idx].kind) {
        case BLOCKITEM_STATEMENT:
            Stmt_destroy(block->items[b_idx].as.statement);
            break;
        }
    }

    block->cap = 0;
    block->len = 0;

    free(block->items);
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

Function *Function_create() {
    Function *func = (Function *)malloc(sizeof(Function));
    func->name = (String){0};
    Block_init(&func->block);
    return func;
}

void Function_destroy(Function *func) {
    if (func == NULL) return;
    String_free(&func->name);
    Block_deinit(&func->block);
    free(func);
}

// TODO: Function list?
void Program_init(Program *prog) {
    /* NOP */
}

void Program_deinit(Program *prog) {
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

void Expr_print(Expr *expr, int indent_lvl) {
    if (expr == NULL) return;

    int spaces = indent_lvl * SPACES_PER_INDENT;

    switch(expr->kind) {
    case EXPR_INVALID:
        printf("%2$*1$s\n", spaces+18, "INVALID_EXPRESSION");
        break;
    case EXPR_CONSTANT:
        printf("%2$*1$s%3$d)\n", spaces+9, "Constant(", expr->as.constant);
        break;

    case EXPR_UNARY:
        printf("%2$*1$s\n", spaces+6, "Unary(");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s%3$s\n%5$*4$s\n", spaces+3, "op=",
            (expr->as.unary.op == UNARY_NEGATE) ? "Negate" :
                (expr->as.unary.op == UNARY_COMPLEMENT) ? "Complement" :
                    (expr->as.unary.op == UNARY_NOT) ? "Not" : "Invald",
            spaces + 5, "exp=(");
        Expr_print(expr->as.unary.expr, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;

    case EXPR_BINARY:
        printf("%2$*1$s\n", spaces+7, "Binary(");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s%3$s\n%5$*4$s\n",
            spaces+3, "op=",
            binary_op_names[expr->as.binary.op],
            spaces+6,
            "left=(");
        Expr_print(expr->as.binary.left, indent_lvl+1);
        printf("%2$*1$c\n%4$*3$s\n", spaces+1, ')', spaces+7, "right=(");
        Expr_print(expr->as.binary.right, indent_lvl+1);
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
        printf("Return(\n");
        Expr_print(stmt->as.ret, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    }
}

static void BlockItem_print(BlockItem *item, int indent_lvl) {
    if (item == NULL) return;

    int spaces = indent_lvl * SPACES_PER_INDENT;

    switch (item->kind) {
    case BLOCKITEM_INVALID:
        printf("%2$*1$s\n", spaces+17, "INVALID_BLOCKITEM");
        break;
    case BLOCKITEM_STATEMENT:
        printf("%2$*1$s(\n", spaces+9, "Statement");
        Stmt_print(item->as.statement, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    case BLOCKITEM_DECLARATION:
        printf("%2$*1$s(\n", spaces+11, "Declaration");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s\n", spaces+38, "This is unhandled. How'd you get here?");
        //Stmt_print(item->as.statement, indent_lvl+1);
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    }
}

void Function_print(Function *func, int indent_lvl) {
    if (func == NULL) return;

    int spaces = indent_lvl * SPACES_PER_INDENT;

    printf("%2$*1$s\n", spaces+9, "Function(");
    indent_lvl += 1;
    spaces = indent_lvl * SPACES_PER_INDENT;
    printf("%2$*1$s\"%3$s\"\n%5$*4$s",
        spaces+5, "name=", func->name.cstr, spaces+5, "body=");
    for (size_t b_idx = 0; b_idx < func->block.len; b_idx++) {
        BlockItem_print(&func->block.items[b_idx], indent_lvl);
    }
    indent_lvl -= 1;
    spaces = indent_lvl * SPACES_PER_INDENT;
    printf("%2$*1$c\n", spaces+1, ')');
}

void Program_print(Program *prog) {
    if (prog == NULL) return;
    if (prog->func == NULL) {
        printf("NULL PROGRAM\n");
        return;
    }

    printf("Program(\n");
    Function_print(prog->func, 1);
    printf(")\n");
}
