#ifndef AST_H
#define AST_H

#include "common.h"
#include "dd_string.h"

typedef enum unaryOpKind_ {
    UNARY_INVALID,
    UNARY_COMPLEMENT,
    UNARY_NEGATE,
    UNARY_NOT,
    UNARY_PRE_INCR,
    UNARY_POST_INCR,
    UNARY_PRE_DECR,
    UNARY_POST_DECR
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
    BINARY_LSHFT,
    BINARY_RSHFT,

    BINARY_LOGICAND,
    BINARY_LOGICOR,
    BINARY_EQUAL,
    BINARY_NOT_EQUAL,
    BINARY_LT,
    BINARY_LTE,
    BINARY_GT,
    BINARY_GTE
} BinaryOpKind;

typedef enum exprKind_ {
    EXPR_INVALID,
    EXPR_CONSTANT,
    EXPR_VAR,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGN
} ExpressionKind;

typedef enum stmtKind_ {
    STMT_INVALID,
    STMT_RET,
    STMT_EXPR,
    STMT_NULL
} StatementKind;

typedef enum declKind_ {
    DECL_INVALID,
    DECL_LCL_VAR
} DeclarationKind;

typedef enum blockItemKind_ {
    BLOCKITEM_INVALID,
    BLOCKITEM_DECLARATION,
    BLOCKITEM_STATEMENT
} BlockItemKind;

typedef struct expr_ *Expr_ty;
typedef struct expr_ {
    ExpressionKind kind;
    Position pos;
    union {
        int constant;
        const String *var;
        struct { Expr_ty lhs; Expr_ty rhs; } assign;
        struct { UnaryOpKind op; Expr_ty expr; } unary;
        struct { BinaryOpKind op; Expr_ty left; Expr_ty right; } binary;
    } as;
} Expr;

Expr *Expr_create(ExpressionKind kind);
void Expr_destroy(Expr *expr);
void Expr_print(Expr *expr, int indent_lvl);

typedef struct stmt_ {
    StatementKind kind;
    Position pos;
    union {
        Expr *ret;
        Expr *expr;
    } as;
} Stmt;

Stmt *Stmt_create(StatementKind kind);
void Stmt_destroy(Stmt *stmt);
void Stmt_print(Stmt *stmt, int indent_lvl);

typedef struct decl_ {
    DeclarationKind kind;
    Position pos;
    union {
        struct { const String *identifier; Expr *init; } loc_var;
    } as;
} Decl;

Decl *Decl_create(DeclarationKind kind);
void Decl_destroy(Decl *decl);
void Decl_print(Decl *decl, int indent_lvl);

typedef struct blockItem_ {
    BlockItemKind kind;
    union {
        Stmt *statement;
        Decl *declaration;
    } as;
} BlockItem;

typedef struct block_ {
    BlockItem *items;
    size_t len;
    size_t cap;
} Block;

void Block_init(Block *block);
void Block_deinit(Block *block);
void Block_append(Block *block, BlockItem item);

typedef struct func_ {
    const String *name;
    Block block;
} Function;

Function *Function_create();
void Function_destroy(Function *func);
void Function_print(Function *func, int indent_lvl);

typedef struct astProgram_ {
    Function *func;
} AstProgram;

void Program_init(AstProgram *prog);
void Program_deinit(AstProgram *prog);
void Program_print(AstProgram *prog);

#endif //AST_H
