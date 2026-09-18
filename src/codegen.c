#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "comp_driver.h"
#include "dd_string.h"
#include "tacd.h"

#include "codegen.h"

/*
   InstrArray
*/

void InstrArray_init(InstrArray *ia) {
    ia->cap = 2;
    ia->len = 0;
    ia->instrs = (AsmInstr *)calloc(ia->cap, sizeof(AsmInstr));
}

void InstrArray_deinit(InstrArray *ia) {
    if (ia == NULL) return;
    if (ia->instrs == NULL) return;

    free(ia->instrs);
    ia->cap = 0;
    ia->len = 0;
}

void InstrArray_insert(InstrArray *ia, AsmInstr in, size_t idx) {
    if (ia == NULL) return;
    if (ia->instrs == NULL) return;

    if (ia->len == ia->cap) {
        size_t old_cap = ia->cap;
        size_t new_cap = (old_cap == 0) ? 2 : old_cap / 2 + old_cap;
        AsmInstr *new_instrs = realloc(ia->instrs, new_cap*sizeof(AsmInstr));

        if (new_instrs == NULL) return;

        ia->instrs = new_instrs;
        ia->cap = new_cap;
    }

    if (idx < ia->len) {
        memmove(ia->instrs+idx+1, ia->instrs+idx, (ia->len - idx) * sizeof(AsmInstr));
    }

    ia->instrs[idx] = in;
    ia->len += 1;
}

void InstrArray_append(InstrArray *ia, AsmInstr in) {
    if (ia == NULL) return;
    if (ia->instrs == NULL) return;

    if (ia->len == ia->cap) {
        size_t old_cap = ia->cap;
        size_t new_cap = old_cap / 2 + old_cap;
        AsmInstr *new_instrs = calloc(new_cap, sizeof(AsmInstr));
        memcpy(new_instrs, ia->instrs, old_cap*sizeof(AsmInstr));
        free(ia->instrs);
        ia->instrs = new_instrs;
        ia->cap = new_cap;
    }

    memcpy(&ia->instrs[ia->len], &in, sizeof(AsmInstr));
    ia->len += 1;
}

/*
   PseudoSymMap
*/

void PseudoSymMap_init(PseudoSymMap *map) {
    map->cap = 2;
    map->len = 0;
    map->data = (PseudoStackMapping *)calloc(map->cap, sizeof(PseudoStackMapping));
}

void PseudoSymMap_deinit(PseudoSymMap *map) {
    if (map == NULL) return;
    if (map->data == NULL) return;

    for (size_t mapping_idx = 0; mapping_idx < map->len; mapping_idx++) {
        String_free(&map->data[mapping_idx].ident);
    }

    free(map->data);
}

void PseudoSymMap_append(PseudoSymMap *map, PseudoStackMapping item) {
    if (map == NULL) return;
    if (map->data == NULL) return;

    if (map->len == map->cap) {
        size_t old_cap = map->cap;
        size_t new_cap = old_cap / 2 + old_cap;
        PseudoStackMapping *new_data = calloc(new_cap, sizeof(PseudoStackMapping));
        memcpy(new_data, map->data, old_cap*sizeof(PseudoStackMapping));
        free(map->data);
        map->data = new_data;
        map->cap = new_cap;
    }

    memcpy(&map->data[map->len], &item, sizeof(PseudoStackMapping));
    map->len += 1;
}

/* Search `map` for `key`.
   Returns true If `key` is found, and sets `val` to the value saved at `key`.
   Else, returns false and `val` is not set. */
bool PseudoSymMap_contains(PseudoSymMap *map, String key, int *val) {
    if (map == NULL) return false;
    if (map->data == NULL || map->len <= 0) return false;
    if (val == NULL) return false;

    for (size_t mapping_idx = 0; mapping_idx < map->len; mapping_idx++) {
        if (strcmp(map->data[mapping_idx].ident.cstr, key.cstr) == 0) {
            *val = map->data[mapping_idx].stack_offset;
            return true;
        }
    }

    return false;
}

void AsmFnArray_init(AsmFnArray *afa) {
    if (afa == NULL) return;

    afa->len = 0;
    afa->cap = 2;
    afa->fns = calloc(afa->cap, sizeof(AsmFn));
}

void AsmFnArray_deinit(AsmFnArray *afa) {
    if (afa == NULL) return;

    for (size_t fn_idx = 0; fn_idx < afa->len; fn_idx++) {
        InstrArray_deinit(&afa->fns[fn_idx].instrs);
    }

    free(afa->fns);
    afa->fns = NULL;
    afa->cap = 0;
    afa->len = 0;
}

void AsmFnArray_append(AsmFnArray *afa, AsmFn afn) {
    if (afa == NULL) return;
    if (afa->fns == NULL) return;

    if (afa->len == afa->cap) {
        size_t old_cap = afa->cap;
        size_t new_cap = old_cap / 2 + old_cap;
        AsmFn *new_fns = calloc(new_cap, sizeof(AsmFn));
        memcpy(new_fns, afa->fns, old_cap*sizeof(AsmFn));
        free(afa->fns);
        afa->fns = new_fns;
        afa->cap = new_cap;
    }

    memcpy(&afa->fns[afa->len], &afn, sizeof(AsmFn));
    afa->len += 1;
}

AsmProgram *AsmProgram_create(void) {
    AsmProgram *prog = malloc(sizeof(AsmProgram));
    *prog = (AsmProgram){0};
    AsmFnArray_init(&prog->fns);
    return prog;
}

void AsmProgram_destroy(AsmProgram *prog) {
    if (prog == NULL) return;

    AsmFnArray_deinit(&prog->fns);
    free(prog);
    prog = NULL;
    //switch (node->kind) {
    //case ASMNODE_PROGRAM:
    //    AsmNode_destroy(node->node.program.function);
    //    free(node);
    //    break;

    //case ASMNODE_FUNCTION:
    //    AsmNode_function_destroy(node);
    //    break;
    //}
}

/*
   CodegenDriver
*/

void CodegenDriver_init(CodegenDriver *cgd) {
    TacdGenerator_init(&cgd->tacd_gen);
    PseudoSymMap_init(&cgd->stack_offsets);
}

void CodegenDriver_deinit(CodegenDriver *cgd) {
    TacdGenerator_deinit(&cgd->tacd_gen);
    PseudoSymMap_deinit(&cgd->stack_offsets);
    AsmProgram_destroy(cgd->program);
    String_free(&cgd->dest);
}


/*
    trx_*   (TACD->ASM)
*/

static Operand trx_operand(TacdValue val) {
    switch (val.kind) {
    case TACD_VALUE_INVALID:
        /*  TODO: should probably be an internal compiler error.    */
        break;

    case TACD_VALUE_CONSTANT:
        return (Operand){ .type = OPERAND_IMM, .val.imm = val.val.constant };

    case TACD_VALUE_IDENTIFIER:
        return (Operand){ .type = OPERAND_PSEUDO, .val.pseudo = val.val.identifier };
    }

    return (Operand){ .type = OPERAND_INVALID };
}

static UnaryOp trx_unary_op(TacdUnaryOp op) {
    switch (op) {
    case TACD_UNARY_COMPLEMENT:
        return UNARYOP_NOT;
    case TACD_UNARY_NEGATE:
        return UNARYOP_NEG;
    case TACD_UNARY_INVALID:
        return UNARYOP_INVALID;
    case TACD_UNARY_NOT:
        return UNARYOP_COND_NOT;
    }
}

static BinaryOp trx_binary_op(TacdBinaryOp op) {
    switch (op) {
    case TACD_BINARY_INVALID:
        return BINARYOP_INVALID;
    case TACD_BINARY_ADD:
        return BINARYOP_ADD;
    case TACD_BINARY_SUBTRACT:
        return BINARYOP_SUB;
    case TACD_BINARY_MULTIPLY:
        return BINARYOP_MULT;
    case TACD_BINARY_BITAND:
        return BINARYOP_BITAND;
    case TACD_BINARY_BITOR:
        return BINARYOP_BITOR;
    case TACD_BINARY_BITXOR:
        return BINARYOP_BITXOR;
    case TACD_BINARY_LSHFT:
        return BINARYOP_LSHFT;
    case TACD_BINARY_RSHFT:
        return BINARYOP_RSHFT;
    case TACD_BINARY_EQUAL:
        return BINARYOP_EQUAL;
    case TACD_BINARY_NOT_EQUAL:
        return BINARYOP_NOT_EQUAL;
    case TACD_BINARY_LT:
        return BINARYOP_LT;
    case TACD_BINARY_LTE:
        return BINARYOP_LTE;
    case TACD_BINARY_GT:
        return BINARYOP_GT;
    case TACD_BINARY_GTE:
        return BINARYOP_GTE;
    default:
        return BINARYOP_INVALID;
    }
}

static AsmInstr trx_binary_divide(AsmFn *asm_fn, TacdCode *tacd_code) {
    Operand op_src1 = trx_operand(tacd_code->code.binary.src1);
    Operand op_src2 = trx_operand(tacd_code->code.binary.src2);
    Operand op_dest = trx_operand(tacd_code->code.binary.dest);

    Operand op_ax = (Operand){
        .type = OPERAND_REG,
        .val.reg = AX
    };

    AsmInstr mov_src1_ax = {0};
    mov_src1_ax.kind = ASM_INSTR_MOV;
    mov_src1_ax.instr.mov.src = op_src1;
    mov_src1_ax.instr.mov.dest = op_ax;
    InstrArray_append(&asm_fn->instrs, mov_src1_ax);

    AsmInstr cdq = {0};
    cdq.kind = ASM_INSTR_CDQ;
    InstrArray_append(&asm_fn->instrs, cdq);

    AsmInstr asm_idiv = {0};
    asm_idiv.kind = ASM_INSTR_IDIV;
    asm_idiv.instr.idiv.divisor = op_src2;
    InstrArray_append(&asm_fn->instrs, asm_idiv);

    AsmInstr mov_ax_dest = {0};
    mov_ax_dest.kind = ASM_INSTR_MOV;
    mov_ax_dest.instr.mov.src = op_ax;
    mov_ax_dest.instr.mov.dest = op_dest;

    return mov_ax_dest;
}

static AsmInstr trx_binary_remainder(AsmFn *asm_fn, TacdCode *tacd_code) {
    Operand op_src1 = trx_operand(tacd_code->code.binary.src1);
    Operand op_src2 = trx_operand(tacd_code->code.binary.src2);
    Operand op_dest = trx_operand(tacd_code->code.binary.dest);


    AsmInstr mov_src1_ax = {0};
    mov_src1_ax.kind = ASM_INSTR_MOV;
    mov_src1_ax.instr.mov.src = op_src1;
    mov_src1_ax.instr.mov.dest = (Operand){ .type = OPERAND_REG, .val.reg = AX };
    InstrArray_append(&asm_fn->instrs, mov_src1_ax);

    AsmInstr cdq = {0};
    cdq.kind = ASM_INSTR_CDQ;
    InstrArray_append(&asm_fn->instrs, cdq);

    AsmInstr asm_idiv = {0};
    asm_idiv.kind = ASM_INSTR_IDIV;
    asm_idiv.instr.idiv.divisor = op_src2;
    InstrArray_append(&asm_fn->instrs, asm_idiv);

    AsmInstr mov_dx_dest = {0};
    mov_dx_dest.kind = ASM_INSTR_MOV;
    mov_dx_dest.instr.mov.src = (Operand){ .type = OPERAND_REG, .val.reg = DX };
    mov_dx_dest.instr.mov.dest = op_dest;

    return mov_dx_dest;
}

static ConditionCode trx_binary_cond(BinaryOp cond_op) {
    switch (cond_op) {
    case BINARYOP_EQUAL: return CC_E;
    case BINARYOP_NOT_EQUAL: return CC_NE;
    case BINARYOP_LT: return CC_L;
    case BINARYOP_LTE: return CC_LE;
    case BINARYOP_GT: return CC_G;
    case BINARYOP_GTE: return CC_GE;
    default: return CC_INVALID;
    }
}

static bool is_conditional_op(BinaryOp cond_op) {
    switch (cond_op) {
    case BINARYOP_EQUAL:
    case BINARYOP_NOT_EQUAL:
    case BINARYOP_LT:
    case BINARYOP_LTE:
    case BINARYOP_GT:
    case BINARYOP_GTE:
        return true;
    default:
        return false;
    }
}

static AsmInstr trx_code(AsmFn *asm_fn, TacdCode *tacd_code) {
    switch (tacd_code->kind) {
    case TACD_CODE_INVALID:
        /*  TODO: should be an internal compiler error. */
        break;

    case TACD_CODE_UNARY: {
        UnaryOp unop = trx_unary_op(tacd_code->code.unary.op);
        Operand op_src = trx_operand(tacd_code->code.unary.src);
        Operand op_dest = trx_operand(tacd_code->code.unary.dest);

        AsmInstr asm_mov = {0};
        asm_mov.kind = ASM_INSTR_MOV;
        if (unop == UNARYOP_COND_NOT) {
            AsmInstr cmp = { .kind = ASM_INSTR_CMP };
            cmp.instr.cmp.src1 = (Operand){ .type = OPERAND_IMM, .val.imm = 0 };
            cmp.instr.cmp.src2 = op_src;
            InstrArray_append(&asm_fn->instrs, cmp);
            asm_mov.instr.mov.src = (Operand){ .type = OPERAND_IMM, .val.imm = 0 };
        } else {
            asm_mov.instr.mov.src = op_src;
        }
        asm_mov.instr.mov.dest = op_dest;
        InstrArray_append(&asm_fn->instrs, asm_mov);

        AsmInstr asm_unary = {0};
        if (unop == UNARYOP_COND_NOT) {
            asm_unary.kind = ASM_INSTR_SETCC;
            asm_unary.instr.setcc.cond_code = CC_E;
            asm_unary.instr.setcc.dest = op_dest;
        } else {
            asm_unary.kind = ASM_INSTR_UNARY;
            asm_unary.instr.unary.unop = unop;
            asm_unary.instr.unary.op = op_dest;
        }

        return asm_unary;
    }

    case TACD_CODE_BINARY: {
        // Division-like instructions
        if (tacd_code->code.binary.op == TACD_BINARY_DIVIDE) {
            return trx_binary_divide(asm_fn, tacd_code);
        } else if (tacd_code->code.binary.op == TACD_BINARY_REMAINDER) {
            return trx_binary_remainder(asm_fn, tacd_code);
        }

        BinaryOp binop = trx_binary_op(tacd_code->code.binary.op);
        Operand op_src1 = trx_operand(tacd_code->code.binary.src1);
        Operand op_src2 = trx_operand(tacd_code->code.binary.src2);
        Operand op_dest = trx_operand(tacd_code->code.binary.dest);

        // conditional instructions
        if (is_conditional_op(binop)) {
            ConditionCode cond_code = trx_binary_cond(binop);
            AsmInstr cmp = { .kind = ASM_INSTR_CMP };
            cmp.instr.cmp.src1 = op_src2;
            cmp.instr.cmp.src2 = op_src1;
            InstrArray_append(&asm_fn->instrs, cmp);
            AsmInstr mov_zero = { .kind = ASM_INSTR_MOV };
            mov_zero.instr.mov.src = (Operand){ .type = OPERAND_IMM, .val.imm = 0 };
            mov_zero.instr.mov.dest = op_dest;
            InstrArray_append(&asm_fn->instrs, mov_zero);
            AsmInstr setcc = { .kind = ASM_INSTR_SETCC };
            setcc.instr.setcc.cond_code = cond_code;
            setcc.instr.setcc.dest = op_dest;
            return setcc;
        }

        // Add, Subtract, multiply instructions
        // Bitwise-AND, -OR, -XOR instructions

        AsmInstr mov_src_dest = {0};
        mov_src_dest.kind = ASM_INSTR_MOV;
        mov_src_dest.instr.mov.src = op_src1;
        mov_src_dest.instr.mov.dest = op_dest;
        InstrArray_append(&asm_fn->instrs, mov_src_dest);

        AsmInstr asm_binary = {0};
        asm_binary.kind = ASM_INSTR_BINARY;
        asm_binary.instr.binary.binop = binop;
        asm_binary.instr.binary.src = op_src2;
        asm_binary.instr.binary.dest = op_dest;

        return asm_binary;
    }

    case TACD_CODE_COPY: {
        Operand src = trx_operand(tacd_code->code.copy.src);
        Operand dest = trx_operand(tacd_code->code.copy.dest);
        AsmInstr cpy_mov = { 0 };
        cpy_mov.kind = ASM_INSTR_MOV;
        cpy_mov.instr.mov.src = src;
        cpy_mov.instr.mov.dest = dest;
        return cpy_mov;
    }

    case TACD_CODE_JUMP: {
        AsmInstr jmp = (AsmInstr){ .kind = ASM_INSTR_JMP, .instr.jmp = tacd_code->code.jump };
        return jmp;
    }

    case TACD_CODE_JUMP_IF_ZERO: {
        Operand cond = trx_operand(tacd_code->code.jump_conditional.condition);
        AsmInstr comp = (AsmInstr){ .kind = ASM_INSTR_CMP };
        comp.instr.cmp.src1 = (Operand){ .type = OPERAND_IMM, .val.imm = 0 };
        comp.instr.cmp.src2 = cond;
        InstrArray_append(&asm_fn->instrs, comp);

        AsmInstr jmp = (AsmInstr){ .kind = ASM_INSTR_JMPCC };
        jmp.instr.jmpcc.cond_code = CC_E;
        jmp.instr.jmpcc.target = tacd_code->code.jump_conditional.target;

        return jmp;
    }

    case TACD_CODE_JUMP_IF_NOT_ZERO: {
        Operand cond = trx_operand(tacd_code->code.jump_conditional.condition);
        AsmInstr comp = (AsmInstr){ .kind = ASM_INSTR_CMP };
        comp.instr.cmp.src1 = (Operand){ .type = OPERAND_IMM, .val.imm = 0 };
        comp.instr.cmp.src2 = cond;
        InstrArray_append(&asm_fn->instrs, comp);

        AsmInstr jmp = (AsmInstr){ .kind = ASM_INSTR_JMPCC };
        jmp.instr.jmpcc.cond_code = CC_NE;
        jmp.instr.jmpcc.target = tacd_code->code.jump_conditional.target;

        return jmp;
    }

    case TACD_CODE_LABEL: {
        AsmInstr lbl = (AsmInstr){ .kind = ASM_INSTR_LABEL, .instr.label = tacd_code->code.label };
        return lbl;
    }

    case TACD_CODE_RET: {
        Operand op_src = trx_operand(tacd_code->code.ret);

        // Create the MOV instruction.
        AsmInstr asm_mov = {0};
        asm_mov.kind = ASM_INSTR_MOV;
        asm_mov.instr.mov.src = op_src;
        asm_mov.instr.mov.dest = (Operand){ .type = OPERAND_REG, .val.reg = AX };

        // Add generated code to asm_function.
        InstrArray_append(&asm_fn->instrs, asm_mov);

        AsmInstr asm_ret = { .kind = ASM_INSTR_RET };
        return asm_ret;
    }

    case TACD_CODE_FN_CALL: {
        // Get the args
        ValueArray *va = tacd_code->code.fn_call.args == NULL ? NULL : tacd_code->code.fn_call.args;
        Operand reg_ax = (Operand){ .type = OPERAND_REG, .val.reg = AX };
        int reg_arg_count = 0;
        int stack_arg_count = 0;
        int padding = 0;
        if (va != NULL) {
            // Args exist
            // Determine count of reg args, stack args, and padding (if needed)
            if (va->len > 6) {
                reg_arg_count = 6;
                stack_arg_count = va->len - 6;
            } else {
                reg_arg_count = va->len;
            }
            padding = stack_arg_count % 2 != 0 ? 8 : 0;
            printf("Reg args: %d\nStack args: %d\n", reg_arg_count, stack_arg_count);

            if (padding != 0) {
                AsmInstr alloc_padding = (AsmInstr){
                    .kind = ASM_ALLOCSTACK,
                    .instr.alloc_stack = padding
                };
                InstrArray_append(&asm_fn->instrs, alloc_padding);
            }

            // Move args to regs and stack
            size_t tacd_arg_idx = 0; // will remain on last arg to push to stack
            for (size_t a_idx = 0; a_idx < reg_arg_count; a_idx++, tacd_arg_idx++) {
                Operand arg = trx_operand(va->vals[tacd_arg_idx]);
                AsmInstr mov_arg = (AsmInstr){
                    .kind = ASM_INSTR_MOV,
                    .instr.mov = {
                        .src = arg,
                        .dest = (Operand){
                            .type = OPERAND_REG,
                            .val.reg = param_regs[a_idx]
                        }
                    }
                };
                InstrArray_append(&asm_fn->instrs, mov_arg);
            }

            for (size_t a_idx = va->len-1; a_idx >= tacd_arg_idx; a_idx--) {
                Operand arg = trx_operand(va->vals[a_idx]);
                if (arg.type == OPERAND_REG || arg.type == OPERAND_IMM) {
                    AsmInstr push_arg = (AsmInstr){
                        .kind = ASM_INSTR_PUSH,
                        .instr.push = arg
                    };
                    InstrArray_append(&asm_fn->instrs, push_arg);
                } else {
                    AsmInstr mov_arg = (AsmInstr){
                        .kind = ASM_INSTR_MOV,
                        .instr.mov = {
                            .src = arg,
                            .dest = reg_ax
                        }
                    };
                    InstrArray_append(&asm_fn->instrs, mov_arg);
                    AsmInstr push_arg = (AsmInstr){
                        .kind = ASM_INSTR_PUSH,
                        .instr.push = reg_ax
                    };
                    InstrArray_append(&asm_fn->instrs, push_arg);
                }
            }
        }

        AsmInstr call = (AsmInstr){
            .kind = ASM_INSTR_CALL,
            .instr.call = tacd_code->code.fn_call.name
        };
        InstrArray_append(&asm_fn->instrs, call);

        int dealloc_size = 8 * stack_arg_count + padding;
        if (dealloc_size > 0) {
            AsmInstr dealloc = (AsmInstr){
                .kind = ASM_DEALLOCSTACK,
                .instr.dealloc_stack = dealloc_size
            };
            InstrArray_append(&asm_fn->instrs, dealloc);
        }

        Operand dest = trx_operand(tacd_code->code.fn_call.dest);
        AsmInstr get_res = (AsmInstr){
            .kind = ASM_INSTR_MOV,
            .instr.mov = {
                .src = reg_ax,
                .dest = dest
            }
        };

        return get_res;
    }
    }

    return (AsmInstr){ .kind = ASM_INSTR_INVALID };
}

static void mov_params_to_stack(AsmFn *asm_fn, ParamArray *tacd_params) {
    int reg_arg_count = 0;
    int stack_arg_count = 0;
    if (tacd_params->len > 6) {
        reg_arg_count = 6;
        stack_arg_count = tacd_params->len - 6;
    } else {
        reg_arg_count = tacd_params->len;
    }
    for (size_t p_idx = 0; p_idx < 6 && p_idx < tacd_params->len; p_idx++) {
        AsmInstr param_mov = (AsmInstr){
            .kind = ASM_INSTR_MOV,
            .instr.mov.src = (Operand){
                .type = OPERAND_REG,
                .val.reg = param_regs[p_idx]
            },
            .instr.mov.dest = (Operand){
                .type = OPERAND_PSEUDO,
                .val.pseudo = tacd_params->params[p_idx]
            }
        };
        InstrArray_append(&asm_fn->instrs, param_mov);
    }

    int mem_offset = 16; // for function stack frame, +8 -> return address, +16 -> first stack arg
    for (size_t p_idx = 6; p_idx < tacd_params->len; p_idx++, mem_offset += 8) {
        AsmInstr param_mov = (AsmInstr){
            .kind = ASM_INSTR_MOV,
            .instr.mov = {
                .src = (Operand){
                    .type = OPERAND_STACK,
                    .val.stack = mem_offset
                },
                .dest = (Operand){
                    .type = OPERAND_PSEUDO,
                    .val.pseudo = tacd_params->params[p_idx]
                }
            }
        };
        InstrArray_append(&asm_fn->instrs, param_mov);
    }
}

static AsmFn trx_function(TacdFunction *tacd_fn) {
    if (tacd_fn == NULL) return (AsmFn){0};

    AsmFn asm_fn = (AsmFn){0};
    asm_fn.name = tacd_fn->name;
    InstrArray_init(&asm_fn.instrs);

    if (tacd_fn->params != NULL) {
        mov_params_to_stack(&asm_fn, tacd_fn->params);
    }

    CodeList func_body = tacd_fn->body;
    for (size_t code_idx = 0; code_idx < func_body.len; code_idx++) {
        AsmInstr instr = trx_code(&asm_fn, &func_body.codes[code_idx]);
        // TODO: check if instr is ASM_INSTR_INVALID
        InstrArray_append(&asm_fn.instrs, instr);
    }
    //AsmInstr asm_ret = trx_ret(ast_function->node.function.statement, asm_function);

    return asm_fn;
}

static AsmProgram *trx_program(TacdProgram *tacd_program) {
    /*  TODO: these checks should result in internal compiler error.    */
    if (tacd_program == NULL) return NULL;

    AsmProgram *prog = AsmProgram_create();
    // TODO: Now a list of TacdFunction
    //prog->node.program.function = trx_function(tacd_program->node.program.function);
    for (size_t fn_idx = 0; fn_idx < tacd_program->fn_defs.len; fn_idx++) {
        TacdFunction *tacd_fn = &tacd_program->fn_defs.fns[fn_idx];
        AsmFn asm_fn = trx_function(tacd_fn);
        AsmFnArray_append(&prog->fns, asm_fn);
    }

    return prog;
}

static void resolve_function_stack(CodegenDriver *cgd, AsmFn *asm_fn, int resolved_offset) {
    int offset_remainder = resolved_offset % 16;
    int new_offset =
        offset_remainder == 0 ? offset_remainder : resolved_offset + (16 - offset_remainder);
    AsmInstr stack_alloc = (AsmInstr){
        .kind = ASM_ALLOCSTACK,
        .instr.alloc_stack = new_offset
    };
    InstrArray_insert(&asm_fn->instrs, stack_alloc, 0);
}

//static void resolve_invalid_mov(InstrArray *ia){}

static void resolve_invalid_instructions(CodegenDriver *cgd, AsmFn *asm_fn) {
    InstrArray *func_instrs = &asm_fn->instrs;
    for (size_t instr_idx = 0; instr_idx < func_instrs->len; instr_idx++) {
        AsmInstr *instr = &func_instrs->instrs[instr_idx];
        switch (instr->kind) {
        case ASM_INSTR_INVALID:
        case ASM_INSTR_UNARY:
        case ASM_ALLOCSTACK:
        case ASM_INSTR_CDQ:
        case ASM_INSTR_RET:
                break;

        case ASM_INSTR_MOV: {
            if (instr->instr.mov.src.type == OPERAND_STACK && instr->instr.mov.dest.type == OPERAND_STACK) {
                int src = instr->instr.mov.src.val.stack;
                instr->instr.mov.src.type = OPERAND_REG;
                instr->instr.mov.src.val.reg = R10;
                AsmInstr new_instr = (AsmInstr){
                    .kind = ASM_INSTR_MOV,
                    .instr.mov = {
                        .src = (Operand){ .type = OPERAND_STACK, .val.stack = src },
                        .dest = (Operand){ .type = OPERAND_REG, .val.reg = R10 },
                    }};
                InstrArray_insert(func_instrs, new_instr, instr_idx);
            }
            break;
        }   // case ASM_INSTR_MOV

        case ASM_INSTR_IDIV: {
            if (instr->instr.idiv.divisor.type == OPERAND_IMM) {
                int old_imm = instr->instr.idiv.divisor.val.imm;
                instr->instr.idiv.divisor.type = OPERAND_REG;
                instr->instr.idiv.divisor.val.reg = R10;
                AsmInstr new_instr = (AsmInstr){
                    .kind = ASM_INSTR_MOV,
                    .instr.mov = {
                        .src = (Operand){ .type = OPERAND_IMM, .val.imm = old_imm },
                        .dest = (Operand){ .type = OPERAND_REG, .val.reg = R10 }
                    }
                };
                InstrArray_insert(func_instrs, new_instr, instr_idx);
            }
            break;
        }   // case ASM_INSTR_IDIV

        case ASM_INSTR_BINARY: {
            switch (instr->instr.binary.binop) {
            case BINARYOP_INVALID:
                    break;
            case BINARYOP_ADD:
            case BINARYOP_SUB:
            case BINARYOP_BITAND:
            case BINARYOP_BITOR:
            case BINARYOP_BITXOR: {
                if (instr->instr.binary.src.type == OPERAND_STACK && instr->instr.binary.dest.type == OPERAND_STACK) {
                    int old_src = instr->instr.binary.src.val.stack;
                    instr->instr.binary.src.type = OPERAND_REG;
                    instr->instr.binary.src.val.reg = R10;
                    AsmInstr new_instr = (AsmInstr){
                        .kind = ASM_INSTR_MOV,
                        .instr.mov = {
                            .src = (Operand){ .type = OPERAND_STACK, .val.stack = old_src },
                            .dest = (Operand){ .type = OPERAND_REG, .val.reg = R10 }
                        }};
                    InstrArray_insert(func_instrs, new_instr, instr_idx);
                }
                break;
            }   // case BINARYOP_ADD/SUB/BITAND/BITOR/BITXOR

            case BINARYOP_MULT: {
                if (instr->instr.binary.dest.type == OPERAND_STACK) {
                    int dest = instr->instr.binary.dest.val.stack;
                    instr->instr.binary.dest.type = OPERAND_REG;
                    instr->instr.binary.dest.val.reg = R11;

                    AsmInstr first_mov = (AsmInstr){
                        .kind = ASM_INSTR_MOV,
                        .instr.mov = {
                            .src = (Operand){ .type = OPERAND_STACK, .val.stack = dest },
                            .dest = (Operand){ .type = OPERAND_REG, .val.reg = R11 }
                        }
                    };
                    AsmInstr second_mov = (AsmInstr){
                        .kind = ASM_INSTR_MOV,
                        .instr.mov = {
                            .src = (Operand){ .type = OPERAND_REG, .val.reg = R11 },
                            .dest = (Operand){ .type = OPERAND_STACK, .val.stack = dest }
                        }
                    };

                    InstrArray_insert(func_instrs, second_mov, instr_idx+1);
                    InstrArray_insert(func_instrs, first_mov, instr_idx);
                }
                break;
            }   // case BINARYOP_MULT

            case BINARYOP_LSHFT:
            case BINARYOP_RSHFT: {
                if (instr->instr.binary.src.type == OPERAND_STACK) {
                    // TODO: This may have to be more detailed later, reg = CL
                    int src = instr->instr.binary.src.val.stack;
                    instr->instr.binary.src.type = OPERAND_REG;
                    instr->instr.binary.src.val.reg = CX;

                    AsmInstr mov_stack_cl = (AsmInstr){
                        .kind = ASM_INSTR_MOV,
                        .instr.mov = {
                            .src = (Operand) { .type = OPERAND_STACK, .val.stack = src },
                            .dest = (Operand){ .type = OPERAND_REG, .val.reg = CX }
                        }
                    };
                    InstrArray_insert(func_instrs, mov_stack_cl, instr_idx);
                }
                break;
            }
            }

            break;
        }   // case ASM_INSTR_BINARY

        case ASM_INSTR_CMP: {
            if (instr->instr.cmp.src1.type == OPERAND_STACK && instr->instr.cmp.src2.type == OPERAND_STACK) {
                AsmInstr mov = (AsmInstr){ .kind = ASM_INSTR_MOV };
                mov.instr.mov.src = instr->instr.cmp.src1;
                mov.instr.mov.dest = (Operand){ .type = OPERAND_REG, .val.reg = R10 };
                instr->instr.cmp.src1 = (Operand){ .type = OPERAND_REG, .val.reg = R10 };
                InstrArray_insert(func_instrs, mov, instr_idx);
            } else if (instr->instr.cmp.src2.type == OPERAND_IMM) {
                AsmInstr mov = (AsmInstr){ .kind = ASM_INSTR_MOV };
                mov.instr.mov.src = instr->instr.cmp.src2;
                mov.instr.mov.dest = (Operand){ .type = OPERAND_REG, .val.reg = R11 };
                instr->instr.cmp.src2 = (Operand){ .type = OPERAND_REG, .val.reg = R11 };
                InstrArray_insert(func_instrs, mov, instr_idx);
            }
            break;
        } // case ASM_INSTR_CMP

        }
    }
}

static void resolve_pseudo_operand(CodegenDriver *cgd, Operand *op, int *total_offset) {
    int val = 0;
    if (PseudoSymMap_contains(&cgd->stack_offsets, *op->val.pseudo, &val)) {
        Operand new_op = {0};
        new_op.type = OPERAND_STACK;
        new_op.val.stack = val;
        *op = new_op;
    } else {
        *total_offset += 4;
        PseudoStackMapping mapping = {0};
        mapping.stack_offset = -(*total_offset);
        mapping.ident = String_copy(*op->val.pseudo);
        PseudoSymMap_append(&cgd->stack_offsets, mapping);
    
        Operand new_op = {0};
        new_op.type = OPERAND_STACK;
        new_op.val.stack = mapping.stack_offset;
        *op = new_op;
    }
}

static void resolve_instr_pseudo_ops(CodegenDriver *cgd, AsmInstr *instr, int *total_offset) {
    switch (instr->kind) {
    case ASM_INSTR_INVALID:
    case ASM_INSTR_CDQ:
    case ASM_INSTR_RET:
    case ASM_ALLOCSTACK:
        break;

    case ASM_INSTR_MOV:
        if (instr->instr.mov.src.type == OPERAND_PSEUDO) {
            resolve_pseudo_operand(cgd, &instr->instr.mov.src, total_offset);
        }
        if (instr->instr.mov.dest.type == OPERAND_PSEUDO) {
            resolve_pseudo_operand(cgd, &instr->instr.mov.dest, total_offset);
        }
        break;

    case ASM_INSTR_CMP:
        if (instr->instr.cmp.src1.type == OPERAND_PSEUDO) {
            resolve_pseudo_operand(cgd, &instr->instr.cmp.src1, total_offset);
        }
        if (instr->instr.cmp.src2.type == OPERAND_PSEUDO) {
            resolve_pseudo_operand(cgd, &instr->instr.cmp.src2, total_offset);

        }
        break;

    case ASM_INSTR_SETCC:
        if (instr->instr.setcc.dest.type == OPERAND_PSEUDO) {
            resolve_pseudo_operand(cgd, &instr->instr.setcc.dest, total_offset);
        }
        break;

    case ASM_INSTR_UNARY:
        if (instr->instr.unary.op.type == OPERAND_PSEUDO) {
            resolve_pseudo_operand(cgd, &instr->instr.unary.op, total_offset);
        }
        break;

    case ASM_INSTR_BINARY:
        if (instr->instr.binary.src.type == OPERAND_PSEUDO) {
            resolve_pseudo_operand(cgd, &instr->instr.binary.src, total_offset);
        }
        if (instr->instr.binary.dest.type == OPERAND_PSEUDO) {
            resolve_pseudo_operand(cgd, &instr->instr.binary.dest, total_offset);
        }
        break;

    case ASM_INSTR_IDIV:
        if (instr->instr.idiv.divisor.type == OPERAND_PSEUDO) {
            resolve_pseudo_operand(cgd, &instr->instr.idiv.divisor, total_offset);
        }
        break;
    case ASM_INSTR_PUSH:
        if (instr->instr.push.type == OPERAND_PSEUDO) {
            resolve_pseudo_operand(cgd, &instr->instr.push, total_offset);
        }
        break;
    }
}

// Second pass of TACD -> ASM; Replace Pseudo registers with stack offsets.
static int resolve_pseudo_registers(CodegenDriver *cgd, AsmFn *asm_fn) {
    if (cgd == NULL || asm_fn == NULL) return -1;
    if (cgd->program == NULL) return -1;

    InstrArray *instructions = &asm_fn->instrs;
    int total_offset = 0;

    for (size_t instr_idx = 0; instr_idx < instructions->len; instr_idx++) {
        AsmInstr *instr = &instructions->instrs[instr_idx];
        resolve_instr_pseudo_ops(cgd, instr, &total_offset);
    }

    return total_offset;
}

static void AsmProgram_print(AsmProgram *prog, int indent_lvl);

void emit_asm(CompDriver *cd, TacdProgram *src) {
    CodegenDriver *cgd = &cd->cgd;
    // First pass of TACD -> ASM; Generate preliminary ASM.
    cgd->program = trx_program(src);

#ifdef DEBUG
    if (cd->opts.dev_debug_flags[DDF_PRINT_CODEGEN] || cd->opts.dev_debug_flags[DDF_PRINT_ALL]) {
        puts("Generated ASM Structure ([1] Initial Generation)\n================================================");
        AsmProgram_print(cgd->program, 0);
    }
#endif

    // Second pass of TACD -> ASM; Replace Pseudo registers with stack offsets.
    for (size_t fn_idx = 0; fn_idx < cgd->program->fns.len; fn_idx++) {
        AsmFn *asm_fn = &cgd->program->fns.fns[fn_idx];
        int resolved_offset = resolve_pseudo_registers(cgd, asm_fn);
        resolve_function_stack(cgd, asm_fn, resolved_offset);
        resolve_invalid_instructions(cgd, asm_fn);
    }


#ifdef DEBUG
    if (cd->opts.dev_debug_flags[DDF_PRINT_CODEGEN] || cd->opts.dev_debug_flags[DDF_PRINT_ALL]) {
        puts("Generated ASM Structure ([4] Fix Bad Instructions)\n==================================================");
        AsmProgram_print(cgd->program, 0);
    }
#endif

}

/*
   ASM tree printing (Debug purposes)
*/

static void Operand_print(Operand op) {
    switch (op.type) {
    case OPERAND_IMM:
        printf("%d", op.val.imm);
        break;

    case OPERAND_REG:
        switch (op.val.reg) {
            case AX:
                printf("%%AX");
                break;
            case CX:
                printf("%%CX");
                break;
            case DX:
                printf("%%DX");
                break;
            case DI:
                printf("%%DI");
                break;
            case SI:
                printf("%%SI");
                break;
            case R8:
                printf("%%R8");
                break;
            case R9:
                printf("%%R9");
                break;
            case R10:
                printf("%%R10");
                break;
            case R11:
                printf("%%R11");
                break;
            default:
                printf("Register");
                break;
        }
        break;

    case OPERAND_PSEUDO:
        printf("%s", op.val.pseudo->cstr);
        break;

    case OPERAND_STACK:
        printf("%d(%%rbp)", op.val.stack);
        break;

    case OPERAND_INVALID:
        printf("INVALID OPERAND");
    }
}

static void UnaryOp_print(UnaryOp unop) {
    switch (unop) {
    case UNARYOP_INVALID:
        printf("INVALID UNARY");
        break;
    case UNARYOP_NEG:
        printf("Neg");
        break;
    case UNARYOP_NOT:
        printf("Not");
        break;
    default:
        printf("INVALID UNARY");
    }
}

static void BinaryOp_print(BinaryOp binop) {
    switch (binop) {
    case BINARYOP_INVALID:
        printf("INVALID BINARY");
        break;
    case BINARYOP_ADD:
        printf("Add");
        break;
    case BINARYOP_SUB:
        printf("Sub");
        break;
    case BINARYOP_MULT:
        printf("Mult");
        break;
    case BINARYOP_BITAND:
        printf("BitAnd");
        break;
    case BINARYOP_BITOR:
        printf("BitOr");
        break;
    case BINARYOP_BITXOR:
        printf("BitXor");
        break;
    case BINARYOP_LSHFT:
        printf("Left Shift");
        break;
    case BINARYOP_RSHFT:
        printf("Right Shift");
        break;
    default:
        printf("INVALID BINARY");
        break;
    }
}

static void ConditionCode_print(ConditionCode cond) {
    switch (cond) {
    case CC_INVALID:
        printf("INVALID");
        break;
    case CC_E:
        printf("E");
        break;
    case CC_NE:
        printf("NE");
        break;
    case CC_L:
        printf("L");
        break;
    case CC_LE:
        printf("LE");
        break;
    case CC_G:
        printf("G");
        break;
    case CC_GE:
        printf("GE");
        break;
    }
}

static void AsmInstr_print(AsmInstr *instr, int indent_lvl) {
    int spaces = indent_lvl * 4;

    switch(instr->kind) {
    case ASM_INSTR_MOV:
        printf("%2$*1$s", spaces+4, "Mov(");
        Operand_print(instr->instr.mov.src);
        printf(",");
        Operand_print(instr->instr.mov.dest);
        printf(")\n");
        break;

    case ASM_INSTR_UNARY:
        printf("%2$*1$s", spaces+6, "Unary(");
        UnaryOp_print(instr->instr.unary.unop);
        printf(",");
        Operand_print(instr->instr.unary.op);
        printf(")\n");
        break;

    case ASM_INSTR_BINARY:
        printf("%2$*1$s", spaces+7, "Binary(");
        BinaryOp_print(instr->instr.binary.binop);
        printf(",");
        Operand_print(instr->instr.binary.src);
        printf(",");
        Operand_print(instr->instr.binary.dest);
        printf(")\n");
        break;

    case ASM_INSTR_IDIV:
        printf("%2$*1$s", spaces+5, "Idiv(");
        Operand_print(instr->instr.idiv.divisor);
        printf(")\n");
        break;

    case ASM_INSTR_CDQ:
        printf("%2$*1$s\n", spaces+3, "Cdq");
        break;

    case ASM_INSTR_CMP:
        printf("%2$*1$s", spaces+4, "Cmp(");
        Operand_print(instr->instr.cmp.src1);
        printf(",");
        Operand_print(instr->instr.cmp.src2);
        printf(")\n");
        break;

    case ASM_INSTR_JMP:
        printf("%2$*1$s%3$s)\n", spaces+4, "Jmp(", instr->instr.jmp->cstr);
        break;

    case ASM_INSTR_JMPCC:
        printf("%2$*1$s", spaces+6, "JmpCC(");
        ConditionCode_print(instr->instr.jmpcc.cond_code);
        printf(",%s)\n", instr->instr.jmpcc.target->cstr);
        break;

    case ASM_INSTR_SETCC:
        printf("%2$*1$s", spaces+6, "SetCC(");
        ConditionCode_print(instr->instr.setcc.cond_code);
        printf(",");
        Operand_print(instr->instr.setcc.dest);
        printf(")\n");
        break;

    case ASM_INSTR_LABEL:
        printf("%2$*1$s%3$s)\n", spaces+6-4, "Label(", instr->instr.label->cstr);
        break;

    case ASM_ALLOCSTACK:
        printf("%2$*1$s%3$d)\n", spaces+14, "AllocateStack(", instr->instr.alloc_stack);
        break;
    case ASM_DEALLOCSTACK:
        printf("%2$*1$s%3$d)\n", spaces+16, "DeallocateStack(", instr->instr.alloc_stack);
        break;

    case ASM_INSTR_RET:
        printf("%2$*1$s\n", spaces+3, "Ret");
        break;

    case ASM_INSTR_PUSH:
        printf("%2$*1$s(", spaces+4, "Push");
        Operand_print(instr->instr.push);
        printf(")\n");
        break;

    case ASM_INSTR_CALL:
        printf("%2$*1$s(%3$s)\n", spaces+4, "Call", instr->instr.call->cstr);
        break;

    case ASM_INSTR_INVALID:
        printf("%2$*1$s\n", spaces+19, "INVALID INSTRUCTION");
        break;
    }
}

static void AsmFn_print(AsmFn *asm_fn, int indent_lvl) {
    int spaces = indent_lvl * 4;

    printf("%2$*1$s\n", spaces+9, "Function(");
    indent_lvl += 1;
    spaces = indent_lvl * 4;
    printf("%2$*1$s\"%3$s\"\n%5$*4$s\n",
        spaces+5, "name=", asm_fn->name->cstr, spaces+6, "body=(");
    for (size_t instr_idx = 0; instr_idx < asm_fn->instrs.len; instr_idx++) {
        AsmInstr_print(&asm_fn->instrs.instrs[instr_idx], indent_lvl+1);
    }
    printf("%2$*1$c\n", spaces+1, ')');
    indent_lvl -= 1;
    spaces = indent_lvl * 4;
    printf("%2$*1$c\n", spaces+1, ')');
}

static void AsmProgram_print(AsmProgram *prog, int indent_lvl) {
    if (prog == NULL) return;
    int spaces = indent_lvl * 4;

    printf("%*s\n", spaces+8, "Program(");
    for (size_t fn_idx = 0; fn_idx < prog->fns.len; fn_idx++) {
        AsmFn_print(&prog->fns.fns[fn_idx], indent_lvl+1);
    }
    printf("%2$*1$c\n", spaces, ')');
}
