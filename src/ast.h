#ifndef AST_H
#define AST_H

#include "dd_string.h"

typedef enum astNodeKind_ {
    ASTNODE_PROGRAM,
    ASTNODE_FUNCTION,
    ASTNODE_RETURN,
    ASTNODE_CONSTANT
} AstNodeKind;

typedef struct astNode_ *AstNode_ty;
typedef struct astNode_ {
    AstNodeKind kind;
    union {
        struct { AstNode_ty function; } program;
        struct { String name; AstNode_ty statement; } function;
        struct { AstNode_ty constant; } ret;
        struct { int c; } constant;
    } node;
} AstNode;

AstNode *AstNode_create();
void AstNode_destroy(AstNode *node);
void AstNode_print(AstNode *node, int indent_lvl);

#endif //AST_H
