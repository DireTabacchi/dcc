#include <stdio.h>
#include "codegen.h"

static void emit_mov(AsmInstr *mov, FILE *dest) {
    fprintf(dest, "\tmovl\t");
    switch (mov->instr.mov.src.type) {
    case OPERAND_REG:
        fprintf(dest, "%%eax,");
        break;
    case OPERAND_IMM:
        fprintf(dest, "$%d,", mov->instr.mov.src.val.imm);
        break;
    default:
        /*  do nothing  */
        break;
    }
    switch (mov->instr.mov.dest.type) {
    case OPERAND_REG:
        fprintf(dest, " %%eax\n");
        break;
    case OPERAND_IMM:
        fprintf(dest, " $%d\n", mov->instr.mov.src.val.imm);
        break;
    default:
        /*  do nothing  */
        break;
    }
}

static void emit_function(AsmNode *func, FILE *dest) {
    fprintf(dest, "\t.globl %1$s\n%1$s:\n", func->node.function.name.cstr);
    for (size_t instr_idx = 0; instr_idx < func->node.function.instrs.len; instr_idx++) {
        AsmInstr *instr = &func->node.function.instrs.instrs[instr_idx];
        switch (instr->kind) {
        case ASM_INSTR_MOV:
            emit_mov(instr, dest);
            break;
        case ASM_INSTR_RET:
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

    emit_function(cgd->program->node.program.function, asm_file);

    fprintf(asm_file, "\t.section .note.GNU-stack,\"\",@progbits\n");

    fclose(asm_file);
}
