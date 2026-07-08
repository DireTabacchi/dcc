#include <stdio.h>
#include <stdlib.h>

#include "dd_string.h"

#include "token.h"
#include "tokenizer.h"
#include "dcc_error.h"
#include "ast.h"

#include "parser.h"

// TODO: write error creating util function (vargs?)

// Parser Utilities

typedef enum {
    // Highest precedence
    PREC_MDM,       // Multiply-Divide-Modulo
    PREC_AS,        // Add-Subtract
    PREC_BITSHIFT,  // Left/Right Bitwise Shift
    PREC_LTGTE,     // Less Than (Equal), Greater Than (Equal)
    PREC_EQ,        // Equal, Not Equal
    PREC_BAND,      // Bitwise And
    PREC_BXOR,      // Bitwise Xor
    PREC_BOR,       // Bitwise Or
    PREC_LAND,      // Logical And
    PREC_LOR,       // Logical Or
    // Lowest precedence
    PREC_LENGTH
} PrecedenceKind;

static int precedence_table[PREC_LENGTH] = {
    130,    // PREC_MDM
    120,    // PREC_AS
    110,    // PREC_BITSHIFT
    100,    // PREC_LTGTE
    90,     // PREC_EQ
    80,     // PREC_BAND
    70,     // PREC_BXOR
    60,     // PREC_BOR
    50,     // PREC_LAND
    40      // PREC_LOR
};

static int precedence(Token tok) {
    switch (tok.kind) {
    case TOKEN_OP_MINUS:
    case TOKEN_OP_PLUS:
        return precedence_table[PREC_AS];
    case TOKEN_OP_ASTERISK:
    case TOKEN_OP_SLASH:
    case TOKEN_OP_PERCENT:
        return precedence_table[PREC_MDM];
    case TOKEN_OP_LSHFT:
    case TOKEN_OP_RSHFT:
        return precedence_table[PREC_BITSHIFT];
    case TOKEN_OP_LT:
    case TOKEN_OP_GT:
    case TOKEN_OP_LTE:
    case TOKEN_OP_GTE:
        return precedence_table[PREC_LTGTE];
    case TOKEN_OP_DOUBLE_EQUAL:
    case TOKEN_OP_EXCLAMATION_EQUAL:
        return precedence_table[PREC_EQ];
    case TOKEN_OP_AMPERSAND:
        return precedence_table[PREC_BAND];
    case TOKEN_OP_BAR:
        return precedence_table[PREC_BOR];
    case TOKEN_OP_CARET:
        return precedence_table[PREC_BXOR];
    case TOKEN_OP_DOUBLE_AMP:
        return precedence_table[PREC_LAND];
    case TOKEN_OP_DOUBLE_BAR:
        return precedence_table[PREC_LOR];
    default:
        return 0;
    }
}

static Token advance_token(Parser* p) {
    p->prev_idx = p->curr_idx;
    p->curr_idx++;
    if (p->curr_idx >= p->tokenizer.tokens.len) {
        p->curr_idx = -1;
    }

    return p->tokenizer.tokens.toks[p->prev_idx];
}

static Token peek_token(Parser *p) {
    if (p->curr_idx < 0) return (Token){0};
    return p->tokenizer.tokens.toks[p->curr_idx];
}

static Token expect_token(Parser *p, TokenKind expected_kind) {
    if (p->curr_idx == -1) {
        return p->tokenizer.tokens.toks[p->prev_idx];
    }

    if (p->tokenizer.tokens.toks[p->curr_idx].kind == TOKEN_EOF && expected_kind != TOKEN_EOF) {
        Token err_token = p->tokenizer.tokens.toks[p->curr_idx];
        Error err = {0};
        err.file = String_init_cstr(p->tokenizer.src_path.cstr);
        err.pos = err_token.pos;
        String expected_token_kind = String_init_cstr(token_literal_list[expected_kind]);
        err.desc = String_init_length(expected_token_kind.len + 33);
        snprintf(err.desc.cstr, err.desc.len+1, "expected %s, but found end of input", expected_token_kind.cstr);
        String_free(&expected_token_kind);

        ErrorList_append(&p->errors, err);

        // Advance parser so curr_idx becomes -1, We've reached EOF, don't need
        // anymore errors saying that we've reached it.
        advance_token(p);

        return p->tokenizer.tokens.toks[p->prev_idx];
    }

    Token actual = p->tokenizer.tokens.toks[p->curr_idx];
    if (actual.kind == expected_kind) {
        advance_token(p);
        return actual;
    }

    Error err = {0};
    err.file = String_init_cstr(p->tokenizer.src_path.cstr);
    err.pos = actual.pos;
    String expected_token_kind = String_init_cstr(token_literal_list[expected_kind]);
    String found_token_kind;
    if (actual.kind == TOKEN_IDENTIFIER) {
        found_token_kind = String_init_length(actual.text.len + 13);
        snprintf(found_token_kind.cstr, found_token_kind.len+1, "identifier '%s'", actual.text.cstr);
    } else if (actual.kind == TOKEN_CONSTANT) { 
        found_token_kind = String_init_length(actual.text.len + 11);
        snprintf(found_token_kind.cstr, found_token_kind.len+1, "constant '%s'", actual.text.cstr);
    } else {
        found_token_kind = String_init_cstr(token_literal_list[actual.kind]);
    }
    err.desc = String_init_length(expected_token_kind.len + found_token_kind.len + 21);
    snprintf(err.desc.cstr, err.desc.len+1, "expected %s, but found %s", expected_token_kind.cstr, found_token_kind.cstr);
    String_free(&expected_token_kind);
    String_free(&found_token_kind);

    ErrorList_append(&p->errors, err);

    advance_token(p);
    return actual;
}

// parse_* functions

static String parse_identifier(Parser *p) {
    Token tok;
    if ((tok = expect_token(p, TOKEN_IDENTIFIER)), tok.kind != TOKEN_IDENTIFIER) {
        return String_init_cstr("ERROR");
    }

    Token ident = p->tokenizer.tokens.toks[p->prev_idx];
    return String_copy(ident.text);
}

static UnaryOpKind parse_unop(Parser *p) {
    Token tok = advance_token(p);
    if (tok.kind == TOKEN_OP_MINUS) {
        return UNARY_NEGATE;
    } else if (tok.kind == TOKEN_OP_COMPLEMENT) {
        return UNARY_COMPLEMENT;
    } else if (tok.kind == TOKEN_OP_EXCLAMATION) {
        return UNARY_NOT;
    }
    return UNARY_INVALID;
}

static BinaryOpKind parse_binop(Parser *p) {
    Token tok = advance_token(p);
    switch (tok.kind) {
    case TOKEN_OP_PLUS:
        return BINARY_ADD;
    case TOKEN_OP_MINUS:
        return BINARY_SUBTRACT;
    case TOKEN_OP_ASTERISK:
        return BINARY_MULTIPLY;
    case TOKEN_OP_SLASH:
        return BINARY_DIVIDE;
    case TOKEN_OP_PERCENT:
        return BINARY_REMAINDER;
    case TOKEN_OP_AMPERSAND:
        return BINARY_BITAND;
    case TOKEN_OP_BAR:
        return BINARY_BITOR;
    case TOKEN_OP_CARET:
        return BINARY_BITXOR;
    case TOKEN_OP_LSHFT:
        return BINARY_LSHFT;
    case TOKEN_OP_RSHFT:
        return BINARY_RSHFT;
    case TOKEN_OP_EXCLAMATION_EQUAL:
        return BINARY_NOT_EQUAL;
    case TOKEN_OP_DOUBLE_AMP:
        return BINARY_LOGICAND;
    case TOKEN_OP_DOUBLE_BAR:
        return BINARY_LOGICOR;
    case TOKEN_OP_DOUBLE_EQUAL:
        return BINARY_EQUAL;
    case TOKEN_OP_LT:
        return BINARY_LT;
    case TOKEN_OP_LTE:
        return BINARY_LTE;
    case TOKEN_OP_GT:
        return BINARY_GT;
    case TOKEN_OP_GTE:
        return BINARY_GTE;
    default:
        break;
    }

    return BINARY_INVALID;
}

static Expr *parse_expression(Parser *p, int min_prec);

static Expr *parse_factor(Parser *p) {
    Token tok = peek_token(p);
    switch (tok.kind) {
    case TOKEN_CONSTANT: {
        expect_token(p, TOKEN_CONSTANT);
        Token constant_tok = p->tokenizer.tokens.toks[p->prev_idx];

        int constant_val = strtol(constant_tok.text.cstr, NULL, 10);

        Expr *constant = Expr_create(EXPR_CONSTANT);
        constant->as.constant = constant_val;

        return constant;
    }
    case TOKEN_OP_MINUS:
    case TOKEN_OP_COMPLEMENT:
    case TOKEN_OP_EXCLAMATION: {
        UnaryOpKind op = parse_unop(p);
        Expr *expr = parse_factor(p);

        Expr *unary = Expr_create(EXPR_UNARY);
        unary->as.unary.op = op;
        unary->as.unary.expr = expr;

        return unary;
    }
    case TOKEN_LEFT_PAREN: {
        advance_token(p);
        Expr *exp = parse_expression(p, 0);
        expect_token(p, TOKEN_RIGHT_PAREN);
        return exp;
    }
    default: {
        Error exp_err = {0};
        exp_err.file = String_init_cstr(p->tokenizer.src_path.cstr);
        exp_err.desc = String_init_length(30 + dd_strlen(token_literal_list[tok.kind]));
        snprintf(exp_err.desc.cstr, exp_err.desc.len+1, "expected an expression, got \"%s\"", token_literal_list[tok.kind]);
        exp_err.pos = tok.pos;
        ErrorList_append(&p->errors, exp_err);
    }
    }

    Expr *inv = Expr_create(EXPR_INVALID);
    return inv;
}

static Expr *parse_expression(Parser *p, int min_prec) {
    Expr *left = parse_factor(p);
    Token next_tok = peek_token(p);
    while (Token_is_operator(next_tok) && precedence(next_tok) >= min_prec) {
        BinaryOpKind binop = parse_binop(p);
        Expr *right = parse_expression(p, precedence(next_tok)+1);

        Expr *new_left = Expr_create(EXPR_BINARY);
        new_left->as.binary.op = binop;
        new_left->as.binary.left = left;
        new_left->as.binary.right = right;

        left = new_left;

        next_tok = peek_token(p);
    }

    return left;
}

static Stmt *parse_statement(Parser *p) {
    expect_token(p, TOKEN_KW_RETURN);
    Expr *exp = parse_expression(p, 0);
    expect_token(p, TOKEN_SEMICOLON);

    Stmt *stmt = Stmt_create(STMT_RET);
    stmt->as.ret = exp;

    return stmt;
}

static Function *parse_function(Parser *p) {
    expect_token(p, TOKEN_KW_INT);
    String name = parse_identifier(p);
    expect_token(p, TOKEN_LEFT_PAREN);
    expect_token(p, TOKEN_KW_VOID);
    expect_token(p, TOKEN_RIGHT_PAREN);
    expect_token(p, TOKEN_LEFT_BRACE);
    Stmt *stmt = parse_statement(p);
    expect_token(p, TOKEN_RIGHT_BRACE);

    Function *func = Function_create();

    BlockItem item = {
        .kind = BLOCKITEM_STATEMENT,
        .as.statement = stmt
    };

    func->name = name;
    Block_append(&func->block, item);

    return func;
}

void parse(Parser *p) {
    Function *func = parse_function(p);
    expect_token(p, TOKEN_EOF);

    p->program->func = func;
}

// Parser management

void Parser_init(Parser *p, const char *path) {
    Tokenizer_init(&p->tokenizer, path);
    ErrorList_init(&p->errors);
    p->program = (Program *)malloc(sizeof(Program));
    Program_init(p->program);
    p->curr_idx = 0;
    p->prev_idx = p->curr_idx-1;
}

void Parser_destroy(Parser *p) {
    Tokenizer_destroy(&p->tokenizer);
    ErrorList_destroy(&p->errors);
    Program_deinit(p->program);
    free(p->program);
}

void Parser_print_ast(Parser *p) {
    puts("Generated AST Structure\n=======================");
    Program_print(p->program);
}

