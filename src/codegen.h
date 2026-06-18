#ifndef CODEGEN_H
#define CODEGEN_H

#include "dd_string.h"
#include "ast.h"
#include "tacd.h"


typedef enum asmNodeKind_ {
    ASMNODE_PROGRAM,
    ASMNODE_FUNCTION,
} AsmNodeKind;

typedef enum asmInstrKind_ {
    ASM_ALLOCSTACK,     // instruction `subq $n, %rsp`
    ASM_INSTR_MOV,
    ASM_INSTR_UNARY,
    ASM_INSTR_RET
} AsmInstrKind;

typedef enum unop_ {
    NEG,
    NOT
} UnaryOp;

typedef enum operandType_ {
    OPERAND_IMM,
    OPERAND_REG,
    OPERAND_PSEUDO,
    OPERAND_STACK
} OperandType;

typedef enum register_ {
    AX,
    R10
} Register;

typedef struct operand_ {
    OperandType type;
    union {
        int imm;
        Register reg;
        String pseudo;
        int stack;
    } val;
} Operand;

typedef struct asmInstr_ {
    AsmInstrKind kind;
    union {
        struct { Operand src; Operand dest; } mov;
        struct { UnaryOp unop; Operand op; } unary;
        int alloc_stack;
    } instr;
} AsmInstr;

typedef struct instrArray_ {
    AsmInstr *instrs;
    size_t len;
    size_t cap;
} InstrArray;

typedef struct asmNode_ *AsmNode_ty;
typedef struct asmNode_ {
    AsmNodeKind kind;
    union {
        struct { AsmNode_ty function; } program;
        struct {
            String name;
            InstrArray instrs;
        } function;
    } node;
} AsmNode;

typedef struct codegen_ {
    String dest;
    TacdGenerator tacd_gen;
    AsmNode *program;
} CodegenDriver;

/*
   InstrArray
*/

void InstrArray_init(InstrArray *ia);
void InstrArray_deinit(InstrArray *ia);
void InstrArray_append(InstrArray *ia, AsmInstr in);

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
