#include <stdio.h>
#include "codegen.h"
#include "codegen_x86_64.h"

static void emit_operand(Operand *op, AsmOpSize size, FILE *dest) {
    switch (op->type) {
    case OPERAND_INVALID:
    case OPERAND_PSEUDO:
        break;
    case OPERAND_REG:
        switch (op->val.reg) {
        case AX:
            switch (size) {
            case AOS_BYTE:
                fprintf(dest, "%%al");
                break;
            case AOS_DWORD:
                fprintf(dest, "%%eax");
                break;
            }
            break;
        case CX:
            switch (size) {
            case AOS_BYTE:
                fprintf(dest, "%%cl");
                break;
            case AOS_DWORD:
                fprintf(dest, "%%ecx");
                break;
            }
            break;
        case DX:
            switch (size) {
            case AOS_BYTE:
                fprintf(dest, "%%dl");
                break;
            case AOS_DWORD:
                fprintf(dest, "%%edx");
                break;
            }
            break;
        case R10:
            switch (size) {
            case AOS_BYTE:
                fprintf(dest, "%%r10b");
                break;
            case AOS_DWORD:
                fprintf(dest, "%%r10d");
                break;
            }
            break;
        case R11:
            switch (size) {
            case AOS_BYTE:
                fprintf(dest, "%%r11b");
                break;
            case AOS_DWORD:
                fprintf(dest, "%%r11d");
                break;
            }
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
        emit_operand(&mov->instr.mov.src, AOS_BYTE, dest);
    } else {
        fprintf(dest, "l\t");
        emit_operand(&mov->instr.mov.src, AOS_DWORD, dest);
    }
    fprintf(dest, ", ");

    if (mov->instr.mov.dest.type == OPERAND_REG && mov->instr.mov.dest.val.reg == CX) {
        emit_operand(&mov->instr.mov.dest, AOS_BYTE, dest);
    } else {
        emit_operand(&mov->instr.mov.dest, AOS_DWORD, dest);
    }
    fprintf(dest, "\n");
}

static void emit_unary(AsmInstr *unary, FILE *dest) {
    switch (unary->instr.unary.unop) {
    case UNARYOP_INVALID:
    case UNARYOP_COND_NOT:
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
    emit_operand(&unary->instr.unary.op, AOS_DWORD, dest);
    fprintf(dest, "\n");
}

// TODO: encode sizes in Instructions (Operands?)
// TODO: improve logic for binary shift with previous TODO
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
//#ifdef DEBUG
//            fprintf(dest, "# Mov\n");
//#endif
            emit_mov(instr, dest);
            break;

        case ASM_INSTR_CMP:
//#ifdef DEBUG
//            fprintf(dest, "# Cmp\n");
//#endif
            fprintf(dest, "\tcmpl\t");
            emit_operand(&instr->instr.cmp.src1, AOS_DWORD, dest);
            fprintf(dest, ", ");
            emit_operand(&instr->instr.cmp.src2, AOS_DWORD, dest);
            fprintf(dest, "\n");
            break;

        case ASM_INSTR_JMP:
//#ifdef DEBUG
//            fprintf(dest, "# Jmp\n");
//#endif
            fprintf(dest, "\tjmp\t.L%s\n", instr->instr.jmp.cstr);
            break;

        case ASM_INSTR_JMPCC:
//#ifdef DEBUG
//            fprintf(dest, "# JmpCC\n");
//#endif
            fprintf(dest, "\tj%s\t.L%s\n",
                cond_code_table[instr->instr.jmpcc.cond_code],
                instr->instr.jmpcc.target.cstr);
            break;

        case ASM_INSTR_SETCC:
//#ifdef DEBUG
//            fprintf(dest, "# SetCC\n");
//#endif
            fprintf(dest, "\tset%s\t", cond_code_table[instr->instr.setcc.cond_code]);
            emit_operand(&instr->instr.setcc.dest, AOS_BYTE, dest);
            fprintf(dest, "\n");
            break;

        case ASM_INSTR_LABEL:
//#ifdef DEBUG
//            fprintf(dest, "# Label\n");
//#endif
            fprintf(dest, ".L%s:\n", instr->instr.label.cstr);
            break;

        case ASM_INSTR_RET:
//#ifdef DEBUG
//            fprintf(dest, "# Return\n");
//#endif
            fprintf(dest, "\tmovq\t%%rbp, %%rsp\n\tpopq\t%%rbp\n\tret\n");
            break;

        case ASM_ALLOCSTACK:
//#ifdef DEBUG
//            fprintf(dest, "# Allocate Stack\n");
//#endif
            fprintf(dest, "\tsubq\t$%d, %%rsp\n", -instr->instr.alloc_stack);
            break;

        case ASM_INSTR_UNARY:
            emit_unary(instr, dest);
            break;

        case ASM_INSTR_IDIV:
//#ifdef DEBUG
//            fprintf(dest, "# Divide\n");
//#endif
            fprintf(dest, "\tidivl\t");
            emit_operand(&instr->instr.idiv.divisor, AOS_DWORD, dest);
            fprintf(dest, "\n");
            break;

        case ASM_INSTR_CDQ:
            fprintf(dest, "\tcdq\n");
            break;

        case ASM_INSTR_BINARY: 
            switch (instr->instr.binary.binop) {
            case BINARYOP_INVALID:
            case BINARYOP_EQUAL:
            case BINARYOP_NOT_EQUAL:
            case BINARYOP_LT:
            case BINARYOP_LTE:
            case BINARYOP_GT:
            case BINARYOP_GTE:
                break;
            case BINARYOP_ADD:
//#ifdef DEBUG
//                fprintf(dest, "# Add\n");
//#endif
                fprintf(dest, "\taddl\t");
                break;
            case BINARYOP_SUB:
//#ifdef DEBUG
//                fprintf(dest, "# Subtract\n");
//#endif
                fprintf(dest, "\tsubl\t");
                break;
            case BINARYOP_MULT:
//#ifdef DEBUG
//                fprintf(dest, "# Multiply\n");
//#endif
                fprintf(dest, "\timull\t");
                break;
            case BINARYOP_BITAND:
//#ifdef DEBUG
//                fprintf(dest, "# Bitwise AND\n");
//#endif
                fprintf(dest, "\tandl\t");
                break;
            case BINARYOP_BITOR:
//#ifdef DEBUG
//                fprintf(dest, "# Bitwise OR\n");
//#endif
                fprintf(dest, "\torl \t");
                break;
            case BINARYOP_BITXOR:
//#ifdef DEBUG
//                fprintf(dest, "# Bitwise XOR\n");
//#endif
                fprintf(dest, "\txorl\t");
                break;
            case BINARYOP_LSHFT:
//#ifdef DEBUG
//                fprintf(dest, "# Shift Left\n");
//#endif
                fprintf(dest, "\tsall\t");
                emit_operand(&instr->instr.binary.src, AOS_BYTE, dest);
                break;
            case BINARYOP_RSHFT:
//#ifdef DEBUG
//                fprintf(dest, "# Shift Right\n");
//#endif
                fprintf(dest, "\tsarl\t");
                emit_operand(&instr->instr.binary.src, AOS_BYTE, dest);
                break;
            }
            
            if (instr->instr.binary.binop != BINARYOP_LSHFT && instr->instr.binary.binop != BINARYOP_RSHFT) {
                emit_operand(&instr->instr.binary.src, AOS_DWORD, dest);
            }
            fprintf(dest, ", ");

            if (instr->instr.binary.binop != BINARYOP_LSHFT && instr->instr.binary.binop != BINARYOP_RSHFT) {
                emit_operand(&instr->instr.binary.dest, AOS_DWORD, dest);
            } else {

                emit_operand(&instr->instr.binary.dest, AOS_BYTE, dest);
            }
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
