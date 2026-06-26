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
    ASM_INSTR_INVALID,
    ASM_ALLOCSTACK,     // instruction `subq $n, %rsp`
    ASM_INSTR_MOV,
    ASM_INSTR_UNARY,
    ASM_INSTR_BINARY,
    ASM_INSTR_IDIV,
    ASM_INSTR_CDQ,
    ASM_INSTR_RET
} AsmInstrKind;

typedef enum unop_ {
    UNARYOP_INVALID,
    UNARYOP_NEG,
    UNARYOP_NOT
} UnaryOp;

typedef enum binop_ {
    BINARYOP_INVALID,
    BINARYOP_ADD,
    BINARYOP_SUB,
    BINARYOP_MULT
} BinaryOp;

typedef enum operandType_ {
    OPERAND_INVALID,
    OPERAND_IMM,
    OPERAND_REG,
    OPERAND_PSEUDO,
    OPERAND_STACK
} OperandType;

typedef enum register_ {
    AX,
    DX,
    R10,
    R11
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
        struct { BinaryOp binop; Operand src; Operand dest; } binary;
        struct { Operand divisor; } idiv;
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

/*
   CodegenDriver
*/

/*
    Map Pseudo(identifier) -> Stack(int)
*/

typedef struct {
    String ident;
    int stack_offset;
} PseudoStackMapping;

typedef struct pseudoSymMap_ {
    PseudoStackMapping *data;
    size_t len;
    size_t cap;
} PseudoSymMap;

typedef struct codegen_ {
    String dest;

    TacdGenerator tacd_gen;
    PseudoSymMap stack_offsets;

    AsmNode *program;
} CodegenDriver;

/*
   InstrArray
*/

void InstrArray_init(InstrArray *ia);
void InstrArray_deinit(InstrArray *ia);
void InstrArray_append(InstrArray *ia, AsmInstr in);
void InstrArray_insert(InstrArray *ia, AsmInstr in, size_t idx);

/*
   PseudoSymMap
*/

void PseudoSymMap_init(PseudoSymMap *map);
void PseudoSymMap_deinit(PseudoSymMap *map);
void PseudoSymMap_append(PseudoSymMap *map, PseudoStackMapping item);
bool PseudoSymMap_contains(PseudoSymMap *map, String key, int *val);

/* Translate TACD to generated ASM instructions. First pass.    */
void emit_asm(CodegenDriver *cgd, TacdNode *src);

/* Resolve Pseudo registers to Stack offsets. Returns total stack offset. Second pass.  */
int resolve_pseudo_registers(CodegenDriver *cgd);

/*  Create memory for a general AsmNode.
    If the intended node is a function, prefer `AsmNode_function_create`.   */
AsmNode *AsmNode_create();

/*  Free memory for an AsmNode. */
void AsmNode_destroy(AsmNode *node);

void CodegenDriver_init(CodegenDriver *cgd);
void CodegenDriver_deinit(CodegenDriver *cgd);

#endif // CODEGEN_H
