#ifndef TACD_H
#define TACD_H

#include "ast.h"

#include "dd_string.h"

typedef enum tacdCodeKind_ {
    TACD_INVALID,
    TACD_PROGRAM,
    TACD_FUNCTION,
    TACD_INSTRUCTION
} TacdCodeKind;

typedef enum tacdValueKind_ {
    TACD_VALUE_CONSTANT,
    TACD_VALUE_IDENTIFIER
} TacdValueKind;

typedef struct tacdValue_ {
    TacdValueKind kind;
    union {
        int constant;
        String identifier;
    } val;
} TacdValue;

typedef struct tacdCode_ *TacdCode_ty;
typedef struct tacdCode_ {
    TacdCodeKind kind;
    union {
        struct { TacdCode_ty function; } program;
        struct { String name; TacdCode_ty body; } function;
        struct { TacdValue val; } ret;
        struct { UnaryOpKind op; TacdValue src; TacdValue dest; } unary;
    } code;
} TacdCode;

#endif // TACD_H
