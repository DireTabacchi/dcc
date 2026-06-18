#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dd_string.h"
#include "ast.h"
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

    ia->cap = 0;
    ia->len = 0;
    free(ia->instrs);
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
   AsmNode
*/

AsmNode *AsmNode_create() {
    AsmNode *n = malloc(sizeof(AsmNode));
    return n;
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

// TODO: make the function body a list structure

AsmNode *AsmNode_function_create(String function_name) {
    AsmNode *f = AsmNode_create();
    f->kind = ASMNODE_FUNCTION;
    f->node.function.name = String_copy(function_name);
    InstrArray_init(&f->node.function.instrs);
    return f;
}

void AsmNode_function_destroy(AsmNode *asm_function) {
    if (asm_function == NULL) return;
    if (asm_function->kind != ASMNODE_FUNCTION) return;

    String_free(&asm_function->node.function.name);
    InstrArray_deinit(&asm_function->node.function.instrs);
    free(asm_function);
}

static void Operand_print(Operand op) {
    switch (op.type) {
    case OPERAND_IMM:
        printf("%d", op.val.imm);
        break;
    case OPERAND_REG:
        printf("Register");
        break;
    case OPERAND_PSEUDO:
    case OPERAND_STACK:
        /*  TODO: printing OPERAND_PSEUDO and OPERAND_STACK.    */
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
    case ASM_ALLOCSTACK:
        /*  TODO: printing ASM_INSTR_UNARY and ASM_ALLOCSTACK.  */
        break;
    case ASM_INSTR_RET:
        printf("%2$*1$s\n", spaces+3, "Ret");
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
    trx_*   (AST->ASM)
*/

static Operand trx_constant(AstNode *ast_constant) {
    /*  TODO: These checks should result in internal compiler error.    */
    if (ast_constant == NULL) return (Operand){ 0 };
    if (ast_constant->kind != ASTNODE_CONSTANT) return (Operand){ 0 };

    Operand asm_imm = {0};
    asm_imm.type = OPERAND_IMM;
    asm_imm.val.imm = ast_constant->node.constant.c;

    return asm_imm;
}

static AsmInstr trx_ret(AstNode *ast_ret, AsmNode *asm_function) {
    if (ast_ret == NULL) return (AsmInstr){ 0 };
    if (ast_ret->kind != ASTNODE_RETURN) return (AsmInstr){ 0 };

    // generate code for MOV instruction.
    // generate the constant ASM code.
    Operand op_constant = trx_constant(ast_ret->node.ret.expr);

    // Create the MOV instruction.
    AsmInstr asm_mov  = {0};
    asm_mov.kind = ASM_INSTR_MOV;
    asm_mov.instr.mov.src = op_constant;
    asm_mov.instr.mov.dest = (Operand){ .type = OPERAND_REG, .val.reg = AX };

    // Add generated code to asm_function.
    InstrArray_append(&asm_function->node.function.instrs, asm_mov);

    AsmInstr asm_ret = { .kind = ASM_INSTR_RET };

    return asm_ret;
}

static AsmNode *trx_function(AstNode *ast_function) {
    if (ast_function == NULL) return NULL;
    if (ast_function->kind != ASTNODE_FUNCTION) return NULL;

    AsmNode *asm_function = AsmNode_function_create(ast_function->node.function.name);
    AsmInstr asm_ret = trx_ret(ast_function->node.function.statement, asm_function);

    InstrArray_append(&asm_function->node.function.instrs, asm_ret);

    return asm_function;
}

static AsmNode *trx_program(AstNode *ast_program) {
    /*  TODO: these checks should result in internal compiler error.    */
    if (ast_program == NULL) return NULL;
    if (ast_program->kind != ASTNODE_PROGRAM) return NULL;

    AsmNode *prog = AsmNode_create();
    prog->kind = ASMNODE_PROGRAM;
    prog->node.program.function = trx_function(ast_program->node.program.function);

    return prog;
}

void trx_ast_asm(CodegenDriver *cgd, AstNode *src) {
    cgd->program = trx_program(src);
}

void CodegenDriver_print_gen_asm(CodegenDriver *cgd) {
    puts("Generated ASM Structure\n=======================");
    AsmNode_print(cgd->program, 0);
}
