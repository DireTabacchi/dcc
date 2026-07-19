#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "comp_driver.h"
#include "dd_string.h"
#include "interner.h"
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

    free(il->codes);
    il->cap = 0;
    il->len = 0;
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

//void TacdSymTable_init(TacdSymTable *tst) {
//    tst->cap = 2;
//    tst->len = 0;
//    tst->syms = (String*)calloc(tst->cap, sizeof(String));
//}
//
//void TacdSymTable_deinit(TacdSymTable *tst) {
//    if (tst == NULL) return;
//    if (tst->syms == NULL) return;
//
//    for (size_t sym_idx = 0; sym_idx < tst->len; sym_idx++) {
//        String_free(&tst->syms[sym_idx]);
//    }
//    free(tst->syms);
//    tst->cap = 0;
//    tst->len = 0;
//}
//
//void TacdSymTable_append(TacdSymTable *tst, String *symbol) {
//    if (tst == NULL) return;
//    if (tst->syms == NULL) return;
//
//    if (tst->len == tst->cap) {
//        size_t old_cap = tst->cap;
//        size_t new_cap = old_cap / 2 + old_cap;
//        String **new_syms = calloc(new_cap, sizeof(String *));
//        memcpy(new_syms, tst->syms, old_cap*sizeof(String));
//        free(tst->syms);
//        tst->syms = new_syms;
//        tst->cap = new_cap;
//    }
//
//    memcpy(&tst->syms[tst->len], &symbol, sizeof(String));
//    tst->len += 1;
//}

void TacdGenerator_init(TacdGenerator *tg) {
    tg->program = TacdNode_create();
    tg->program->kind = TACD_NODE_PROGRAM;
    //TacdSymTable_init(&tg->symbols);
}

void TacdGenerator_deinit(TacdGenerator *tg) {
    //String_free(&tg->func_name);
    TacdNode_destroy(tg->program);
    //TacdSymTable_deinit(&tg->symbols);
}

static const String *create_temporary_var(CompDriver *cd) {
    int digit_len = integer_len(cd->uid_count);

    // var length: func_name.len + (5) ".tmp." + digit_len
    String var_name = String_init_length(cd->cgd.tacd_gen.func_name->len + 5 + digit_len);
    snprintf(var_name.cstr, var_name.len+1, "%s.tmp.%ld",
        cd->cgd.tacd_gen.func_name->cstr, cd->uid_count);
    cd->uid_count += 1;
    const String *canon_varname = StrInterner_intern(&cd->str_table, var_name);
    String_free(&var_name);
    //TacdSymTable_append(&cd->cgd.tacd_gen.symbols, canon_varname);
    return canon_varname;
}

static const String *create_label(CompDriver *cd, TacdLabelKind kind) {
    int digit_len = integer_len(cd->cgd.tacd_gen.label_count);

    String label_kind = label_kind_table[kind];

    // label length: func_name.len + (1) "." + label_kind.len + (1) "." + digit_len
    String label = String_init_length(cd->cgd.tacd_gen.func_name->len + 2 + label_kind.len + digit_len);
    snprintf(label.cstr, label.len+1, "%s.%s.%d", cd->cgd.tacd_gen.func_name->cstr, label_kind.cstr, cd->cgd.tacd_gen.label_count);
    cd->cgd.tacd_gen.label_count += 1;
    //TacdSymTable_append(&tg->symbols, label);
    const String *canon_label = StrInterner_intern(&cd->str_table, label);
    String_free(&label);
    return canon_label;
}

static TacdBinaryOp trx_binary_operator(BinaryOpKind op) {
    switch (op) {
    case BINARY_INVALID:
        return TACD_BINARY_INVALID;
    case BINARY_ADD:
        return TACD_BINARY_ADD;
    case BINARY_SUBTRACT:
        return TACD_BINARY_SUBTRACT;
    case BINARY_MULTIPLY:
        return TACD_BINARY_MULTIPLY;
    case BINARY_DIVIDE:
        return TACD_BINARY_DIVIDE;
    case BINARY_REMAINDER:
        return TACD_BINARY_REMAINDER;
    case BINARY_BITAND:
        return TACD_BINARY_BITAND;
    case BINARY_BITOR:
        return TACD_BINARY_BITOR;
    case BINARY_BITXOR:
        return TACD_BINARY_BITXOR;
    case BINARY_LSHFT:
        return TACD_BINARY_LSHFT;
    case BINARY_RSHFT:
        return TACD_BINARY_RSHFT;
    case BINARY_EQUAL:
        return TACD_BINARY_EQUAL;
    case BINARY_NOT_EQUAL:
        return TACD_BINARY_NOT_EQUAL;
    case BINARY_LT:
        return TACD_BINARY_LT;
    case BINARY_LTE:
        return TACD_BINARY_LTE;
    case BINARY_GT:
        return TACD_BINARY_GT;
    case BINARY_GTE:
        return TACD_BINARY_GTE;
    case BINARY_LOGICAND:   // These shouldn't be converted here.
    case BINARY_LOGICOR:
        return TACD_BINARY_INVALID;
    }
}

static TacdValue trx_expression(CompDriver *cd, TacdNode *tacd_fn, Expr *expr) {
    switch (expr->kind) {
    case EXPR_CONSTANT:
        return (TacdValue){
            .kind = TACD_VALUE_CONSTANT,
            .val.constant = expr->as.constant
        };

    case EXPR_UNARY: {
        TacdValue src = trx_expression(cd, tacd_fn, expr->as.unary.expr);
        const String *dest_name = create_temporary_var(cd);
        TacdValue dest = (TacdValue){
            .kind = TACD_VALUE_IDENTIFIER,
            .val.identifier = dest_name
        };
        TacdCode unop = {0};
        unop.kind = TACD_CODE_UNARY;
        if (expr->as.unary.op == UNARY_COMPLEMENT) {
            unop.code.unary.op = TACD_UNARY_COMPLEMENT;
        } else if (expr->as.unary.op == UNARY_NEGATE) {
            unop.code.unary.op = TACD_UNARY_NEGATE;
        } else if (expr->as.unary.op == UNARY_NOT) {
            unop.code.unary.op = TACD_UNARY_NOT;
        }
        unop.code.unary.src = src;
        unop.code.unary.dest = dest;
        CodeList_append(&tacd_fn->node.function.body, unop);

        return dest;
    }

    case EXPR_BINARY: {
        if (expr->as.binary.op == BINARY_LOGICAND || expr->as.binary.op == BINARY_LOGICOR) {
            TacdValue result = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            TacdValue left_result = trx_expression(cd, tacd_fn, expr->as.binary.left);
            const String *jump_condition_label = NULL;
            TacdCode jump_conditional = {0};
            TacdCode copy_res = {
                .kind = TACD_CODE_COPY,
                .code.copy = {
                    .src.kind = TACD_VALUE_CONSTANT, .dest = result
                }
            };
            const String *end_label = NULL;
            TacdCode jump_end = { .kind = TACD_CODE_JUMP };
            TacdCode condition_label_code = { .kind = TACD_CODE_LABEL };
            TacdCode end_label_code = { .kind = TACD_CODE_LABEL };
            if (expr->as.binary.op == BINARY_LOGICAND) {
                jump_condition_label = create_label(cd, AND_FALSE);
                jump_conditional.kind = TACD_CODE_JUMP_IF_ZERO;
                copy_res.code.copy.src.val.constant = 1;
                end_label = create_label(cd, AND_END);
            } else if (expr->as.binary.op == BINARY_LOGICOR) {
                jump_condition_label = create_label(cd, OR_TRUE);
                jump_conditional.kind = TACD_CODE_JUMP_IF_NOT_ZERO;
                copy_res.code.copy.src.val.constant = 0;
                end_label = create_label(cd, OR_END);
            }

            jump_conditional.code.jump_conditional.condition = left_result;
            jump_conditional.code.jump_conditional.target = jump_condition_label;
            CodeList_append(&tacd_fn->node.function.body, jump_conditional);
            TacdValue right_result = trx_expression(cd, tacd_fn, expr->as.binary.right);
            jump_conditional.code.jump_conditional.condition = right_result;
            CodeList_append(&tacd_fn->node.function.body, jump_conditional);
            CodeList_append(&tacd_fn->node.function.body, copy_res);
            jump_end.code.jump = end_label;
            CodeList_append(&tacd_fn->node.function.body, jump_end);
            condition_label_code.code.label = jump_condition_label;
            CodeList_append(&tacd_fn->node.function.body, condition_label_code);

            if (expr->as.binary.op == BINARY_LOGICAND) {
                copy_res.code.copy.src.val.constant = 0;
            } else if (expr->as.binary.op == BINARY_LOGICOR) {
                copy_res.code.copy.src.val.constant = 1;
            }

            CodeList_append(&tacd_fn->node.function.body, copy_res);
            end_label_code.code.label = end_label;
            CodeList_append(&tacd_fn->node.function.body, end_label_code);

            return result;
        }

        TacdValue src1 = trx_expression(cd, tacd_fn, expr->as.binary.left);
        TacdValue src2 = trx_expression(cd, tacd_fn, expr->as.binary.right);

        const String *dest_name = create_temporary_var(cd);
        TacdValue dest = (TacdValue){
            .kind = TACD_VALUE_IDENTIFIER,
            .val.identifier = dest_name
        };

        TacdCode binop = {0};
        binop.kind = TACD_CODE_BINARY;
        binop.code.binary.op = trx_binary_operator(expr->as.binary.op);
        binop.code.binary.src1 = src1;
        binop.code.binary.src2 = src2;
        binop.code.binary.dest = dest;
        CodeList_append(&tacd_fn->node.function.body, binop);

        return dest;
    }

    case EXPR_VAR: {
        TacdValue var = {0};
        var.kind = TACD_VALUE_IDENTIFIER;
        var.val.identifier = expr->as.var;
        return var;
    }

    case EXPR_ASSIGN: {
        TacdValue lhs = trx_expression(cd, tacd_fn, expr->as.assign.lhs);
        TacdValue rhs = trx_expression(cd, tacd_fn, expr->as.assign.rhs);
        TacdCode assign_copy = {0};
        assign_copy.kind = TACD_CODE_COPY;
        assign_copy.code.copy.dest = lhs;
        assign_copy.code.copy.src = rhs;
        CodeList_append(&tacd_fn->node.function.body, assign_copy);
        return lhs;
    }

    case EXPR_INVALID: // Should err?
        break;
    }

    return (TacdValue){ .kind = TACD_VALUE_INVALID };
}

static void trx_statement(CompDriver *cd, TacdNode *tacd_fn, Stmt *stmt) {
    switch(stmt->kind) {
    case STMT_RET: {
        TacdValue val = trx_expression(cd, tacd_fn, stmt->as.ret);
        TacdCode ret = {0};
        ret.kind = TACD_CODE_RET;
        ret.code.ret = val;
        CodeList_append(&tacd_fn->node.function.body, ret);
        break;
    }
    case STMT_EXPR: {
        trx_expression(cd, tacd_fn, stmt->as.expr);
        break;
    }
    case STMT_NULL:
    case STMT_INVALID: // Should err?
        break;

    }
}

// TODO: eventually handle other declarations
static void trx_declaration(CompDriver *cd, TacdNode *tacd_fn, Decl *decl) {
    if (decl->as.loc_var.init == NULL) return;

    TacdValue lhs = { .kind = TACD_VALUE_IDENTIFIER, .val.identifier = decl->as.loc_var.identifier };
    TacdValue rhs = trx_expression(cd, tacd_fn, decl->as.loc_var.init);
    TacdCode assign_copy = {0};
    assign_copy.kind = TACD_CODE_COPY;
    assign_copy.code.copy.dest = lhs;
    assign_copy.code.copy.src = rhs;
    CodeList_append(&tacd_fn->node.function.body, assign_copy);
}

static void trx_blockitem(CompDriver *cd, TacdNode *tacd_fn, BlockItem *item) {
    switch(item->kind) {
    case BLOCKITEM_STATEMENT:
        trx_statement(cd, tacd_fn, item->as.statement);
        break;
    case BLOCKITEM_DECLARATION:
        trx_declaration(cd, tacd_fn, item->as.declaration);
        break;
    default:
        break;
    }
}

static void trx_function(CompDriver *cd, TacdNode *tacd_fn, Function *ast_fn) {
    cd->cgd.tacd_gen.func_name = ast_fn->name;

    tacd_fn->kind = TACD_NODE_FUNCTION;
    tacd_fn->node.function.name = ast_fn->name;
    CodeList_init(&tacd_fn->node.function.body);

    for (size_t b_idx = 0; b_idx < ast_fn->block.len; b_idx++) {
        trx_blockitem(cd, tacd_fn, &ast_fn->block.items[b_idx]);
    }

    TacdCode implicit_ret = { .kind = TACD_CODE_RET };
    implicit_ret.code.ret.kind = TACD_VALUE_CONSTANT;
    implicit_ret.code.ret.val.constant = 0;
    CodeList_append(&tacd_fn->node.function.body, implicit_ret);
}

void generate_tacd(CompDriver *cd, AstProgram *ast_prog) {
    TacdGenerator *tg = &cd->cgd.tacd_gen;

    tg->program->node.program.function = TacdNode_create();
    trx_function(cd, tg->program->node.program.function, ast_prog->func);
}

static void TacdValue_print(TacdValue value, int indent_lvl) {
    int spaces = indent_lvl * 4;
    switch(value.kind) {
    case TACD_VALUE_INVALID:
        printf("%2$*1$s", spaces+7, "INVALID");
        break;
    case TACD_VALUE_IDENTIFIER:
        printf("%2$*1$s", spaces+(int)value.val.identifier->len, value.val.identifier->cstr);
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
        puts("INVALID");
        break;

    case TACD_CODE_UNARY:
        TacdValue_print(code->code.unary.dest, indent_lvl);
        if (code->code.unary.op == TACD_UNARY_NEGATE) {
            printf(" = -");
        } else if (code->code.unary.op == TACD_UNARY_COMPLEMENT) {
            printf(" = ~");
        } else if (code->code.unary.op == TACD_UNARY_NOT) {
            printf(" = !");
        }
        TacdValue_print(code->code.unary.src, 0);
        printf("\n");
        break;

    case TACD_CODE_BINARY:
        TacdValue_print(code->code.binary.dest, indent_lvl);
        printf(" = ");
        TacdValue_print(code->code.binary.src1, 0);
        switch (code->code.binary.op) {
        case TACD_BINARY_INVALID:
            printf(" ??? ");
            break;
        case TACD_BINARY_ADD:
            printf(" + ");
            break;
        case TACD_BINARY_SUBTRACT:
            printf(" - ");
            break;
        case TACD_BINARY_MULTIPLY:
            printf(" * ");
            break;
        case TACD_BINARY_DIVIDE:
            printf(" / ");
            break;
        case TACD_BINARY_REMAINDER:
            printf(" %% ");
            break;
        case TACD_BINARY_BITAND:
            printf(" & ");
            break;
        case TACD_BINARY_BITOR:
            printf(" | ");
            break;
        case TACD_BINARY_BITXOR:
            printf(" ^ ");
            break;
        case TACD_BINARY_LSHFT:
            printf(" << ");
            break;
        case TACD_BINARY_RSHFT:
            printf(" >> ");
            break;
        case TACD_BINARY_EQUAL:
            printf(" == ");
            break;
        case TACD_BINARY_NOT_EQUAL:
            printf(" != ");
            break;
        case TACD_BINARY_LT:
            printf(" < ");
            break;
        case TACD_BINARY_LTE:
            printf(" <= ");
            break;
        case TACD_BINARY_GT:
            printf(" > ");
            break;
        case TACD_BINARY_GTE:
            printf(" >= ");
            break;
        }
        TacdValue_print(code->code.binary.src2, 0);
        printf("\n");
        break;

    case TACD_CODE_COPY:
        TacdValue_print(code->code.copy.dest, indent_lvl);
        printf(" = ");
        TacdValue_print(code->code.copy.src, 0);
        printf("\n");
        break;

    case TACD_CODE_JUMP:
        printf("%2$*1$s(%3$s)\n", spaces+4, "jump", code->code.jump->cstr);
        break;

    case TACD_CODE_JUMP_IF_ZERO:
        printf("%2$*1$s(", spaces+12, "jump_if_zero");
        TacdValue_print(code->code.jump_conditional.condition, 0);
        printf(",%s)\n", code->code.jump_conditional.target->cstr);
        break;

    case TACD_CODE_JUMP_IF_NOT_ZERO:
        printf("%2$*1$s(", spaces+16, "jump_if_not_zero");
        TacdValue_print(code->code.jump_conditional.condition, 0);
        printf(",%s)\n", code->code.jump_conditional.target->cstr);
        break;
    
    case TACD_CODE_LABEL:
        printf("%s(%s)\n", "label", code->code.label->cstr);
        break;

    case TACD_CODE_RET:
        printf("%2$*1$s", spaces+7, "return ");
        TacdValue_print(code->code.ret, 0);
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
            "fn", node->node.function.name->cstr);
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
