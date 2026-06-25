#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dd_string.h"
#include "ast.h"

#include "tacd.h"

TacdNode *TacdNode_create() {
    TacdNode *n = malloc(sizeof(TacdNode));
    *n = (TacdNode){0};
    return n;
}

void TacdNode_destroy(TacdNode *node) {
    if (node == NULL) return;

    switch (node->kind) {
    case TACD_NODE_INVALID:
        free(node);
        break;
    case TACD_NODE_PROGRAM:
        TacdNode_destroy(node->node.program.function);
        free(node);
        break;
    case TACD_NODE_FUNCTION:
        String_free(&node->node.function.name);
        CodeList_deinit(&node->node.function.body);
        free(node);
        break;
    }
}

void CodeList_init(CodeList *il) {
    il->cap = 2;
    il->len = 0;
    il->codes = (TacdCode*)calloc(il->cap, sizeof(TacdCode));
}

void CodeList_deinit(CodeList* il) {
    if (il == NULL) return;
    if (il->codes == NULL) return;

    il->cap = 0;
    il->len = 0;
    free(il->codes);
}

void CodeList_append(CodeList* cl, TacdCode code) {
    if (cl == NULL) return;
    if (cl->codes == NULL) return;

    if (cl->len == cl->cap) {
        size_t old_cap = cl->cap;
        size_t new_cap = old_cap / 2 + old_cap;
        TacdCode *new_codes = calloc(new_cap, sizeof(TacdCode));
        memcpy(new_codes, cl->codes, old_cap*sizeof(TacdCode));
        free(cl->codes);
        cl->codes = new_codes;
        cl->cap = new_cap;
    }

    memcpy(&cl->codes[cl->len], &code, sizeof(TacdCode));
    cl->len += 1;
}

void TacdSymTable_init(TacdSymTable *tst) {
    tst->cap = 2;
    tst->len = 0;
    tst->syms = (String*)calloc(tst->cap, sizeof(String));
}

void TacdSymTable_deinit(TacdSymTable *tst) {
    if (tst == NULL) return;
    if (tst->syms == NULL) return;

    for (size_t sym_idx = 0; sym_idx < tst->len; sym_idx++) {
        String_free(&tst->syms[sym_idx]);
    }
    tst->cap = 0;
    tst->len = 0;
    free(tst->syms);
}

void TacdSymTable_append(TacdSymTable *tst, String symbol) {
    if (tst == NULL) return;
    if (tst->syms == NULL) return;

    if (tst->len == tst->cap) {
        size_t old_cap = tst->cap;
        size_t new_cap = old_cap / 2 + old_cap;
        String *new_syms = calloc(new_cap, sizeof(String));
        memcpy(new_syms, tst->syms, old_cap*sizeof(String));
        free(tst->syms);
        tst->syms = new_syms;
        tst->cap = new_cap;
    }

    memcpy(&tst->syms[tst->len], &symbol, sizeof(String));
    tst->len += 1;
}

void TacdGenerator_init(TacdGenerator *tg) {
    tg->program = TacdNode_create();
    tg->program->kind = TACD_NODE_PROGRAM;
    tg->tmpvar_count = 0;
    TacdSymTable_init(&tg->symbols);
}

void TacdGenerator_deinit(TacdGenerator *tg) {
    String_free(&tg->func_name);
    TacdNode_destroy(tg->program);
    TacdSymTable_deinit(&tg->symbols);
}

// Return the number of base-10 places this number contains.
static int integer_len(int num) {
    if (num == 0) {
        return 1;
    }

    int digit_len = 0;
    while (num != 0) {
        num /= 10;
        digit_len += 1;
    }
    return digit_len;
}

static String create_temporary_var(TacdGenerator *tg) {
    int digit_len = integer_len(tg->tmpvar_count);

    // var length: func_name.len + (5) ".tmp." + digit_len
    String var_name = String_init_length(tg->func_name.len + 5 + digit_len);
    snprintf(var_name.cstr, var_name.len+1, "%s.tmp.%d", tg->func_name.cstr, tg->tmpvar_count);
    tg->tmpvar_count += 1;
    TacdSymTable_append(&tg->symbols, var_name);
    return var_name;
}

static TacdValue trx_expression(TacdGenerator *tg, TacdNode *tacd_fn, AstNode *expr) {
    switch (expr->kind) {
    case ASTNODE_CONSTANT:
        return (TacdValue){
            .kind = TACD_VALUE_CONSTANT,
            .val.constant = expr->node.constant
        };
    case ASTNODE_UNARY: {
        TacdValue src = trx_expression(tg, tacd_fn, expr->node.unary.exp);
        String dest_name = create_temporary_var(tg);
        TacdValue dest = (TacdValue){
            .kind = TACD_VALUE_IDENTIFIER,
            .val.identifier = dest_name
        };
        TacdCode unop = {0};
        unop.kind = TACD_CODE_UNARY;
        if (expr->node.unary.op == UNARY_COMPLEMENT) {
            unop.code.unary.op = TACD_UNARY_COMPLEMENT;
        } else if (expr->node.unary.op == UNARY_NEGATE) {
            unop.code.unary.op = TACD_UNARY_NEGATE;
        }
        unop.code.unary.src = src;
        unop.code.unary.dest = dest;
        CodeList_append(&tacd_fn->node.function.body, unop);

        return dest;
    }
    default:
        break;
    }

    return (TacdValue){ .kind = TACD_VALUE_INVALID };
}

static void trx_statement(TacdGenerator *tg, TacdNode *tacd_fn, AstNode *statement) {
    switch(statement->kind) {
    case ASTNODE_RETURN: {
        TacdValue val = trx_expression(tg, tacd_fn, statement->node.ret.expr);
        TacdCode ret = {0};
        ret.kind = TACD_CODE_RET;
        ret.code.ret.val = val;
        CodeList_append(&tacd_fn->node.function.body, ret);
        break;
    }
    default:
        break;

    }
}

static void trx_function(TacdGenerator *tg, TacdNode *tacd_fn, AstNode *ast_fn) {
    if (ast_fn->kind != ASTNODE_FUNCTION) return;

    tg->func_name = String_copy(ast_fn->node.function.name);

    tacd_fn->kind = TACD_NODE_FUNCTION;
    tacd_fn->node.function.name = String_copy(ast_fn->node.function.name);
     CodeList_init(&tacd_fn->node.function.body);

    trx_statement(tg, tacd_fn, ast_fn->node.function.statement);
}

void generate_tacd(TacdGenerator *tg, AstNode *ast_prog) {
    if (tg == NULL) return;
    if (ast_prog == NULL) return;
    if (ast_prog->kind != ASTNODE_PROGRAM) return;

    tg->program->node.program.function = TacdNode_create();
    trx_function(tg, tg->program->node.program.function, ast_prog->node.program.function);
}

// IDEA: Perhaps this should just return a String object?
static void TacdValue_print(TacdValue value, int indent_lvl) {
    int spaces = indent_lvl * 4;
    switch(value.kind) {
    case TACD_VALUE_INVALID:
        /*  Nothing to print.   */
        break;
    case TACD_VALUE_IDENTIFIER:
        printf("%2$*1$s", spaces+(int)value.val.identifier.len, value.val.identifier.cstr);
        break;
    case TACD_VALUE_CONSTANT:
        printf("%2$*1$d", spaces+integer_len(value.val.constant), value.val.constant);
        break;
    }
}

static void TacdCode_print(TacdCode *code, int indent_lvl) {
    int spaces = indent_lvl * 4;
    switch (code->kind) {
    case TACD_CODE_INVALID:
        /*  nothing to print.   */
        break;
    case TACD_CODE_UNARY:
        TacdValue_print(code->code.unary.dest, indent_lvl);
        if (code->code.unary.op == TACD_UNARY_NEGATE) {
            printf(" = -");
        } else if (code->code.unary.op == TACD_UNARY_COMPLEMENT) {
            printf(" = ~");
        }
        TacdValue_print(code->code.unary.src, 0);
        printf("\n");
        break;
    case TACD_CODE_RET:
        printf("%2$*1$s", spaces+7, "return ");
        TacdValue_print(code->code.ret.val, 0);
        printf("\n");
        break;
    }
}

static void TacdNode_print(TacdNode *node, int indent_lvl) {
    int spaces = indent_lvl * 4;
    switch (node->kind) {
    case TACD_NODE_INVALID:
        /*  nothing to print.   */
        break;
    case TACD_NODE_PROGRAM: {
        printf("%2$*1$s\n", spaces+8, "Program:");
        TacdNode_print(node->node.program.function, indent_lvl);
        break;
    }
    case TACD_NODE_FUNCTION: {
        printf("%2$*1$s %3$s:\n",
            spaces+2,
            "fn", node->node.function.name.cstr);
        CodeList *instructions = &node->node.function.body;
        indent_lvl += 1;
        for (size_t instr_idx = 0; instr_idx < instructions->len; instr_idx++) {
            TacdCode_print(&instructions->codes[instr_idx], indent_lvl);
        }
        indent_lvl -= 1;
        break;
    }
    }
}

void Tacd_print(TacdNode *node, int indent_lvl) {
    puts("Generated TACD\n==============");
    TacdNode_print(node, indent_lvl);
}
