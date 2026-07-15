#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

/*
   AsmNode
*/

AsmNode *AsmNode_create() {
    AsmNode *n = malloc(sizeof(AsmNode));
    return n;
}

static AsmNode *AsmNode_function_create(String function_name) {
    AsmNode *f = AsmNode_create();
    f->kind = ASMNODE_FUNCTION;
    f->node.function.name = String_copy(function_name);
    InstrArray_init(&f->node.function.instrs);
    return f;
}

static void AsmNode_function_destroy(AsmNode *asm_function) {
    if (asm_function == NULL) return;
    if (asm_function->kind != ASMNODE_FUNCTION) return;

    String_free(&asm_function->node.function.name);
    InstrArray_deinit(&asm_function->node.function.instrs);
    free(asm_function);
}

void AsmNode_destroy(AsmNode *node) {
    if (node == NULL) return;

    switch (node->kind) {
    case ASMNODE_PROGRAM:
        AsmNode_destroy(node->node.program.function);
        free(node);
        break;

    case ASMNODE_FUNCTION:
        AsmNode_function_destroy(node);
        break;
    }
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
    AsmNode_destroy(cgd->program);
    String_free(&cgd->dest);
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
                printf("%%eax");
                break;
            case CX:
                printf("%%cl");
                break;
            case DX:
                printf("%%edx");
                break;
            case R10:
                printf("%%r10d");
                break;
            case R11:
                printf("%%r11d");
                break;
            default:
                printf("Register");
        }
        break;

    case OPERAND_PSEUDO:
        printf("%s", op.val.pseudo.cstr);
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
        printf("%2$*1$s%3$s)\n", spaces+4, "Jmp(", instr->instr.jmp.cstr);
        break;

    case ASM_INSTR_JMPCC:
        printf("%2$*1$s", spaces+6, "JmpCC(");
        ConditionCode_print(instr->instr.jmpcc.cond_code);
        printf(",%s)\n", instr->instr.jmpcc.target.cstr);
        break;

    case ASM_INSTR_SETCC:
        printf("%2$*1$s", spaces+6, "SetCC(");
        ConditionCode_print(instr->instr.setcc.cond_code);
        printf(",");
        Operand_print(instr->instr.setcc.dest);
        printf(")\n");
        break;

    case ASM_INSTR_LABEL:
        printf("%2$*1$s%3$s)\n", spaces+6-4, "Label(", instr->instr.label.cstr);
        break;

    case ASM_ALLOCSTACK:
        printf("%2$*1$s%3$d)\n", spaces+14, "AllocateStack(", instr->instr.alloc_stack);
        break;

    case ASM_INSTR_RET:
        printf("%2$*1$s\n", spaces+3, "Ret");
        break;

    case ASM_INSTR_INVALID:
        printf("%2$*1$s\n", spaces+19, "INVALID INSTRUCTION");
        break;
    }
}

static void AsmNode_print(AsmNode *node, int indent_lvl) {
    int spaces = indent_lvl * 4;

    switch (node->kind) {
    case ASMNODE_PROGRAM:
        printf("%*s\n", spaces+8, "Program(");
        AsmNode_print(node->node.program.function, indent_lvl+1);
        printf("%2$*1$c\n", spaces, ')');
        break;

    case ASMNODE_FUNCTION:
        printf("%2$*1$s\n", spaces+9, "Function(");
        indent_lvl += 1;
        spaces = indent_lvl * 4;
        printf("%2$*1$s\"%3$s\"\n%5$*4$s\n",
            spaces+5, "name=", node->node.function.name.cstr, spaces+6, "body=(");
        for (size_t instr_idx = 0; instr_idx < node->node.function.instrs.len; instr_idx++) {
            AsmInstr_print(&node->node.function.instrs.instrs[instr_idx], indent_lvl+1);
        }
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * 4;
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    }
}

/*
    trx_*   (TACD->ASM)
*/

static Operand trx_operand(TacdValue val) {
    switch (val.kind) {
    case TACD_NODE_INVALID:
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

static AsmInstr trx_binary_divide(AsmNode *asm_function, TacdCode *tacd_code) {
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
    InstrArray_append(&asm_function->node.function.instrs, mov_src1_ax);

    AsmInstr cdq = {0};
    cdq.kind = ASM_INSTR_CDQ;
    InstrArray_append(&asm_function->node.function.instrs, cdq);

    AsmInstr asm_idiv = {0};
    asm_idiv.kind = ASM_INSTR_IDIV;
    asm_idiv.instr.idiv.divisor = op_src2;
    InstrArray_append(&asm_function->node.function.instrs, asm_idiv);

    AsmInstr mov_ax_dest = {0};
    mov_ax_dest.kind = ASM_INSTR_MOV;
    mov_ax_dest.instr.mov.src = op_ax;
    mov_ax_dest.instr.mov.dest = op_dest;

    return mov_ax_dest;
}

static AsmInstr trx_binary_remainder(AsmNode *asm_function, TacdCode *tacd_code) {
    Operand op_src1 = trx_operand(tacd_code->code.binary.src1);
    Operand op_src2 = trx_operand(tacd_code->code.binary.src2);
    Operand op_dest = trx_operand(tacd_code->code.binary.dest);


    AsmInstr mov_src1_ax = {0};
    mov_src1_ax.kind = ASM_INSTR_MOV;
    mov_src1_ax.instr.mov.src = op_src1;
    mov_src1_ax.instr.mov.dest = (Operand){ .type = OPERAND_REG, .val.reg = AX };
    InstrArray_append(&asm_function->node.function.instrs, mov_src1_ax);

    AsmInstr cdq = {0};
    cdq.kind = ASM_INSTR_CDQ;
    InstrArray_append(&asm_function->node.function.instrs, cdq);

    AsmInstr asm_idiv = {0};
    asm_idiv.kind = ASM_INSTR_IDIV;
    asm_idiv.instr.idiv.divisor = op_src2;
    InstrArray_append(&asm_function->node.function.instrs, asm_idiv);

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

static AsmInstr trx_code(AsmNode *asm_function, TacdCode *tacd_code) {
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
            InstrArray_append(&asm_function->node.function.instrs, cmp);
            asm_mov.instr.mov.src = (Operand){ .type = OPERAND_IMM, .val.imm = 0 };
        } else {
            asm_mov.instr.mov.src = op_src;
        }
        asm_mov.instr.mov.dest = op_dest;
        InstrArray_append(&asm_function->node.function.instrs, asm_mov);

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
            return trx_binary_divide(asm_function, tacd_code);
        } else if (tacd_code->code.binary.op == TACD_BINARY_REMAINDER) {
            return trx_binary_remainder(asm_function, tacd_code);
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
            InstrArray_append(&asm_function->node.function.instrs, cmp);
            AsmInstr mov_zero = { .kind = ASM_INSTR_MOV };
            mov_zero.instr.mov.src = (Operand){ .type = OPERAND_IMM, .val.imm = 0 };
            mov_zero.instr.mov.dest = op_dest;
            InstrArray_append(&asm_function->node.function.instrs, mov_zero);
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
        InstrArray_append(&asm_function->node.function.instrs, mov_src_dest);

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
        InstrArray_append(&asm_function->node.function.instrs, comp);

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
        InstrArray_append(&asm_function->node.function.instrs, comp);

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
        InstrArray_append(&asm_function->node.function.instrs, asm_mov);

        AsmInstr asm_ret = { .kind = ASM_INSTR_RET };
        return asm_ret;
    }
    }

    return (AsmInstr){ .kind = ASM_INSTR_INVALID };
}

static AsmNode *trx_function(TacdNode *tacd_function) {
    if (tacd_function == NULL) return NULL;
    if (tacd_function->kind != TACD_NODE_FUNCTION) return NULL;

    AsmNode *asm_function = AsmNode_function_create(tacd_function->node.function.name);

    CodeList func_body = tacd_function->node.function.body;
    for (size_t code_idx = 0; code_idx < func_body.len; code_idx++) {
        AsmInstr instr = trx_code(asm_function, &func_body.codes[code_idx]);
        // TODO: check if instr is ASM_INSTR_INVALID
        InstrArray_append(&asm_function->node.function.instrs, instr);
    }
    //AsmInstr asm_ret = trx_ret(ast_function->node.function.statement, asm_function);


    return asm_function;
}

static AsmNode *trx_program(TacdNode *tacd_program) {
    /*  TODO: these checks should result in internal compiler error.    */
    if (tacd_program == NULL) return NULL;
    if (tacd_program->kind != TACD_NODE_PROGRAM) return NULL;

    AsmNode *prog = AsmNode_create();
    prog->kind = ASMNODE_PROGRAM;
    prog->node.program.function = trx_function(tacd_program->node.program.function);

    return prog;
}

static void resolve_function_stack(CodegenDriver *cgd, int resolved_offset) {
    AsmNode *function = cgd->program->node.program.function;
    AsmInstr stack_alloc = (AsmInstr){ .kind = ASM_ALLOCSTACK, .instr.alloc_stack = resolved_offset };
    InstrArray_insert(&function->node.function.instrs, stack_alloc, 0);
}

//static void resolve_invalid_mov(InstrArray *ia){}

static void resolve_invalid_instructions(CodegenDriver *cgd, AsmNode *function) {
    InstrArray *func_instrs = &function->node.function.instrs;
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
    if (PseudoSymMap_contains(&cgd->stack_offsets, op->val.pseudo, &val)) {
        Operand new_op = {0};
        new_op.type = OPERAND_STACK;
        new_op.val.stack = val;
        *op = new_op;
    } else {
        *total_offset -= 4;
        PseudoStackMapping mapping = {0};
        mapping.stack_offset = *total_offset;
        mapping.ident = String_copy(op->val.pseudo);
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
    }
}

// Second pass of TACD -> ASM; Replace Pseudo registers with stack offsets.
int resolve_pseudo_registers(CodegenDriver *cgd) {
    if (cgd == NULL) return -1;
    if (cgd->program == NULL) return -1;
    if (cgd->program->node.program.function->node.function.instrs.len <= 0) return -1;

    InstrArray *instructions = &cgd->program->node.program.function->node.function.instrs;
    int total_offset = 0;

    for (size_t instr_idx = 0; instr_idx < instructions->len; instr_idx++) {
        AsmInstr *instr = &instructions->instrs[instr_idx];
        resolve_instr_pseudo_ops(cgd, instr, &total_offset);
    }

    return total_offset;
}

void emit_asm(CodegenDriver *cgd, TacdNode *src) {
    // First pass of TACD -> ASM; Generate preliminary ASM.
    cgd->program = trx_program(src);

#ifdef DEBUG
    puts("Generated ASM Structure ([1] Initial Generation)\n================================================");
    AsmNode_print(cgd->program, 0);
#endif

    // Second pass of TACD -> ASM; Replace Pseudo registers with stack offsets.
    int resolved_offset = resolve_pseudo_registers(cgd);
#ifdef DEBUG
    puts("Generated ASM Structure ([2] Resolve Pseudo Registers)\n======================================================");
    AsmNode_print(cgd->program, 0);
    printf("resolved offset: %d\n", resolved_offset);
#endif

    // Third pass Resolve the function stack and invalid instructions
    resolve_function_stack(cgd, resolved_offset);
#ifdef DEBUG
    puts("Generated ASM Structure ([3] Add Stack Allocation)\n==================================================");
    AsmNode_print(cgd->program, 0);
#endif

    resolve_invalid_instructions(cgd, cgd->program->node.program.function);
#ifdef DEBUG
    puts("Generated ASM Structure ([4] Fix Bad Instructions)\n==================================================");
    AsmNode_print(cgd->program, 0);
#endif

}

