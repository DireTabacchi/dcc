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
    ASM_DEALLOCSTACK,
    ASM_INSTR_MOV,
    ASM_INSTR_UNARY,
    ASM_INSTR_BINARY,
    ASM_INSTR_IDIV,
    ASM_INSTR_CDQ,
    ASM_INSTR_CMP,
    ASM_INSTR_JMP,
    ASM_INSTR_JMPCC,
    ASM_INSTR_SETCC,
    ASM_INSTR_LABEL,
    ASM_INSTR_PUSH,
    ASM_INSTR_CALL,
    ASM_INSTR_RET
} AsmInstrKind;

typedef enum unop_ {
    UNARYOP_INVALID,
    UNARYOP_NEG,
    UNARYOP_NOT,
    UNARYOP_COND_NOT
} UnaryOp;

typedef enum binop_ {
    BINARYOP_INVALID,
    BINARYOP_ADD,
    BINARYOP_SUB,
    BINARYOP_MULT,

    BINARYOP_BITAND,
    BINARYOP_BITOR,
    BINARYOP_BITXOR,
    BINARYOP_LSHFT,
    BINARYOP_RSHFT,

    BINARYOP_EQUAL,
    BINARYOP_NOT_EQUAL,
    BINARYOP_LT,
    BINARYOP_LTE,
    BINARYOP_GT,
    BINARYOP_GTE
} BinaryOp;

typedef enum operandType_ {
    OPERAND_INVALID,
    OPERAND_IMM,
    OPERAND_REG,
    OPERAND_PSEUDO,
    OPERAND_STACK
} OperandType;

typedef enum conditionCode_ {
    CC_INVALID,
    CC_E,
    CC_NE,
    CC_L,
    CC_LE,
    CC_G,
    CC_GE
} ConditionCode;

typedef enum register_ {
    AX,
    CX,
    DX,
    DI,
    SI,
    R8,
    R9,
    R10,
    R11
} Register;

static Register param_regs[6] = {
    DI, SI, DX, CX, R8, R9
};

typedef struct operand_ {
    OperandType type;
    union {
        int imm;
        Register reg;
        const String *pseudo;
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
        int dealloc_stack;
        struct { Operand src1; Operand src2; } cmp;
        const String *jmp;
        struct { ConditionCode cond_code; const String *target; } jmpcc;
        struct { ConditionCode cond_code; Operand dest; } setcc;
        const String *label;
        Operand push;
        const String *call;
    } instr;
} AsmInstr;

typedef struct instrArray_ {
    size_t len;
    size_t cap;
    AsmInstr *instrs;
} InstrArray;

void InstrArray_init(InstrArray *ia);
void InstrArray_deinit(InstrArray *ia);
void InstrArray_append(InstrArray *ia, AsmInstr in);
void InstrArray_insert(InstrArray *ia, AsmInstr in, size_t idx);

typedef struct asmFunction_ {
    const String *name;
    InstrArray instrs;
} AsmFn;

typedef struct asmFunctionArray_ {
    size_t len;
    size_t cap;
    AsmFn *fns;
} AsmFnArray;

void AsmFnArray_init(AsmFnArray *afa);
void AsmFnArray_deinit(AsmFnArray *afa);
void AsmFnArray_append(AsmFnArray *afa, AsmFn afn);

typedef struct asmProgram_ {
    AsmFnArray fns;
} AsmProgram;

AsmProgram *AsmProgram_create(void);
void AsmProgram_destroy(AsmProgram *prog);

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

void PseudoSymMap_init(PseudoSymMap *map);
void PseudoSymMap_deinit(PseudoSymMap *map);
void PseudoSymMap_append(PseudoSymMap *map, PseudoStackMapping item);
bool PseudoSymMap_contains(PseudoSymMap *map, String key, int *val);

typedef struct codegen_ {
    String dest;

    TacdGenerator tacd_gen;
    PseudoSymMap stack_offsets;

    AsmProgram *program;
} CodegenDriver;

typedef struct compDriver_ CompDriver;

/* Translate TACD to generated ASM instructions. First pass.    */
void emit_asm(CompDriver *cd, TacdProgram *src);

/* Resolve Pseudo registers to Stack offsets. Returns total stack offset. Second pass.  */
//int resolve_pseudo_registers(CodegenDriver *cgd);


void CodegenDriver_init(CodegenDriver *cgd);
void CodegenDriver_deinit(CodegenDriver *cgd);

#endif // CODEGEN_H
