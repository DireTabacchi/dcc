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

typedef enum assignOpKind_ {
    ASSIGN_INVALID,
    ASSIGN_SIMPLE,
    ASSIGN_SUM,
    ASSIGN_DIFFERENCE,
    ASSIGN_PRODUCT,
    ASSIGN_QUOTIENT,
    ASSIGN_REMAINDER,

    ASSIGN_BITAND,
    ASSIGN_BITOR,
    ASSIGN_BITXOR,
    ASSIGN_LSHFT,
    ASSIGN_RSHFT
} AssignOpKind;

typedef enum exprKind_ {
    EXPR_INVALID,
    EXPR_CONSTANT,
    EXPR_VAR,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGN,
    EXPR_TERNARY
} ExpressionKind;

typedef enum stmtKind_ {
    STMT_INVALID,
    STMT_RET,
    STMT_EXPR,
    STMT_NULL,
    STMT_IF,
    STMT_LABELED,
    STMT_GOTO,
    STMT_COMPOUND,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_WHILE,
    STMT_DOWHILE,
    STMT_FOR
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

typedef enum forInitKind_ {
    FOR_INIT_INVALID,
    FOR_INIT_NULL,
    FOR_INIT_DECL,
    FOR_INIT_EXP
} ForInitKind;

typedef struct expr_ *Expr_ty;
typedef struct expr_ {
    ExpressionKind kind;
    Position pos;
    union {
        int constant;
        const String *var;
        struct { AssignOpKind op; Expr_ty lhs; Expr_ty rhs; } assign;
        struct { Expr_ty lhs; Expr_ty rhs; } compound_assign;
        struct { UnaryOpKind op; Expr_ty expr; } unary;
        struct { BinaryOpKind op; Expr_ty left; Expr_ty right; } binary;
        struct { Expr_ty cond; Expr_ty then_expr; Expr_ty else_expr; } ternary;
    } as;
} Expr;

Expr *Expr_create(ExpressionKind kind);
void Expr_destroy(Expr *expr);
void Expr_print(Expr *expr, int indent_lvl);

typedef struct blockItem_ BlockItem;
typedef struct block_ {
    BlockItem *items;
    size_t len;
    size_t cap;
} Block;

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

typedef struct forInit_ {
    ForInitKind kind;
    union { Decl *decl; Expr *exp; } as;
} ForInit;

typedef struct stmt_ *Stmt_ty;
typedef struct stmt_ {
    StatementKind kind;
    Position pos;
    union {
        Expr *ret;
        Expr *expr;
        struct { Expr *cond; Stmt_ty then_stmt; Stmt_ty else_stmt; } if_stmt;
        struct { const String *lbl; Stmt_ty stmt; } labeled_stmt;
        const String* goto_stmt;
        Block *compound_stmt;
        const String *break_stmt;
        const String *continue_stmt;
        struct { Expr *cond; Stmt_ty body; const String *lbl; } while_stmt;
        struct { Stmt_ty body; Expr *cond; const String *lbl; } do_while_stmt;
        struct { ForInit init; Expr *cond; Expr *post; Stmt_ty body; const String *lbl; } for_stmt;
    } as;
} Stmt;

Stmt *Stmt_create(StatementKind kind);
void Stmt_destroy(Stmt *stmt);
void Stmt_print(Stmt *stmt, int indent_lvl);

typedef struct blockItem_ {
    BlockItemKind kind;
    union {
        Stmt *statement;
        Decl *declaration;
    } as;
} BlockItem;

Block *Block_create();
void Block_destroy(Block *block);
void Block_append(Block *block, BlockItem item);

typedef struct func_ {
    const String *name;
    Block *block;
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
