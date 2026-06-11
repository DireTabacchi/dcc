#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dd_string.h"
#include "ast.h"
#include "codegen.h"


AsmNode *AsmNode_create() {
    AsmNode *n = malloc(sizeof(AsmNode));
    return n;
}

void AsmNode_destroy(AsmNode *node) {
    if (node == NULL) return;

    switch (node->kind) {
    case ASMNODE_PROGRAM:
        AsmNode_destroy(node->instr.program.function);
        free(node);
        break;
    case ASMNODE_FUNCTION:
        AsmNode_function_destroy(node);
        break;
    case ASMNODE_IMM:
    case ASMNODE_REG:
        free(node);
        break;
    /*  The following may appear in ASMNODE_FUNCTION, so may or may not have memory needing to be freed, or is
        taken care of in AsmNode_function_destroy.  */
    case ASMNODE_MOV:
        AsmNode_destroy(node->instr.mov.src);
        AsmNode_destroy(node->instr.mov.dest);
        /*  Don't need to free this node, it's part of calloc'd memory. */
        break;
    case ASMNODE_RET:
        /* NOTE: Nothing to do here, this node should
                 only appear in an ASMNODE_FUNCTION */
        break;
    }
}

AsmNode *AsmNode_function_create(String function_name) {
    AsmNode *f = malloc(sizeof(AsmNode));
    f->kind = ASMNODE_FUNCTION;
    f->instr.function.name = String_copy(function_name);
    f->instr.function.instrs_len = 0;
    f->instr.function.instrs_cap = 2;
    f->instr.function.instructions = calloc(f->instr.function.instrs_cap, sizeof(AsmNode));
    return f;
}

void AsmNode_function_destroy(AsmNode *asm_function) {
    if (asm_function == NULL) return;
    if (asm_function->kind != ASMNODE_FUNCTION) return;
    String_free(&asm_function->instr.function.name);
    for (size_t instr_idx = 0; instr_idx < asm_function->instr.function.instrs_len; instr_idx++) {
        AsmNode *instr = &asm_function->instr.function.instructions[instr_idx];
        if (instr->kind == ASMNODE_MOV) {
            AsmNode_destroy(instr);
        }
    }
    free(asm_function->instr.function.instructions);
    free(asm_function);
}

void AsmNode_function_append(AsmNode *asm_function, AsmNode *instr) {
    if (asm_function == NULL) return;
    if (asm_function->kind != ASMNODE_FUNCTION) return;

    if (asm_function->instr.function.instrs_len == asm_function->instr.function.instrs_cap) {
        size_t old_cap = asm_function->instr.function.instrs_cap;
        size_t new_cap = old_cap / 2 + old_cap;
        AsmNode_ty new_instrs = calloc(new_cap, sizeof(AsmNode));
        memcpy(new_instrs,
            asm_function->instr.function.instructions,
            asm_function->instr.function.instrs_cap*sizeof(AsmNode));
        free(asm_function->instr.function.instructions);
        asm_function->instr.function.instructions = new_instrs;
        asm_function->instr.function.instrs_cap = new_cap;
    }

    memcpy(&asm_function->instr.function.instructions[asm_function->instr.function.instrs_len],
        instr,
        sizeof(AsmNode));
    asm_function->instr.function.instrs_len += 1;
}

static void AsmNode_print(AsmNode *node, int indent_lvl) {
    int spaces = indent_lvl * 4;

    switch (node->kind) {
    case ASMNODE_PROGRAM:
        printf("%*s\n", spaces+8, "Program(");
        AsmNode_print(node->instr.program.function, indent_lvl+1);
        printf("%2$*1$c\n", spaces, ')');
        break;
    case ASMNODE_FUNCTION:
        printf("%2$*1$s\n", spaces+9, "Function(");
        indent_lvl += 1;
        spaces = indent_lvl * 4;
        printf("%2$*1$s\"%3$s\"\n%5$*4$s\n",
            spaces+5, "name=", node->instr.function.name.cstr, spaces+6, "body=(");
        for (size_t instr_idx = 0; instr_idx < node->instr.function.instrs_len; instr_idx++) {
            AsmNode_print(&node->instr.function.instructions[instr_idx], indent_lvl+1);
        }
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * 4;
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    case ASMNODE_MOV:
        printf("%2$*1$s", spaces+4, "Mov(");
        AsmNode_print(node->instr.mov.src, indent_lvl);
        printf(",");
        AsmNode_print(node->instr.mov.dest, indent_lvl);
        printf(")\n");
        break;
    case ASMNODE_IMM:
        printf("IMM(%d)", node->instr.imm.c);
        break;
    case ASMNODE_REG:
        printf("Register");
        break;
    case ASMNODE_RET:
        printf("%2$*1$s\n", spaces+3, "Ret");
        break;
    }
}

static AsmNode *trx_constant(AstNode *ast_constant) {
    if (ast_constant == NULL) return NULL;
    if (ast_constant->kind != ASTNODE_CONSTANT) return NULL;

    AsmNode *asm_imm = AsmNode_create();
    asm_imm->kind = ASMNODE_IMM;
    asm_imm->instr.imm.c = ast_constant->node.constant.c;

    return asm_imm;
}

static AsmNode *trx_ret(AstNode *ast_ret, AsmNode *asm_function) {
    if (ast_ret == NULL) return NULL;
    if (ast_ret->kind != ASTNODE_RETURN) return NULL;

    // generate code for MOV instruction.
    // generate the constant ASM code.
    AsmNode *asm_constant = trx_constant(ast_ret->node.ret.constant);

    // generate ASM for dest reg.
    AsmNode *asm_reg = AsmNode_create();
    asm_reg->kind = ASMNODE_REG;

    // Create the MOV instruction.
    AsmNode asm_mov  = {0};
    asm_mov.kind = ASMNODE_MOV;
    asm_mov.instr.mov.src = asm_constant;
    asm_mov.instr.mov.dest = asm_reg;

    // Add generated code to asm_function.
    AsmNode_function_append(asm_function, &asm_mov);

    AsmNode *asm_ret = AsmNode_create();
    asm_ret->kind = ASMNODE_RET;

    return asm_ret;
}

static AsmNode *trx_function(AstNode *ast_function) {
    if (ast_function == NULL) return NULL;
    if (ast_function->kind != ASTNODE_FUNCTION) return NULL;

    AsmNode *asm_function = AsmNode_function_create(ast_function->node.function.name);
    AsmNode *asm_ret = trx_ret(ast_function->node.function.statement, asm_function);

    AsmNode_function_append(asm_function, asm_ret);
    free(asm_ret);

    return asm_function;
}

static AsmNode *trx_program(AstNode *ast_program) {
    if (ast_program == NULL) return NULL;
    if (ast_program->kind != ASTNODE_PROGRAM) return NULL;

    AsmNode *prog = AsmNode_create();
    prog->kind = ASMNODE_PROGRAM;
    prog->instr.program.function = trx_function(ast_program->node.program.function);

    return prog;
}

void trx_ast_asm(CodegenDriver *cgd, AstNode *src) {
    cgd->program = trx_program(src);
}

void CodegenDriver_print_gen_asm(CodegenDriver *cgd) {
    puts("Generated ASM Structure\n=======================");
    AsmNode_print(cgd->program, 0);
}
