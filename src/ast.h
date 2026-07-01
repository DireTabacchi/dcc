#ifndef AST_H
#define AST_H

#include "dd_string.h"

typedef enum astNodeKind_ {
    ASTNODE_INVALID,
    ASTNODE_PROGRAM,
    ASTNODE_FUNCTION,
    ASTNODE_RETURN,
    ASTNODE_CONSTANT,
    ASTNODE_UNARY,
    ASTNODE_BINARY
} AstNodeKind;

typedef enum unaryOpKind_ {
    UNARY_INVALID,
    UNARY_COMPLEMENT,
    UNARY_NEGATE
} UnaryOpKind;

typedef enum binaryOpKind_ {
    BINARY_INVALID,
    BINARY_ADD,
    BINARY_SUBTRACT,
    BINARY_MULTIPLY,
    BINARY_DIVIDE,
    BINARY_REMAINDER,

    BINARY_BITAND,
    BINARY_BITOR,
    BINARY_BITXOR,
    BINARY_LT,
    BINARY_GT,
    BINARY_LSHFT,
    BINARY_RSHFT
} BinaryOpKind;

typedef struct astNode_ *AstNode_ty;
typedef struct astNode_ {
    AstNodeKind kind;
    union {
        struct { AstNode_ty function; } program;
        struct { String name; AstNode_ty statement; } function;
        struct { AstNode_ty expr; } ret;
        int constant;
        struct { UnaryOpKind op; AstNode_ty exp; } unary;
        struct { BinaryOpKind op; AstNode_ty left; AstNode_ty right; } binary;
    } node;
} AstNode;

AstNode *AstNode_create();
void AstNode_destroy(AstNode *node);
void AstNode_print(AstNode *node, int indent_lvl);

#endif //AST_H
