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
    PREC_BAND,      // Bitwise And
    PREC_BXOR,      // Bitwise Xor
    PREC_BOR,       // Bitwise Or
    // Lowest precedence
    PREC_LENGTH
} PrecedenceKind;

static int precedence_table[PREC_LENGTH] = {
    130,    // PREC_MDM
    120,    // PREC_AS
    110,    // PREC_BITSHIFT
    80,     // PREC_BAND
    70,     // PREC_BXOR
    60      // PREC_BOR
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
    case TOKEN_OP_AMPERSAND:
        return precedence_table[PREC_BAND];
    case TOKEN_OP_BAR:
        return precedence_table[PREC_BOR];
    case TOKEN_OP_CARET:
        return precedence_table[PREC_BXOR];
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

    default:
        break;
    }

    return BINARY_INVALID;
}

static AstNode *parse_expression(Parser *p, int min_prec);

static AstNode *parse_factor(Parser *p) {
    Token tok = peek_token(p);
    switch (tok.kind) {
    case TOKEN_CONSTANT: {
        expect_token(p, TOKEN_CONSTANT);
        Token constant_tok = p->tokenizer.tokens.toks[p->prev_idx];

        int constant_val = strtol(constant_tok.text.cstr, NULL, 10);

        AstNode *constant = AstNode_create();
        constant->kind = ASTNODE_CONSTANT;
        constant->node.constant = constant_val;

        return constant;
    }
    case TOKEN_OP_MINUS:
    case TOKEN_OP_COMPLEMENT: {
        UnaryOpKind op = parse_unop(p);
        AstNode *exp = parse_factor(p);

        AstNode *unary = AstNode_create();
        unary->kind = ASTNODE_UNARY;
        unary->node.unary.op = op;
        unary->node.unary.exp = exp;

        return unary;
    }
    case TOKEN_LEFT_PAREN: {
        advance_token(p);
        AstNode *exp = parse_expression(p, 0);
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

    AstNode *inv = AstNode_create();
    inv->kind = ASTNODE_INVALID;
    return inv;
}

static AstNode *parse_expression(Parser *p, int min_prec) {
    AstNode *left = parse_factor(p);
    Token next_tok = peek_token(p);
    while (Token_is_operator(next_tok) && precedence(next_tok) >= min_prec) {
        BinaryOpKind binop = parse_binop(p);
        AstNode *right = parse_expression(p, precedence(next_tok)+1);

        AstNode *new_left = AstNode_create();
        new_left->kind = ASTNODE_BINARY;
        new_left->node.binary.op = binop;
        new_left->node.binary.left = left;
        new_left->node.binary.right = right;

        left = new_left;

        next_tok = peek_token(p);
    }

    return left;
}

static AstNode *parse_statement(Parser *p) {
    expect_token(p, TOKEN_KW_RETURN);
    AstNode *exp = parse_expression(p, 0);
    expect_token(p, TOKEN_SEMICOLON);

    AstNode *stmt = AstNode_create();
    stmt->kind = ASTNODE_RETURN;
    stmt->node.ret.expr = exp;

    return stmt;
}

static AstNode *parse_function(Parser *p) {
    expect_token(p, TOKEN_KW_INT);
    String name = parse_identifier(p);
    expect_token(p, TOKEN_LEFT_PAREN);
    expect_token(p, TOKEN_KW_VOID);
    expect_token(p, TOKEN_RIGHT_PAREN);
    expect_token(p, TOKEN_LEFT_BRACE);
    AstNode *stmt = parse_statement(p);
    expect_token(p, TOKEN_RIGHT_BRACE);

    AstNode *f = AstNode_create();
    f->kind = ASTNODE_FUNCTION;
    f->node.function.name = name;
    f->node.function.statement = stmt;

    return f;
}

void parse(Parser *p) {
    AstNode *func = parse_function(p);
    expect_token(p, TOKEN_EOF);

    p->program->node.program.function = func;
}

// Parser management

void Parser_init(Parser *p, const char *path) {
    Tokenizer_init(&p->tokenizer, path);
    ErrorList_init(&p->errors);
    p->program = AstNode_create();
    p->program->kind = ASTNODE_PROGRAM;
    p->curr_idx = 0;
    p->prev_idx = p->curr_idx-1;
}

void Parser_destroy(Parser *p) {
    Tokenizer_destroy(&p->tokenizer);
    ErrorList_destroy(&p->errors);
    AstNode_destroy(p->program);
}

void Parser_print_ast(Parser *p) {
    puts("Generated AST Structure\n=======================");
    AstNode_print(p->program, 0);
}

