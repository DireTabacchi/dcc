#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "comp_driver.h"
#include "dd_string.h"
#include "interner.h"
#include "ast.h"
#include "sym_table.h"

#include "tacd.h"

// Forward declarations

static void trx_block(CompDriver *cd, TacdFunction *tacd_fn, Block *block);
static void trx_declaration(CompDriver *cd, TacdFunction *tacd_fn, Decl *decl);

// Definitions

TacdProgram *TacdProgram_create() {
    TacdProgram *prog = malloc(sizeof(TacdProgram));
    *prog = (TacdProgram){0};
    TacdFunctionArray_init(&prog->fn_defs);
    return prog;
}

void TacdProgram_destroy(TacdProgram *prog) {
    if (prog == NULL) return;

    TacdFunctionArray_deinit(&prog->fn_defs);
    free(prog);
}

void TacdFunctionArray_init(TacdFunctionArray *tfa) {
    tfa->len = 0;
    tfa->cap = 2;
    tfa->fns = calloc(tfa->cap, sizeof(TacdFunction));
}

void TacdFunctionArray_deinit(TacdFunctionArray *tfa) {
    free(tfa->fns);
    tfa->fns = NULL;
    tfa->cap =  0;
    tfa->len = 0;
}

void TacdFunctionArray_append(TacdFunctionArray *tfa, TacdFunction fn) {
    if (tfa == NULL) return;
    if (tfa->fns == NULL) return;

    if (tfa->len == tfa->cap) {
        size_t old_cap = tfa->cap;
        size_t new_cap = old_cap / 2 + old_cap;
        TacdFunction *new_fns = calloc(new_cap, sizeof(TacdFunction));
        memcpy(new_fns, tfa->fns, old_cap*sizeof(TacdFunction));
        free(tfa->fns);
        tfa->fns = new_fns;
        tfa->cap = new_cap;
    }

    memcpy(&tfa->fns[tfa->len], &fn, sizeof(TacdFunction));
    tfa->len += 1;
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

void TacdGenerator_init(TacdGenerator *tg) {
    tg->program = TacdProgram_create();
}

void TacdGenerator_deinit(TacdGenerator *tg) {
    TacdProgram_destroy(tg->program);
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
    return canon_varname;
}

static const String *create_label(CompDriver *cd, LabelKind kind, const String *stmt_lbl) {
    String label_kind = label_kind_table[kind];

    // label length: func_name.len + (1) "." + label_kind.len + (1) "." + digit_len
    String label = {0};

    if (stmt_lbl == NULL) {
        int digit_len = integer_len(cd->cgd.tacd_gen.label_count);

        label = String_init_length(cd->cgd.tacd_gen.func_name->len + 2 + label_kind.len + digit_len);
        snprintf(label.cstr, label.len+1, "%s.%s.%d",
            cd->cgd.tacd_gen.func_name->cstr, label_kind.cstr, cd->cgd.tacd_gen.label_count);
        cd->cgd.tacd_gen.label_count += 1;
    } else {
        label = String_init_length(cd->cgd.tacd_gen.func_name->len+2+label_kind.len+stmt_lbl->len);
        snprintf(label.cstr, label.len+1, "%s.%s.%s",
            cd->cgd.tacd_gen.func_name->cstr, label_kind.cstr, stmt_lbl->cstr);
    }
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

static TacdValue trx_expression(CompDriver *cd, TacdFunction *tacd_fn, Expr *expr) {
    switch (expr->kind) {
    case EXPR_CONSTANT:
        return (TacdValue){ .kind = TACD_VALUE_CONSTANT, .val.constant = expr->as.constant };

    case EXPR_UNARY: {
        TacdValue src = trx_expression(cd, tacd_fn, expr->as.unary.expr);
        TacdCode unop = {0};
        unop.kind = TACD_CODE_UNARY;
        switch (expr->as.unary.op) {
        case UNARY_INVALID:
            break;
        case UNARY_COMPLEMENT:
            unop.code.unary.op = TACD_UNARY_COMPLEMENT;
            break;
        case UNARY_NEGATE:
            unop.code.unary.op = TACD_UNARY_NEGATE;
            break;
        case UNARY_NOT:
            unop.code.unary.op = TACD_UNARY_NOT;
            break;
        case UNARY_PRE_INCR: {
            TacdCode incr = {0};
            incr.kind = TACD_CODE_BINARY;
            incr.code.binary.op = TACD_BINARY_ADD;
            incr.code.binary.dest = src;
            incr.code.binary.src1 = src;
            incr.code.binary.src2 = (TacdValue){ .kind = TACD_VALUE_CONSTANT, .val.constant = 1 };
            CodeList_append(&tacd_fn->body, incr);
            return src;
        }
        case UNARY_PRE_DECR: {
            TacdCode decr = {0};
            decr.kind = TACD_CODE_BINARY;
            decr.code.binary.op = TACD_BINARY_SUBTRACT;
            decr.code.binary.dest = src;
            decr.code.binary.src1 = src;
            decr.code.binary.src2 = (TacdValue){ .kind = TACD_VALUE_CONSTANT, .val.constant = 1 };
            CodeList_append(&tacd_fn->body, decr);
            return src;
        }
        case UNARY_POST_INCR: {
            const String *dest_name = create_temporary_var(cd);
            TacdValue dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = dest_name
            };

            TacdCode copy = {0};
            copy.kind = TACD_CODE_COPY;
            copy.code.copy.src = src;
            copy.code.copy.dest = dest;

            CodeList_append(&tacd_fn->body, copy);

            TacdCode incr = {0};
            incr.kind = TACD_CODE_BINARY;
            incr.code.binary.op = TACD_BINARY_ADD;
            incr.code.binary.dest = src;
            incr.code.binary.src1 = src;
            incr.code.binary.src2 = (TacdValue){ .kind = TACD_VALUE_CONSTANT, .val.constant = 1 };

            CodeList_append(&tacd_fn->body, incr);
            return dest;
        }
        case UNARY_POST_DECR: {
            const String *dest_name = create_temporary_var(cd);
            TacdValue dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = dest_name
            };

            TacdCode copy = {0};
            copy.kind = TACD_CODE_COPY;
            copy.code.copy.src = src;
            copy.code.copy.dest = dest;

            CodeList_append(&tacd_fn->body, copy);

            TacdCode incr = {0};
            incr.kind = TACD_CODE_BINARY;
            incr.code.binary.op = TACD_BINARY_SUBTRACT;
            incr.code.binary.dest = src;
            incr.code.binary.src1 = src;
            incr.code.binary.src2 = (TacdValue){ .kind = TACD_VALUE_CONSTANT, .val.constant = 1 };

            CodeList_append(&tacd_fn->body, incr);
            return dest;
        }
        }

        const String *dest_name = create_temporary_var(cd);
        TacdValue dest = (TacdValue){ .kind = TACD_VALUE_IDENTIFIER, .val.identifier = dest_name };

        unop.code.unary.src = src;
        unop.code.unary.dest = dest;
        CodeList_append(&tacd_fn->body, unop);

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
                jump_condition_label = create_label(cd, AND_FALSE, NULL);
                jump_conditional.kind = TACD_CODE_JUMP_IF_ZERO;
                copy_res.code.copy.src.val.constant = 1;
                end_label = create_label(cd, AND_END, NULL);
            } else if (expr->as.binary.op == BINARY_LOGICOR) {
                jump_condition_label = create_label(cd, OR_TRUE, NULL);
                jump_conditional.kind = TACD_CODE_JUMP_IF_NOT_ZERO;
                copy_res.code.copy.src.val.constant = 0;
                end_label = create_label(cd, OR_END, NULL);
            }

            jump_conditional.code.jump_conditional.condition = left_result;
            jump_conditional.code.jump_conditional.target = jump_condition_label;
            CodeList_append(&tacd_fn->body, jump_conditional);
            TacdValue right_result = trx_expression(cd, tacd_fn, expr->as.binary.right);
            jump_conditional.code.jump_conditional.condition = right_result;
            CodeList_append(&tacd_fn->body, jump_conditional);
            CodeList_append(&tacd_fn->body, copy_res);
            jump_end.code.jump = end_label;
            CodeList_append(&tacd_fn->body, jump_end);
            condition_label_code.code.label = jump_condition_label;
            CodeList_append(&tacd_fn->body, condition_label_code);

            if (expr->as.binary.op == BINARY_LOGICAND) {
                copy_res.code.copy.src.val.constant = 0;
            } else if (expr->as.binary.op == BINARY_LOGICOR) {
                copy_res.code.copy.src.val.constant = 1;
            }

            CodeList_append(&tacd_fn->body, copy_res);
            end_label_code.code.label = end_label;
            CodeList_append(&tacd_fn->body, end_label_code);

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
        CodeList_append(&tacd_fn->body, binop);

        return dest;
    }

    case EXPR_VAR: {
        TacdValue var = { .kind = TACD_VALUE_IDENTIFIER, .val.identifier = expr->as.var.name };
        return var;
    }

    case EXPR_ASSIGN: {
        TacdValue lhs = trx_expression(cd, tacd_fn, expr->as.assign.lhs);
        TacdValue rhs = trx_expression(cd, tacd_fn, expr->as.assign.rhs);
        TacdCode assign_copy = { .kind = TACD_CODE_COPY };
        assign_copy.code.copy.dest = lhs;
        switch (expr->as.assign.op) {
        case ASSIGN_INVALID:
            assign_copy.code.copy.src = (TacdValue){ .kind = TACD_VALUE_INVALID };
            break;
        case ASSIGN_SIMPLE: {
            assign_copy.code.copy.src = rhs;
            break;
        }
        case ASSIGN_SUM: {
            TacdCode pre_add = {0};
            pre_add.kind = TACD_CODE_BINARY;
            pre_add.code.binary.op = TACD_BINARY_ADD;
            TacdValue pre_add_dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            pre_add.code.binary.dest = pre_add_dest;
            pre_add.code.binary.src1 = lhs;
            pre_add.code.binary.src2 = rhs;
            CodeList_append(&tacd_fn->body, pre_add);
            assign_copy.code.copy.src = pre_add_dest;
            break;
        }
        case ASSIGN_DIFFERENCE: {
            TacdCode pre_sub = {0};
            pre_sub.kind = TACD_CODE_BINARY;
            pre_sub.code.binary.op = TACD_BINARY_SUBTRACT;
            TacdValue pre_sub_dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            pre_sub.code.binary.dest = pre_sub_dest;
            pre_sub.code.binary.src1 = lhs;
            pre_sub.code.binary.src2 = rhs;
            CodeList_append(&tacd_fn->body, pre_sub);
            assign_copy.code.copy.src = pre_sub_dest;
            break;
        }
        case ASSIGN_PRODUCT: {
            TacdCode pre_mult = {0};
            pre_mult.kind = TACD_CODE_BINARY;
            pre_mult.code.binary.op = TACD_BINARY_MULTIPLY;
            TacdValue pre_mult_dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            pre_mult.code.binary.dest = pre_mult_dest;
            pre_mult.code.binary.src1 = lhs;
            pre_mult.code.binary.src2 = rhs;
            CodeList_append(&tacd_fn->body, pre_mult);
            assign_copy.code.copy.src = pre_mult_dest;
            break;
        }
        case ASSIGN_QUOTIENT: {
            TacdCode pre_div = {0};
            pre_div.kind = TACD_CODE_BINARY;
            pre_div.code.binary.op = TACD_BINARY_DIVIDE;
            TacdValue pre_div_dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            pre_div.code.binary.dest = pre_div_dest;
            pre_div.code.binary.src1 = lhs;
            pre_div.code.binary.src2 = rhs;
            CodeList_append(&tacd_fn->body, pre_div);
            assign_copy.code.copy.src = pre_div_dest;
            break;
        }
        case ASSIGN_REMAINDER: {
            TacdCode pre_rem = {0};
            pre_rem.kind = TACD_CODE_BINARY;
            pre_rem.code.binary.op = TACD_BINARY_REMAINDER;
            TacdValue pre_rem_dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            pre_rem.code.binary.dest = pre_rem_dest;
            pre_rem.code.binary.src1 = lhs;
            pre_rem.code.binary.src2 = rhs;
            CodeList_append(&tacd_fn->body, pre_rem);
            assign_copy.code.copy.src = pre_rem_dest;
            break;
        }
        case ASSIGN_BITAND: {
            TacdCode pre_and = {0};
            pre_and.kind = TACD_CODE_BINARY;
            pre_and.code.binary.op = TACD_BINARY_BITAND;
            TacdValue pre_and_dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            pre_and.code.binary.dest = pre_and_dest;
            pre_and.code.binary.src1 = lhs;
            pre_and.code.binary.src2 = rhs;
            CodeList_append(&tacd_fn->body, pre_and);
            assign_copy.code.copy.src = pre_and_dest;
            break;
        }
        case ASSIGN_BITOR: {
            TacdCode pre_or = {0};
            pre_or.kind = TACD_CODE_BINARY;
            pre_or.code.binary.op = TACD_BINARY_BITOR;
            TacdValue pre_or_dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            pre_or.code.binary.dest = pre_or_dest;
            pre_or.code.binary.src1 = lhs;
            pre_or.code.binary.src2 = rhs;
            CodeList_append(&tacd_fn->body, pre_or);
            assign_copy.code.copy.src = pre_or_dest;
            break;
        }
        case ASSIGN_BITXOR: {
            TacdCode pre_xor = {0};
            pre_xor.kind = TACD_CODE_BINARY;
            pre_xor.code.binary.op = TACD_BINARY_BITXOR;
            TacdValue pre_xor_dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            pre_xor.code.binary.dest = pre_xor_dest;
            pre_xor.code.binary.src1 = lhs;
            pre_xor.code.binary.src2 = rhs;
            CodeList_append(&tacd_fn->body, pre_xor);
            assign_copy.code.copy.src = pre_xor_dest;
            break;
        }
        case ASSIGN_LSHFT: {
            TacdCode pre_lshft = {0};
            pre_lshft.kind = TACD_CODE_BINARY;
            pre_lshft.code.binary.op = TACD_BINARY_LSHFT;
            TacdValue pre_lshft_dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            pre_lshft.code.binary.dest = pre_lshft_dest;
            pre_lshft.code.binary.src1 = lhs;
            pre_lshft.code.binary.src2 = rhs;
            CodeList_append(&tacd_fn->body, pre_lshft);
            assign_copy.code.copy.src = pre_lshft_dest;
            break;
        }
        case ASSIGN_RSHFT: {
            TacdCode pre_rshft = {0};
            pre_rshft.kind = TACD_CODE_BINARY;
            pre_rshft.code.binary.op = TACD_BINARY_RSHFT;
            TacdValue pre_rshft_dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            pre_rshft.code.binary.dest = pre_rshft_dest;
            pre_rshft.code.binary.src1 = lhs;
            pre_rshft.code.binary.src2 = rhs;
            CodeList_append(&tacd_fn->body, pre_rshft);
            assign_copy.code.copy.src = pre_rshft_dest;
            break;
        }
        }

        CodeList_append(&tacd_fn->body, assign_copy);

        return lhs;
    }

    case EXPR_TERNARY: {
        TacdValue tern_res = (TacdValue){
            .kind = TACD_VALUE_IDENTIFIER,
            .val.identifier = create_temporary_var(cd)
        };
        TacdValue cond_res = trx_expression(cd, tacd_fn, expr->as.ternary.cond);
        TacdValue cond_dest = (TacdValue){
            .kind = TACD_VALUE_IDENTIFIER,
            .val.identifier = create_temporary_var(cd)
        };
        TacdCode cond_copy = (TacdCode){
            .kind = TACD_CODE_COPY,
            .code.copy = {
                .src = cond_res,
                .dest = cond_dest
            }
        };
        CodeList_append(&tacd_fn->body, cond_copy);
        const String *tern_else_lbl_txt = create_label(cd, TERN_ELSE, NULL);
        TacdCode tern_lbl = (TacdCode){
            .kind = TACD_CODE_LABEL,
            .code.label = tern_else_lbl_txt
        };
        const String *tern_end_lbl_txt = create_label(cd, TERN_END, NULL);
        TacdCode jz_else = (TacdCode){
            .kind = TACD_CODE_JUMP_IF_ZERO,
            .code.jump_conditional = { .condition = cond_dest, .target = tern_else_lbl_txt }
        };
        CodeList_append(&tacd_fn->body, jz_else);
        TacdValue then_res = trx_expression(cd, tacd_fn, expr->as.ternary.then_expr);
        TacdValue then_res_dest = (TacdValue){
            .kind = TACD_VALUE_IDENTIFIER,
            .val.identifier = create_temporary_var(cd)
        };
        TacdCode copy_res = (TacdCode){
            .kind = TACD_CODE_COPY,
            .code.copy = {
                .src = then_res,
                .dest = then_res_dest
            }
        };
        CodeList_append(&tacd_fn->body, copy_res);
        copy_res.code.copy.src = then_res_dest;
        copy_res.code.copy.dest = tern_res;
        CodeList_append(&tacd_fn->body, copy_res);
        TacdCode jmp_end = (TacdCode){
            .kind = TACD_CODE_JUMP,
            .code.jump = tern_end_lbl_txt
        };
        CodeList_append(&tacd_fn->body, jmp_end);
        CodeList_append(&tacd_fn->body, tern_lbl);
        tern_lbl.code.label = tern_end_lbl_txt;
        TacdValue else_res = trx_expression(cd, tacd_fn, expr->as.ternary.else_expr);
        TacdValue else_res_dest = (TacdValue){
            .kind = TACD_VALUE_IDENTIFIER,
            .val.identifier = create_temporary_var(cd)
        };
        copy_res.code.copy.src = else_res;
        copy_res.code.copy.dest = else_res_dest;
        CodeList_append(&tacd_fn->body, copy_res);
        copy_res.code.copy.src = else_res_dest;
        copy_res.code.copy.dest = tern_res;
        CodeList_append(&tacd_fn->body, copy_res);
        CodeList_append(&tacd_fn->body, tern_lbl);
        return tern_res;
        break;
    }

    case EXPR_FN_CALL:
        // TODO: generate code for fn call
        puts("TODO: EXPR_FN_CALL");
        break;

    case EXPR_INVALID: // Should err?
        break;
    }

    return (TacdValue){ .kind = TACD_VALUE_INVALID };
}

static void trx_statement(CompDriver *cd, TacdFunction *tacd_fn, Stmt *stmt) {
    switch(stmt->kind) {
    case STMT_RET: {
        TacdValue val = trx_expression(cd, tacd_fn, stmt->as.ret);
        TacdCode ret = {0};
        ret.kind = TACD_CODE_RET;
        ret.code.ret = val;
        CodeList_append(&tacd_fn->body, ret);
        break;
    }
    case STMT_EXPR: {
        trx_expression(cd, tacd_fn, stmt->as.expr);
        break;
    }
    case STMT_IF: {
        TacdValue cond_res = trx_expression(cd, tacd_fn, stmt->as.if_stmt.cond);
        TacdValue cond_dest = (TacdValue){
            .kind = TACD_VALUE_IDENTIFIER,
            .val.identifier = create_temporary_var(cd)
        };
        TacdCode cond_copy = (TacdCode){
            .kind = TACD_CODE_COPY,
            .code.copy = {
                .src = cond_res,
                .dest = cond_dest
            }
        };
        CodeList_append(&tacd_fn->body, cond_copy);

        const String *if_end_lbl_txt = create_label(cd, IF_END, NULL);
        TacdCode if_end_lbl = (TacdCode){
            .kind = TACD_CODE_LABEL,
            .code.label = if_end_lbl_txt
        };
        if (stmt->as.if_stmt.else_stmt == NULL) {
            TacdCode jz_end = (TacdCode){
                .kind = TACD_CODE_JUMP_IF_ZERO,
                .code.jump_conditional = {
                    .condition = cond_dest,
                    .target = if_end_lbl_txt
                }
            };
            CodeList_append(&tacd_fn->body, jz_end);
            trx_statement(cd, tacd_fn, stmt->as.if_stmt.then_stmt);
        } else {
            const String *if_else_lbl_txt = create_label(cd, IF_ELSE, NULL);
            TacdCode if_else_lbl = (TacdCode){
                .kind = TACD_CODE_LABEL,
                .code.label = if_else_lbl_txt
            };
            TacdCode jz_else = (TacdCode){
                .kind = TACD_CODE_JUMP_IF_ZERO,
                .code.jump_conditional = {
                    .condition = cond_dest,
                    .target = if_else_lbl_txt
                }
            };
            CodeList_append(&tacd_fn->body, jz_else);
            trx_statement(cd, tacd_fn, stmt->as.if_stmt.then_stmt);
            TacdCode jmp_end = (TacdCode){
                .kind = TACD_CODE_JUMP,
                .code.jump = if_end_lbl_txt
            };
            CodeList_append(&tacd_fn->body, jmp_end);
            CodeList_append(&tacd_fn->body, if_else_lbl);
            trx_statement(cd, tacd_fn, stmt->as.if_stmt.else_stmt);
        }

        CodeList_append(&tacd_fn->body, if_end_lbl);
        break;
    }

    case STMT_LABELED: {
        const String *stmt_lbl_txt = create_label(cd, STMT_LABEL, stmt->as.labeled_stmt.lbl);
        TacdCode stmt_lbl = (TacdCode){ .kind = TACD_CODE_LABEL, .code.label = stmt_lbl_txt };
        CodeList_append(&tacd_fn->body, stmt_lbl);
        trx_statement(cd, tacd_fn, stmt->as.labeled_stmt.stmt);
        break;
    }

    case STMT_GOTO: {
        const String *lbl_txt = create_label(cd, STMT_LABEL, stmt->as.goto_stmt);
        TacdCode goto_jmp = (TacdCode){ .kind = TACD_CODE_JUMP,
            .code.jump = lbl_txt
        };
        CodeList_append(&tacd_fn->body, goto_jmp);
        break;
    }

    case STMT_COMPOUND: {
        trx_block(cd, tacd_fn, stmt->as.compound_stmt);
        break;
    }

    case STMT_BREAK: {
        String lbl_txt = String_init_length(stmt->as.break_stmt->len + 6);
        snprintf(lbl_txt.cstr, lbl_txt.len+1, "break.%s", stmt->as.break_stmt->cstr);
        const String *canon_lbl = StrInterner_intern(&cd->str_table, lbl_txt);

        TacdCode break_jmp = (TacdCode){ .kind = TACD_CODE_JUMP, .code.jump = canon_lbl };
        CodeList_append(&tacd_fn->body, break_jmp);
        String_free(&lbl_txt);
        break;
    }

    case STMT_CONTINUE: {
        String lbl_txt = String_init_length(stmt->as.break_stmt->len + 9);
        snprintf(lbl_txt.cstr, lbl_txt.len+1, "continue.%s", stmt->as.break_stmt->cstr);
        const String *canon_lbl = StrInterner_intern(&cd->str_table, lbl_txt);

        TacdCode continue_jmp = (TacdCode){ .kind = TACD_CODE_JUMP, .code.jump = canon_lbl };
        CodeList_append(&tacd_fn->body, continue_jmp);
        String_free(&lbl_txt);
        break;
    }

    case STMT_DOWHILE: {
        TacdCode start_lbl = (TacdCode){
            .kind = TACD_CODE_LABEL,
            .code.label = stmt->as.do_while_stmt.lbl
        };
        CodeList_append(&tacd_fn->body, start_lbl);

        trx_statement(cd, tacd_fn, stmt->as.do_while_stmt.body);

        String continue_lbl_txt = String_init_length(stmt->as.do_while_stmt.lbl->len + 9);
        snprintf(continue_lbl_txt.cstr, continue_lbl_txt.len+1,
            "continue.%s", stmt->as.do_while_stmt.lbl->cstr);
        const String *canon_cont_lbl = StrInterner_intern(&cd->str_table, continue_lbl_txt);
        TacdCode continue_lbl = (TacdCode){ .kind = TACD_CODE_LABEL, .code.label = canon_cont_lbl };
        CodeList_append(&tacd_fn->body, continue_lbl);

        TacdValue cond_res = trx_expression(cd, tacd_fn, stmt->as.do_while_stmt.cond);
        TacdValue cond_dest = {
            .kind = TACD_VALUE_IDENTIFIER,
            .val.identifier = create_temporary_var(cd)
        };
        TacdCode copy_cond_res = (TacdCode){ .kind = TACD_CODE_COPY,
            .code.copy = {
                .dest = cond_dest,
                .src = cond_res
            }
        };
        CodeList_append(&tacd_fn->body, copy_cond_res);

        TacdCode start_jmp = (TacdCode){
            .kind = TACD_CODE_JUMP_IF_NOT_ZERO,
            .code.jump_conditional = {
                .condition = cond_dest,
                .target = start_lbl.code.label 
            }
        };
        CodeList_append(&tacd_fn->body, start_jmp);

        String break_lbl_txt = String_init_length(stmt->as.do_while_stmt.lbl->len + 6);
        snprintf(break_lbl_txt.cstr, break_lbl_txt.len+1,
            "break.%s", stmt->as.do_while_stmt.lbl->cstr);
        const String *canon_break_lbl = StrInterner_intern(&cd->str_table, break_lbl_txt);
        TacdCode break_lbl = (TacdCode){ .kind = TACD_CODE_LABEL, .code.label = canon_break_lbl };
        CodeList_append(&tacd_fn->body, break_lbl);

        String_free(&break_lbl_txt);
        String_free(&continue_lbl_txt);
        break;
    }

    case STMT_WHILE: {
        String continue_lbl_txt = String_init_length(stmt->as.while_stmt.lbl->len + 9);
        snprintf(continue_lbl_txt.cstr, continue_lbl_txt.len+1,
            "continue.%s", stmt->as.while_stmt.lbl->cstr);
        const String *canon_cont_lbl = StrInterner_intern(&cd->str_table, continue_lbl_txt);
        TacdCode continue_lbl = (TacdCode){ .kind = TACD_CODE_LABEL, .code.label = canon_cont_lbl };
        CodeList_append(&tacd_fn->body, continue_lbl);

        TacdValue cond_res = trx_expression(cd, tacd_fn, stmt->as.while_stmt.cond);

        TacdValue cond_dest = {
            .kind = TACD_VALUE_IDENTIFIER,
            .val.identifier = create_temporary_var(cd)
        };
        TacdCode copy_cond_res = (TacdCode){ .kind = TACD_CODE_COPY,
            .code.copy = {
                .dest = cond_dest,
                .src = cond_res
            }
        };
        CodeList_append(&tacd_fn->body, copy_cond_res);

        String break_lbl_txt = String_init_length(stmt->as.while_stmt.lbl->len + 6);
        snprintf(break_lbl_txt.cstr, break_lbl_txt.len+1,
            "break.%s", stmt->as.while_stmt.lbl->cstr);
        const String *canon_break_lbl = StrInterner_intern(&cd->str_table, break_lbl_txt);

        TacdCode end_jmp = (TacdCode){
            .kind = TACD_CODE_JUMP_IF_ZERO,
            .code.jump_conditional =  {
                .condition = cond_dest,
                .target = canon_break_lbl
            }
        };
        CodeList_append(&tacd_fn->body, end_jmp);

        trx_statement(cd, tacd_fn, stmt->as.while_stmt.body);

        TacdCode start_jmp = (TacdCode) {
            .kind = TACD_CODE_JUMP,
            .code.jump = canon_cont_lbl
        };
        CodeList_append(&tacd_fn->body, start_jmp);

        TacdCode break_lbl = (TacdCode){ .kind = TACD_CODE_LABEL, .code.label = canon_break_lbl };
        CodeList_append(&tacd_fn->body, break_lbl);

        String_free(&break_lbl_txt);
        String_free(&continue_lbl_txt);
        break;
    }

    case STMT_FOR: {
        if (stmt->as.for_stmt.init.kind == FOR_INIT_DECL) {
            trx_declaration(cd, tacd_fn, stmt->as.for_stmt.init.as.decl);
        } else if (stmt->as.for_stmt.init.kind == FOR_INIT_EXP) {
            trx_expression(cd, tacd_fn, stmt->as.for_stmt.init.as.exp);
        }

        TacdCode start_lbl = (TacdCode){
            .kind = TACD_CODE_LABEL,
            .code.label = stmt->as.for_stmt.lbl
        };
        CodeList_append(&tacd_fn->body, start_lbl);

        String break_lbl_txt = String_init_length(stmt->as.for_stmt.lbl->len + 6);
        snprintf(break_lbl_txt.cstr, break_lbl_txt.len+1,
            "break.%s", stmt->as.for_stmt.lbl->cstr);
        const String *canon_break_lbl = StrInterner_intern(&cd->str_table, break_lbl_txt);

        if (stmt->as.for_stmt.cond != NULL) {
            TacdValue cond_res = trx_expression(cd, tacd_fn, stmt->as.for_stmt.cond);
            TacdValue cond_dest = (TacdValue){
                .kind = TACD_VALUE_IDENTIFIER,
                .val.identifier = create_temporary_var(cd)
            };
            TacdCode copy_cond_res = (TacdCode){
                .kind = TACD_CODE_COPY,
                .code.copy = {
                    .dest = cond_dest,
                    .src = cond_res
                }
            };
            CodeList_append(&tacd_fn->body, copy_cond_res);

            TacdCode jmp_end = (TacdCode){
                .kind = TACD_CODE_JUMP_IF_ZERO,
                .code.jump_conditional = {
                    .condition = cond_dest,
                    .target = canon_break_lbl
                }
            };
            CodeList_append(&tacd_fn->body, jmp_end);
        }
        
        trx_statement(cd, tacd_fn, stmt->as.for_stmt.body);

        String continue_lbl_txt = String_init_length(stmt->as.for_stmt.lbl->len + 9);
        snprintf(continue_lbl_txt.cstr, continue_lbl_txt.len+1,
            "continue.%s", stmt->as.for_stmt.lbl->cstr);
        const String *canon_cont_lbl = StrInterner_intern(&cd->str_table, continue_lbl_txt);
        TacdCode continue_lbl = (TacdCode){ .kind = TACD_CODE_LABEL, .code.label = canon_cont_lbl };
        CodeList_append(&tacd_fn->body, continue_lbl);

        if (stmt->as.for_stmt.post != NULL) {
            trx_expression(cd, tacd_fn, stmt->as.for_stmt.post);
        }

        TacdCode start_jmp = (TacdCode){
            .kind = TACD_CODE_JUMP,
            .code.jump = stmt->as.for_stmt.lbl
        };
        CodeList_append(&tacd_fn->body, start_jmp);

        TacdCode break_lbl = (TacdCode){
            .kind = TACD_CODE_LABEL,
            .code.label = canon_break_lbl
        };
        CodeList_append(&tacd_fn->body, break_lbl);

        String_free(&continue_lbl_txt);
        String_free(&break_lbl_txt);
        break;
    }

    case STMT_SWITCH: {
        TacdValue ctrl_res_dest = (TacdValue){
            .kind = TACD_VALUE_IDENTIFIER,
            .val.identifier = create_temporary_var(cd)
        };
        TacdValue ctrl_val = trx_expression(cd, tacd_fn, stmt->as.switch_stmt.ctrl_expr);
        for (size_t c_idx = 0; c_idx < stmt->as.switch_stmt.cases->cap; c_idx++) {
            if (stmt->as.switch_stmt.cases->syms[c_idx].status == STE_OCCUPIED) {
                SymEntry *case_entry = &stmt->as.switch_stmt.cases->syms[c_idx];
                if (case_entry->type == SYMTYPE_CASE_LABEL) {
                    TacdValue case_val = (TacdValue){
                        .kind = TACD_VALUE_CONSTANT,
                        .val.constant = case_entry->as.case_lbl.val
                    };
                    TacdCode cmp_ctrl = (TacdCode){
                        .kind = TACD_CODE_BINARY,
                        .code.binary = {
                            .dest = ctrl_res_dest,
                            .src1 = ctrl_val,
                            .src2 = case_val,
                            .op = TACD_BINARY_EQUAL
                        }
                    };
                    TacdCode jmp_case = (TacdCode){
                        .kind = TACD_CODE_JUMP_IF_NOT_ZERO,
                        .code.jump_conditional = {
                            .condition = ctrl_res_dest,
                            .target = case_entry->key
                        }
                    };
                    CodeList_append(&tacd_fn->body, cmp_ctrl);
                    CodeList_append(&tacd_fn->body, jmp_case);
                }
            }
        }

        String swtch_brk_txt = String_init_length(stmt->as.switch_stmt.lbl->len + 6);
        snprintf(swtch_brk_txt.cstr, swtch_brk_txt.len+1,
            "break.%s", stmt->as.switch_stmt.lbl->cstr);
        const String *canon_break_lbl = StrInterner_intern(&cd->str_table, swtch_brk_txt);
        String_free(&swtch_brk_txt);

        String def_str = String_init_length(stmt->as.switch_stmt.lbl->len + 2);
        snprintf(def_str.cstr, def_str.len+1, "%s.d", stmt->as.switch_stmt.lbl->cstr);
        if (SymTable_contains(stmt->as.switch_stmt.cases, def_str.cstr, SYMTYPE_LABEL)) {
            SymEntry *def_entry = SymTable_get(stmt->as.switch_stmt.cases,
                def_str.cstr, SYMTYPE_LABEL);
            TacdCode jmp_def = (TacdCode){
                .kind = TACD_CODE_JUMP,
                .code.jump = def_entry->key
            };
            CodeList_append(&tacd_fn->body, jmp_def);
        } else {
            TacdCode jmp_end = (TacdCode){
                .kind = TACD_CODE_JUMP,
                .code.jump = canon_break_lbl
            };
            CodeList_append(&tacd_fn->body, jmp_end);
        }
        String_free(&def_str);

        trx_statement(cd, tacd_fn, stmt->as.switch_stmt.body);

        TacdCode end_lbl = (TacdCode){
            .kind = TACD_CODE_LABEL,
            .code.label = canon_break_lbl
        };
        CodeList_append(&tacd_fn->body, end_lbl);
        break;
    }

    case STMT_CASE: {
        TacdCode case_lbl = (TacdCode){
            .kind = TACD_CODE_LABEL,
            .code.label = stmt->as.case_stmt.lbl
        };
        CodeList_append(&tacd_fn->body, case_lbl);
        trx_statement(cd, tacd_fn, stmt->as.case_stmt.stmt);
        break;
    }

    case STMT_DEFAULT: {
        TacdCode default_lbl = (TacdCode){
            .kind = TACD_CODE_LABEL,
            .code.label = stmt->as.default_stmt.lbl
        };
        CodeList_append(&tacd_fn->body, default_lbl);
        trx_statement(cd, tacd_fn, stmt->as.default_stmt.stmt);
        break;
    }

    case STMT_NULL:
    case STMT_INVALID: // Should err?
        break;
    }
}

// TODO: eventually handle other declarations
static void trx_declaration(CompDriver *cd, TacdFunction *tacd_fn, Decl *decl) {
    if (decl->as.loc_var.init == NULL) return;

    TacdValue lhs = {
        .kind = TACD_VALUE_IDENTIFIER,
        .val.identifier = decl->as.loc_var.identifier
    };
    TacdValue rhs = trx_expression(cd, tacd_fn, decl->as.loc_var.init);
    TacdCode assign_copy = {0};
    assign_copy.kind = TACD_CODE_COPY;
    assign_copy.code.copy.dest = lhs;
    assign_copy.code.copy.src = rhs;
    CodeList_append(&tacd_fn->body, assign_copy);
}

static void trx_blockitem(CompDriver *cd, TacdFunction *tacd_fn, BlockItem *item) {
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

static void trx_block(CompDriver *cd, TacdFunction *tacd_fn, Block *block) {
    for (size_t b_idx = 0; b_idx < block->len; b_idx++) {
        trx_blockitem(cd, tacd_fn, &block->items[b_idx]);
    }
}

static void trx_fn_definition(CompDriver *cd, TacdFunction *tacd_fn, Decl *ast_fn) {
    cd->cgd.tacd_gen.func_name = ast_fn->as.fn.name;

    tacd_fn->name = ast_fn->as.fn.name;
    CodeList_init(&tacd_fn->body);

    if (ast_fn->as.fn.params != NULL && ast_fn->as.fn.params->len > 0) {
        tacd_fn->params = ParamArray_create();
        for (size_t p_idx = 0; p_idx < ast_fn->as.fn.params->len; p_idx++) {
            ParamArray_append(tacd_fn->params, ast_fn->as.fn.params->params[p_idx]);
        }
    } else {
        tacd_fn->params = NULL;
    }

    trx_block(cd, tacd_fn, ast_fn->as.fn.body);

    TacdCode implicit_ret = { .kind = TACD_CODE_RET };
    implicit_ret.code.ret.kind = TACD_VALUE_CONSTANT;
    implicit_ret.code.ret.val.constant = 0;
    CodeList_append(&tacd_fn->body, implicit_ret);
}

// TODO: trx_function no more... Program is list of declarations
void generate_tacd(CompDriver *cd, AstProgram *ast_prog) {
    TacdGenerator *tg = &cd->cgd.tacd_gen;

    tg->program = TacdProgram_create();
    for (size_t d_idx = 0; d_idx < ast_prog->decls.len; d_idx++) {
        Decl *fn_decl = ast_prog->decls.decls[d_idx];
        if (fn_decl->kind != DECL_FUNCTION) continue;
        if (fn_decl->as.fn.body == NULL) continue;

        TacdFunction tacd_fn = {0};
        trx_fn_definition(cd, &tacd_fn, fn_decl);
        TacdFunctionArray_append(&tg->program->fn_defs, tacd_fn);
    }
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

    case TACD_CODE_FN_CALL:
        printf("%2$*1$s\n", spaces+13, "TODO: FN CALL");
        break;
    }
}

static void TacdFunction_print(TacdFunction *fn, int indent_lvl) {
    int spaces = indent_lvl * 2;
    printf("%2$*1$s %3$s",
        spaces+2,
        "fn", fn->name->cstr);
    if (fn->params != NULL) {
        printf("(");
        for (size_t p_idx = 0; p_idx < fn->params->len; p_idx++) {
            printf("%s%s", fn->params->params[p_idx]->cstr, p_idx+1 < fn->params->len ? "," : "");
        }
        printf(")");
    }
    printf(":\n");
    CodeList *instructions = &fn->body;
    indent_lvl += 1;
    for (size_t instr_idx = 0; instr_idx < instructions->len; instr_idx++) {
        TacdCode_print(&instructions->codes[instr_idx], indent_lvl);
    }
    indent_lvl -= 1;
}

static void TacdProgram_print(TacdProgram *prog, int indent_lvl) {
    int spaces = indent_lvl * 4;
    printf("%2$*1$s\n", spaces+8, "Program:");
    for (size_t d_idx = 0; d_idx < prog->fn_defs.len; d_idx++) {
        TacdFunction_print(&prog->fn_defs.fns[d_idx], indent_lvl);
    }
}

void Tacd_print(TacdProgram *prog) {
    puts("Generated TACD\n==============");
    TacdProgram_print(prog, 0);
}
