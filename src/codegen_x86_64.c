#include <stdio.h>
#include "codegen.h"

static void emit_mov(AsmNode *mov, FILE *dest) {
    fprintf(dest, "\tmovl\t");
    switch (mov->instr.mov.src->kind) {
    case ASMNODE_REG:
        fprintf(dest, "%%eax,");
        break;
    case ASMNODE_IMM:
        fprintf(dest, "$%d,", mov->instr.mov.src->instr.imm.c);
        break;
    default:
        /*  do nothing  */
        break;
    }
    switch (mov->instr.mov.dest->kind) {
    case ASMNODE_REG:
        fprintf(dest, " %%eax\n");
        break;
    case ASMNODE_IMM:
        fprintf(dest, " $%d\n", mov->instr.mov.src->instr.imm.c);
        break;
    default:
        /*  do nothing  */
        break;
    }
}

static void emit_function(AsmNode *func, FILE *dest) {
    fprintf(dest, "\t.globl %1$s\n%1$s:\n", func->instr.function.name.cstr);
    for (size_t instr_idx = 0; instr_idx < func->instr.function.instrs_len; instr_idx++) {
        AsmNode *instr = &func->instr.function.instructions[instr_idx];
        switch (instr->kind) {
        case ASMNODE_MOV:
            emit_mov(instr, dest);
            break;
        case ASMNODE_RET:
            fprintf(dest, "\tret\n");
            break;
        default:
            /*  do nothing  */
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

    emit_function(cgd->program->instr.program.function, asm_file);

    fprintf(asm_file, "\t.section .note.GNU-stack,\"\",@progbits\n");

    fclose(asm_file);
}
