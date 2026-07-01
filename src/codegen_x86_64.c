#include <stdio.h>
#include "codegen.h"

// TODO: Create way to specify operand mnemonic sizes

static void emit_operand(Operand *op, FILE *dest) {
    switch (op->type) {
    case OPERAND_INVALID:
    case OPERAND_PSEUDO:
        break;
    case OPERAND_REG:
        switch (op->val.reg) {
        case AX:
            fprintf(dest, "%%eax");
            break;
        case CX:
            fprintf(dest, "%%cl");
            break;
        case DX:
            fprintf(dest, "%%edx");
            break;
        case R10:
            fprintf(dest, "%%r10d");
            break;
        case R11:
            fprintf(dest, "%%r11d");
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
    fprintf(dest, "\tmov");
    if (mov->instr.mov.dest.type == OPERAND_REG && mov->instr.mov.dest.val.reg == CX) {
        fprintf(dest, "b\t");
    } else {
        fprintf(dest, "l\t");
    }
    emit_operand(&mov->instr.mov.src, dest);
    fprintf(dest, ", ");
    emit_operand(&mov->instr.mov.dest, dest);
    fprintf(dest, "\n");
}

static void emit_unary(AsmInstr *unary, FILE *dest) {
    switch (unary->instr.unary.unop) {
    case UNARYOP_INVALID:
        break;
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
        case ASM_INSTR_INVALID:
            fprintf(dest, "# ERROR: INVALID INSTRUCTION\n");
            break;

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

        case ASM_INSTR_IDIV:
#ifdef DEBUG
            fprintf(dest, "# Divide\n");
#endif
            fprintf(dest, "\tidivl\t");
            emit_operand(&instr->instr.idiv.divisor, dest);
            fprintf(dest, "\n");
            break;

        case ASM_INSTR_CDQ:
            fprintf(dest, "\tcdq\n");
            break;

        case ASM_INSTR_BINARY: 
            switch (instr->instr.binary.binop) {
            case BINARYOP_INVALID:
                break;
            case BINARYOP_ADD:
#ifdef DEBUG
                fprintf(dest, "# Add\n");
#endif
                fprintf(dest, "\taddl\t");
                break;
            case BINARYOP_SUB:
#ifdef DEBUG
                fprintf(dest, "# Subtract\n");
#endif
                fprintf(dest, "\tsubl\t");
                break;
            case BINARYOP_MULT:
#ifdef DEBUG
                fprintf(dest, "# Multiply\n");
#endif
                fprintf(dest, "\timull\t");
                break;
            case BINARYOP_BITAND:
#ifdef DEBUG
                fprintf(dest, "# Bitwise AND\n");
#endif
                fprintf(dest, "\tandl\t");
                break;
            case BINARYOP_BITOR:
#ifdef DEBUG
                fprintf(dest, "# Bitwise OR\n");
#endif
                fprintf(dest, "\torl \t");
                break;
            case BINARYOP_BITXOR:
#ifdef DEBUG
                fprintf(dest, "# Bitwise XOR\n");
#endif
                fprintf(dest, "\txorl\t");
                break;
            case BINARYOP_LSHFT:
#ifdef DEBUG
                fprintf(dest, "# Shift Left\n");
#endif
                fprintf(dest, "\tsall\t");
                break;
            case BINARYOP_RSHFT:
#ifdef DEBUG
                fprintf(dest, "# Shift Right\n");
#endif
                fprintf(dest, "\tsarl\t");
                break;
            }
            
            emit_operand(&instr->instr.binary.src, dest);
            fprintf(dest, ", ");
            emit_operand(&instr->instr.binary.dest, dest);
            fprintf(dest, "\n");

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
