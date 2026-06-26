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
    TACD_CODE_BINARY
} TacdCodeKind;

typedef enum tacdValueKind_ {
    TACD_VALUE_INVALID,
    TACD_VALUE_CONSTANT,
    TACD_VALUE_IDENTIFIER
} TacdValueKind;

typedef enum tacdUnaryOp_ {
    TACD_UNARY_INVALID,
    TACD_UNARY_COMPLEMENT,
    TACD_UNARY_NEGATE
} TacdUnaryOp;

typedef enum tacdBinaryOp_ {
    TACD_BINARY_INVALID,
    TACD_BINARY_ADD,
    TACD_BINARY_SUBTRACT,
    TACD_BINARY_MULTIPLY,
    TACD_BINARY_DIVIDE,
    TACD_BINARY_REMAINDER
} TacdBinaryOp;

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
        struct { TacdBinaryOp op; TacdValue src1; TacdValue src2; TacdValue dest; } binary;
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
    TacdSymTable symbols;
    TacdNode *program;
} TacdGenerator;

/*  TODO: refactor this API */

void TacdGenerator_init(TacdGenerator *tg);
void TacdGenerator_deinit(TacdGenerator *tg);
void generate_tacd(TacdGenerator *tg, AstNode *ast_prog);
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
