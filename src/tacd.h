#ifndef TACD_H
#define TACD_H

#include "ast.h"

#include "dd_string.h"

typedef enum tacdNodeKind_ {
    TACD_NODE_INVALID,
    TACD_NODE_PROGRAM,
    TACD_NODE_FUNCTION,
} TacdNodeKind;

typedef enum tacdCodeKind_ {
    TACD_CODE_INVALID,
    TACD_CODE_RET,
    TACD_CODE_UNARY,
    TACD_CODE_BINARY,
    TACD_CODE_COPY,
    TACD_CODE_JUMP,
    TACD_CODE_JUMP_IF_ZERO,
    TACD_CODE_JUMP_IF_NOT_ZERO,
    TACD_CODE_LABEL
} TacdCodeKind;

typedef enum tacdValueKind_ {
    TACD_VALUE_INVALID,
    TACD_VALUE_CONSTANT,
    TACD_VALUE_IDENTIFIER
} TacdValueKind;

typedef enum tacdUnaryOp_ {
    TACD_UNARY_INVALID,
    TACD_UNARY_COMPLEMENT,
    TACD_UNARY_NEGATE,
    TACD_UNARY_NOT
} TacdUnaryOp;

typedef enum tacdBinaryOp_ {
    TACD_BINARY_INVALID,
    TACD_BINARY_ADD,
    TACD_BINARY_SUBTRACT,
    TACD_BINARY_MULTIPLY,
    TACD_BINARY_DIVIDE,
    TACD_BINARY_REMAINDER,

    TACD_BINARY_BITAND,
    TACD_BINARY_BITOR,
    TACD_BINARY_BITXOR,
    TACD_BINARY_LSHFT,
    TACD_BINARY_RSHFT,

    TACD_BINARY_EQUAL,
    TACD_BINARY_NOT_EQUAL,
    TACD_BINARY_LT,
    TACD_BINARY_LTE,
    TACD_BINARY_GT,
    TACD_BINARY_GTE
} TacdBinaryOp;

typedef enum tacdLabelKind_ {
    AND_FALSE,
    AND_END,
    OR_TRUE,
    OR_END,
    TACD_LABEL_KIND_LENGTH
} TacdLabelKind;

static String label_kind_table[TACD_LABEL_KIND_LENGTH] = {
    (String){ .cstr = (char*)"and_false",   .len = 9 },
    (String){ .cstr = (char*)"and_end",    .len = 7 },
    (String){ .cstr = (char*)"or_true",    .len = 7 },
    (String){ .cstr = (char*)"or_end",     .len = 6 }
};

typedef struct tacdValue_ {
    TacdValueKind kind;
    union {
        int constant;
        String identifier;
    } val;
} TacdValue;

typedef struct tacdCode_ {
    TacdCodeKind kind;
    union {
        TacdValue ret;
        struct { TacdUnaryOp op; TacdValue src; TacdValue dest; } unary;
        struct {
            TacdBinaryOp op;
            TacdValue src1; TacdValue src2; TacdValue dest;
        } binary;
        struct { TacdValue src; TacdValue dest; } copy;
        String jump;
        struct { TacdValue condition; String target; } jump_conditional;    // zero/not-zero encoded in kind
        String label;
    } code;
} TacdCode;

typedef struct CodeList_ {
    TacdCode *codes;
    size_t len;
    size_t cap;
} CodeList;

typedef struct tacdNode_ *TacdNode_ty;
typedef struct tacdNode_ {
    TacdNodeKind kind;
    union {
        struct { TacdNode_ty function; } program;
        struct { String name; CodeList body; } function;
    } node;
} TacdNode;

typedef struct tacdSymbolTable_ {
    String *syms;
    size_t len;
    size_t cap;
} TacdSymTable;

typedef struct tacdGenerator_ {
    int tmpvar_count;   // Current count of temporary vars generated.
    String func_name;   // Current function generating code for.
                        // These two will be used to generate tmp var
                        // names, e.g. "main.tmp.0". (func_name.tmp.tmpvar_count)
    int label_count;    // Like tmpvar_count, but for ASM labels.

    TacdSymTable symbols;
    TacdNode *program;
} TacdGenerator;

/*  TODO: refactor this API */

void TacdGenerator_init(TacdGenerator *tg);
void TacdGenerator_deinit(TacdGenerator *tg);
void generate_tacd(TacdGenerator *tg, AstProgram *ast_prog);
void Tacd_print(TacdNode *program, int indent_lvl);

void TacdSymTable_init(TacdSymTable *tst);
void TacdSymTable_deinit(TacdSymTable *tst);
void TacdSymTable_append(TacdSymTable *tst, String symbol);

TacdNode *TacdNode_create();
void TacdNode_destroy(TacdNode *node);

void CodeList_init(CodeList *il);
void CodeList_deinit(CodeList* il);
void CodeList_append(CodeList* il, TacdCode code);

#endif // TACD_H
