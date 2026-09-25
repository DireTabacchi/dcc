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

typedef enum tacdLinkage_ {
    TACD_LINKAGE_NONE,
    TACD_LINKAGE_INTERNAL,
    TACD_LINKAGE_EXTERNAL
} TacdLinkage;

typedef enum tacdDataType_ {
    TACD_DATATYPE_INT
} TacdDataType;

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
    TacdLinkage linkage;
    ParamArray *params;
    CodeList body;
} TacdFunction;

typedef struct tacdStaticVar_ {
    const String *identifier;
    TacdLinkage linkage;
    TacdDataType type;
    int init;
} TacdStaticVar;

typedef struct tacdStaticVarArray_ {
    size_t len;
    size_t cap;
    TacdStaticVar *vars;
} TacdStaticVarArray;

void TacdStaticVarArray_init(TacdStaticVarArray *tsva);
void TacdStaticVarArray_deinit(TacdStaticVarArray *tsva);
void TacdStaticVarArray_append(TacdStaticVarArray *tsva, TacdStaticVar sv);

typedef struct tacdFunctionArray_ {
    size_t len;
    size_t cap;
    TacdFunction *fns;
} TacdFunctionArray;

void TacdFunctionArray_init(TacdFunctionArray *tfa);
void TacdFunctionArray_deinit(TacdFunctionArray *tfa);
void TacdFunctionArray_append(TacdFunctionArray *tfa, TacdFunction fn);

typedef struct tacdTu_ {
    TacdStaticVarArray var_defs;
    TacdFunctionArray fn_defs;
} TacdTU;

TacdTU *TacdTU_create();
void TacdTU_destroy(TacdTU *node);

typedef struct tacdGenerator_ {
    const String *func_name;    // Current function generating code for.
                                // These two will be used to generate tmp var
                                // names, e.g. "main.tmp.0". (func_name.tmp.tmpvar_count)
    int label_count;            // Current count of ASM labels generated.

    TacdTU *tacd_tu;
} TacdGenerator;

/*  TODO: refactor this API */

void TacdGenerator_init(TacdGenerator *tg);
void TacdGenerator_deinit(TacdGenerator *tg);
typedef struct compDriver_ CompDriver;
void generate_tacd(CompDriver *cd, AstTU *ast_tu);

void CodeList_init(CodeList *il);
void CodeList_deinit(CodeList* il);
void CodeList_append(CodeList* il, TacdCode code);

void Tacd_print(TacdTU *program);

#endif // TACD_H
