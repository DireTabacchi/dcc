#ifndef CODEGEN_H
#define CODEGEN_H

#include "dd_string.h"
#include "ast.h"

typedef enum asmNodeKind_ {
    ASMNODE_PROGRAM,
    ASMNODE_FUNCTION,
    ASMNODE_MOV,
    ASMNODE_IMM,
    ASMNODE_REG,
    ASMNODE_RET
} AsmNodeKind;

typedef struct asmNode_ *AsmNode_ty;
typedef struct asmNode_ {
    AsmNodeKind kind;
    union {
        struct { AsmNode_ty function; } program;
        struct {
            String name;
            AsmNode_ty instructions;
            size_t instrs_len;
            size_t instrs_cap;
        } function;
        struct {
            // TODO: perhaps create a specific structure for operands
            AsmNode_ty src;
            AsmNode_ty dest;
        } mov;
        struct { int c; } imm;
    } instr;
} AsmNode;

typedef struct codegen_ {
    AsmNode *program;
} CodegenDriver;

/* Translate an AST to generated ASM instructions.  */
void trx_ast_asm(CodegenDriver *cg, AstNode *src);

/*  Create memory for a general AsmNode.
    If the intended node is a function, prefer `AsmNode_function_create`.   */
AsmNode *AsmNode_create();

/*  Free memory for an AsmNode. */
void AsmNode_destroy(AsmNode *node);

/*  Create and initialize memory for an AsmNode function with `name`.
    Prefer this over AsmNode_create for making function nodes.  */
AsmNode *AsmNode_function_create(String name);

/*  Free and deinitialize memory for an AsmNode function.
    Also frees memory allocated for the node iteslf.
    Does nothing if `asm_function` kind is not `ASMNODE_FUNCTION`.  */
void AsmNode_function_destroy(AsmNode *asm_function);

/*  Append `instr` to the list of instructions in `asm_function`,
    growing the list as needed.
    The value pointed to by `instr` is copied, and the owner of `instr` will retain ownership of memory.
    Does nothing if `asm_function` kind is not `ASMNODE_FUNCTION`.  */
void AsmNode_function_append(AsmNode *asm_function, AsmNode *instr);
void CodegenDriver_print_gen_asm(CodegenDriver *cgd);

#endif // CODEGEN_H
