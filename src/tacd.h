#ifndef TACD_H
#define TACD_H

#include "dd_string.h"

#include "param_array.h"
#include "ast.h"

typedef enum tacdCodeKind_ {
    TACD_CODE_INVALID,
    TACD_CODE_DCOMMENT, // Debug Comment
    TACD_CODE_RET,
    TACD_CODE_UNARY,
    TACD_CODE_BINARY,
    TACD_CODE_COPY,
    TACD_CODE_JUMP,
    TACD_CODE_JUMP_IF_ZERO,
    TACD_CODE_JUMP_IF_NOT_ZERO,
    TACD_CODE_LABEL,
    TACD_CODE_FN_CALL
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

typedef struct tacdValue_ {
    TacdValueKind kind;
    union {
        int constant;
        const String *identifier;
    } val;
} TacdValue;

typedef struct valueArray_ {
    TacdValue *vals;
    size_t len;
    size_t cap;
} ValueArray;

ValueArray *ValueArray_create(void);
void ValueArray_destroy(ValueArray *va);
void ValueArray_append(ValueArray *va, TacdValue val);

typedef struct tacdCode_ {
    TacdCodeKind kind;
    union {
        String *dcomment;
        TacdValue ret;
        struct {
            TacdUnaryOp op;
            TacdValue src;
            TacdValue dest;
        } unary;
        struct {
            TacdBinaryOp op;
            TacdValue src1; TacdValue src2; TacdValue dest;
        } binary;
        struct {
            TacdValue src;
            TacdValue dest;
        } copy;
        const String *jump;
        struct {
            TacdValue condition;
            const String *target;
        } jump_conditional;    // zero/not-zero encoded in kind
        const String *label;
        struct {
            const String *name;
            ValueArray *args;
            TacdValue dest;
        } fn_call;
    } code;
} TacdCode;

typedef struct CodeList_ {
    TacdCode *codes;
    size_t len;
    size_t cap;
} CodeList;

typedef struct tacdFunction_ {
    const String *name;
    ParamArray *params;
    CodeList body;
} TacdFunction;

typedef struct tacdFunctionArray_ {
    TacdFunction *fns;
    size_t len;
    size_t cap;
} TacdFunctionArray;

void TacdFunctionArray_init(TacdFunctionArray *tfa);
void TacdFunctionArray_deinit(TacdFunctionArray *tfa);
void TacdFunctionArray_append(TacdFunctionArray *tfa, TacdFunction fn);

typedef struct tacdProgram_ {
    TacdFunctionArray fn_defs;
} TacdProgram;

TacdProgram *TacdProgram_create();
void TacdProgram_destroy(TacdProgram *node);

typedef struct tacdGenerator_ {
    const String *func_name;    // Current function generating code for.
                                // These two will be used to generate tmp var
                                // names, e.g. "main.tmp.0". (func_name.tmp.tmpvar_count)
    int label_count;            // Current count of ASM labels generated.

    TacdProgram *program;
} TacdGenerator;

/*  TODO: refactor this API */

void TacdGenerator_init(TacdGenerator *tg);
void TacdGenerator_deinit(TacdGenerator *tg);
typedef struct compDriver_ CompDriver;
void generate_tacd(CompDriver *cd, AstProgram *ast_prog);

void CodeList_init(CodeList *il);
void CodeList_deinit(CodeList* il);
void CodeList_append(CodeList* il, TacdCode code);

void Tacd_print(TacdProgram *program);

#endif // TACD_H
