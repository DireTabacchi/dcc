#include <stdio.h>
#include "codegen.h"

static void emit_operand(Operand *op, FILE *dest) {
    switch (op->type) {
    case OPERAND_REG:
        switch (op->val.reg) {
        case AX:
            fprintf(dest, "%%eax");
            break;

        case R10:
            fprintf(dest, "%%r10d");
            break;
        }
        break;

    case OPERAND_IMM:
        fprintf(dest, "$%d", op->val.imm);
        break;

    case OPERAND_STACK:
        fprintf(dest, "%d(%%rbp)", op->val.stack);
        break;
    //default:
    //    /*  do nothing  */
    //    break;
    }
}

static void emit_mov(AsmInstr *mov, FILE *dest) {
    fprintf(dest, "\tmovl\t");
    emit_operand(&mov->instr.mov.src, dest);
    fprintf(dest, ", ");
    emit_operand(&mov->instr.mov.dest, dest);
    fprintf(dest, "\n");
}

static void emit_unary(AsmInstr *unary, FILE *dest) {
    switch (unary->instr.unary.unop) {
    case UNARYOP_NOT:
#ifdef DEBUG
        fprintf(dest, "# Unary Not\n");
#endif
        fprintf(dest, "\tnotl\t");
        break;
    case UNARYOP_NEG:
#ifdef DEBUG
        fprintf(dest, "# Unary Negate\n");
#endif
        fprintf(dest, "\tnegl\t");
        break;
    }
    emit_operand(&unary->instr.unary.op, dest);
    fprintf(dest, "\n");
}

static void emit_function(AsmNode *func, FILE *dest) {
    fprintf(dest, "\t.globl %1$s\n%1$s:\n", func->node.function.name.cstr);
    fprintf(dest, "\tpushq\t%%rbp\n\tmovq\t%%rsp, %%rbp\n");
    for (size_t instr_idx = 0; instr_idx < func->node.function.instrs.len; instr_idx++) {
        AsmInstr *instr = &func->node.function.instrs.instrs[instr_idx];
        switch (instr->kind) {
        case ASM_INSTR_MOV:
#ifdef DEBUG
            fprintf(dest, "# Mov\n");
#endif
            emit_mov(instr, dest);
            break;

        case ASM_INSTR_RET:
#ifdef DEBUG
            fprintf(dest, "# Return\n");
#endif
            fprintf(dest, "\tmovq\t%%rbp, %%rsp\n\tpopq\t%%rbp\n\tret\n");
            break;

        case ASM_ALLOCSTACK:
#ifdef DEBUG
            fprintf(dest, "# Allocate Stack\n");
#endif
            fprintf(dest, "\tsubq\t$%d, %%rsp\n", instr->instr.alloc_stack);
            break;

        case ASM_INSTR_UNARY:
            emit_unary(instr, dest);
            break;
        }
    }
}

void emit_program(CodegenDriver *cgd) {
    if (cgd->program == NULL) return;
    if (cgd->program->kind != ASMNODE_PROGRAM) return;

    FILE *asm_file = fopen(cgd->dest.cstr, "w");
    if (asm_file == NULL) {
        puts("Failed to open file");
        return;
    }

    emit_function(cgd->program->node.program.function, asm_file);

    fprintf(asm_file, "\t.section .note.GNU-stack,\"\",@progbits\n");

    fclose(asm_file);
}
