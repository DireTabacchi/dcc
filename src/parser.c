#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "dd_string.h"

#include "interner.h"
#include "comp_driver.h"
#include "token.h"
#include "dcc_error.h"
#include "ast.h"
#include "sym_table.h"

#include "parser.h"

// TODO: write error creating util function (vargs?)

// Parser Utilities

typedef enum {
    // Highest precedence
    PREC_POST_INC_DEC,  // Postfix increment/decrement
    PREC_MDM,           // Multiply-Divide-Modulo
    PREC_AS,            // Add-Subtract
    PREC_BITSHIFT,      // Left/Right Bitwise Shift
    PREC_LTGTE,         // Less Than (Equal), Greater Than (Equal)
    PREC_EQ,            // Equal, Not Equal
    PREC_BAND,          // Bitwise And
    PREC_BXOR,          // Bitwise Xor
    PREC_BOR,           // Bitwise Or
    PREC_LAND,          // Logical And
    PREC_LOR,           // Logical Or
    PREC_TERNARY,       // Ternary conditional operator ( ? : )
    PREC_ASSIGN,        // Assignment operations (lhs = rhs)
    // Lowest precedence
    PREC_LENGTH
} PrecedenceKind;

static int precedence_table[PREC_LENGTH] = {
    150,    // PREC_POST_INC_DEC
    130,    // PREC_MDM
    120,    // PREC_AS
    110,    // PREC_BITSHIFT
    100,    // PREC_LTGTE
    90,     // PREC_EQ
    80,     // PREC_BAND
    70,     // PREC_BXOR
    60,     // PREC_BOR
    50,     // PREC_LAND
    40,     // PREC_LOR
    30,     // PREC_TERNARY
    20,     // PREC_ASSIGN
};

static int precedence(Token tok) {
    switch (tok.kind) {
    //case TOKEN_OP_INCREMENT:
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
    case TOKEN_OP_QUESTION:
        return precedence_table[PREC_TERNARY];
    case TOKEN_OP_EQUAL:
    case TOKEN_OP_PLUS_EQUAL:
    case TOKEN_OP_MINUS_EQUAL:
    case TOKEN_OP_ASTERISK_EQUAL:
    case TOKEN_OP_SLASH_EQUAL:
    case TOKEN_OP_PERCENT_EQUAL:
    case TOKEN_OP_AMPERSAND_EQUAL:
    case TOKEN_OP_BAR_EQUAL:
    case TOKEN_OP_CARET_EQUAL:
    case TOKEN_OP_LSHFT_EQUAL:
    case TOKEN_OP_RSHFT_EQUAL:
        return precedence_table[PREC_ASSIGN];
    default:
        return 0;
    }
}

static Token advance_token(CompDriver* cd) {
    if (cd->parser.curr_idx == cd->parser.prev_idx)
        return cd->tokenizer.tokens.toks[cd->parser.curr_idx];
    //if (p->curr_idx < 0) return (Token){ .kind = TOKEN_INVALID };

    cd->parser.prev_idx = cd->parser.curr_idx;
    cd->parser.curr_idx++;
    if (cd->parser.curr_idx >= cd->tokenizer.tokens.len) {
        cd->parser.curr_idx = cd->parser.prev_idx;
    }

    return cd->tokenizer.tokens.toks[cd->parser.prev_idx];
}

static Token advance2_token(CompDriver* cd) {
    if (cd->parser.curr_idx == cd->parser.prev_idx)
        return cd->tokenizer.tokens.toks[cd->parser.curr_idx];
    //if (p->curr_idx < 0) return (Token){ .kind = TOKEN_INVALID };

    cd->parser.curr_idx += 2;
    cd->parser.prev_idx = cd->parser.curr_idx - 1;
    if (cd->parser.curr_idx >= cd->tokenizer.tokens.len) {
        cd->parser.curr_idx = cd->parser.prev_idx;
    }

    return cd->tokenizer.tokens.toks[cd->parser.prev_idx];
}

static Token peek_token(CompDriver *cd) {
    if (cd->parser.curr_idx == cd->parser.prev_idx) return (Token){ .kind = TOKEN_EOF };
    return cd->tokenizer.tokens.toks[cd->parser.curr_idx];
}

static Token peek2_token(CompDriver *cd) {
    if (cd->parser.curr_idx+1 >= cd->tokenizer.tokens.len) return (Token){ .kind = TOKEN_EOF };
    return cd->tokenizer.tokens.toks[cd->parser.curr_idx+1];
}
static Token peek_prev_token(CompDriver *cd) {
    if (cd->parser.curr_idx-1 < 0) return (Token){ .kind = TOKEN_INVALID };
    return cd->tokenizer.tokens.toks[cd->parser.curr_idx-1];
}

static Token expect_token(CompDriver *cd, TokenKind expected_kind) {
    if (cd->parser.curr_idx == -1) {
        return (Token){ .kind = TOKEN_EOF };
    }

    if (cd->tokenizer.tokens.toks[cd->parser.curr_idx].kind == TOKEN_EOF && expected_kind != TOKEN_EOF) {
        Token err_token = cd->tokenizer.tokens.toks[cd->parser.curr_idx];
        if (cd->parser.curr_idx != cd->parser.prev_idx)
            err_expected_token(&cd->errors, cd->tokenizer.src_path, expected_kind, err_token);
        // Advance parser so curr_idx becomes p->prev_idx, We've reached EOF, don't need
        // anymore errors saying that we've reached it.
        advance_token(cd);

        return cd->tokenizer.tokens.toks[cd->parser.prev_idx];
    }

    Token actual = cd->tokenizer.tokens.toks[cd->parser.curr_idx];
    if (actual.kind == expected_kind) {
        advance_token(cd);
        return actual;
    }

    err_expected_token(&cd->errors, cd->tokenizer.src_path, expected_kind, actual);

    advance_token(cd);
    return actual;
}

static void Parser_set_pos(CompDriver *cd, ssize_t set_idx) {
    if (cd == NULL) return;
    if (set_idx < 0 || set_idx >= cd->tokenizer.tokens.len) return;

    cd->parser.curr_idx = set_idx;
    cd->parser.prev_idx = cd->parser.curr_idx - 1;
}

// parse_* functions

static Block *parse_block(CompDriver *cd);
static Expr *parse_expression(CompDriver *cd, int min_prec);
static Decl *parse_var_declaration(CompDriver *cd);

static const String *parse_identifier(CompDriver *cd) {
    Token tok = expect_token(cd, TOKEN_IDENTIFIER);
    if (tok.kind != TOKEN_IDENTIFIER) {
        return (String *)StrInterner_intern(&cd->str_table, token_literals[TOKEN_INVALID]);
    }

    Token ident = cd->tokenizer.tokens.toks[cd->parser.prev_idx];
    return (String *)StrInterner_intern(&cd->str_table, *ident.text);
}

static UnaryOpKind parse_unop(CompDriver *cd) {
    Token tok = advance_token(cd);
    switch (tok.kind) {
    case TOKEN_OP_MINUS:
        return UNARY_NEGATE;
    case TOKEN_OP_COMPLEMENT:
        return UNARY_COMPLEMENT;
    case TOKEN_OP_EXCLAMATION:
        return UNARY_NOT;
    case TOKEN_OP_INCREMENT:
        return UNARY_PRE_INCR;
    case TOKEN_OP_DECREMENT:
        return UNARY_PRE_DECR;
    default:
        break;
    }
    return UNARY_INVALID;
}

static BinaryOpKind parse_binop(CompDriver *cd) {
    Token tok = advance_token(cd);
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

static ExprArray *parse_call_args(CompDriver *cd) {
    Token next_tok = peek_token(cd);
    if (next_tok.kind == TOKEN_RIGHT_PAREN) {
        return NULL;
    }

    ExprArray *call_args = ExprArray_create();
    do {
        if (next_tok.kind == TOKEN_OP_COMMA) advance_token(cd);
        Expr *arg = parse_expression(cd, 0);

        if (arg->kind == EXPR_INVALID) {
            // err should already be noted
            puts("arg is EXPR_INVALID");
            ExprArray_append(call_args, arg);
            break;
        }

        ExprArray_append(call_args, arg);
        
        next_tok = peek_token(cd);
    } while (next_tok.kind != TOKEN_RIGHT_PAREN && next_tok.kind == TOKEN_OP_COMMA);

    return call_args;
}

static Expr *parse_fn_call(CompDriver *cd) {
    Token next_tok = peek_token(cd);
    const String *fn_name = parse_identifier(cd);
    expect_token(cd, TOKEN_LEFT_PAREN);

    ExprArray *args = parse_call_args(cd);
    expect_token(cd, TOKEN_RIGHT_PAREN);

    Expr *fn_call = Expr_create(EXPR_FN_CALL);
    fn_call->pos = next_tok.pos;
    fn_call->as.fn_call.ident = fn_name;
    fn_call->as.fn_call.args = args;

    return fn_call;
}

static Expr *parse_primary(CompDriver *cd) {
    Token tok = peek_token(cd);
    switch (tok.kind) {
    case TOKEN_CONSTANT: {
        expect_token(cd, TOKEN_CONSTANT);
        Token constant_tok = cd->tokenizer.tokens.toks[cd->parser.prev_idx];
        int constant_val = strtol(constant_tok.text->cstr, NULL, 10);

        Expr *constant = Expr_create(EXPR_CONSTANT);
        constant->pos = tok.pos;
        constant->as.constant = constant_val;

        return constant;
    }

    case TOKEN_IDENTIFIER: {
        Token next_tok = peek2_token(cd);
        if (next_tok.kind == TOKEN_LEFT_PAREN) {
            return parse_fn_call(cd);
        }

        const String *var_name = parse_identifier(cd);

        Expr *var = Expr_create(EXPR_VAR);
        var->pos = tok.pos;
        var->as.var.name = var_name;
        var->as.var.old_name = var_name;

        return var;
    }

    case TOKEN_LEFT_PAREN: {
        advance_token(cd);
        Expr *exp = parse_expression(cd, 0);
        expect_token(cd, TOKEN_RIGHT_PAREN);
        return exp;
    }

    default: {
        err_expected_expression(&cd->errors, cd->tokenizer.src_path, tok);
        break;
    }
    }

    Expr *inv = Expr_create(EXPR_INVALID);
    inv->pos = tok.pos;
    return inv;
}

static Expr *parse_unary(CompDriver *cd) {
    Token tok = peek_token(cd);
    switch (tok.kind) {
    case TOKEN_OP_MINUS:
    case TOKEN_OP_COMPLEMENT:
    case TOKEN_OP_EXCLAMATION:
    case TOKEN_OP_INCREMENT:
    case TOKEN_OP_DECREMENT: {
        UnaryOpKind op = parse_unop(cd);
        Expr *expr = parse_unary(cd);

        Expr *unary = Expr_create(EXPR_UNARY);
        unary->pos = tok.pos;
        unary->as.unary.op = op;
        unary->as.unary.expr = expr;

        return unary;
    }

    default: 
        break;
    }

    Expr *expr = parse_primary(cd);
    if (expr->kind == EXPR_INVALID) return expr;

    Token next_tok = peek_token(cd);
    while (next_tok.kind == TOKEN_OP_INCREMENT || next_tok.kind == TOKEN_OP_DECREMENT) {
        Token op_tok = advance_token(cd);

        UnaryOpKind op_kind = (op_tok.kind == TOKEN_OP_INCREMENT) ?
            UNARY_POST_INCR : UNARY_POST_DECR;
        Expr *unary = Expr_create(EXPR_UNARY);
        unary->pos = op_tok.pos;
        unary->as.unary.op = op_kind;
        unary->as.unary.expr = expr;
        expr = unary;

        next_tok = peek_token(cd);
    }

    return expr;
}

static AssignOpKind parse_assign_op(CompDriver *cd) {
    Token tok = advance_token(cd);
    switch (tok.kind) {
    case TOKEN_OP_EQUAL:
        return ASSIGN_SIMPLE;
    case TOKEN_OP_PLUS_EQUAL:
        return ASSIGN_SUM;
    case TOKEN_OP_MINUS_EQUAL:
        return ASSIGN_DIFFERENCE;
    case TOKEN_OP_ASTERISK_EQUAL:
        return ASSIGN_PRODUCT;
    case TOKEN_OP_SLASH_EQUAL:
        return ASSIGN_QUOTIENT;
    case TOKEN_OP_PERCENT_EQUAL:
        return ASSIGN_REMAINDER;
    case TOKEN_OP_AMPERSAND_EQUAL:
        return ASSIGN_BITAND;
    case TOKEN_OP_BAR_EQUAL:
        return ASSIGN_BITOR;
    case TOKEN_OP_CARET_EQUAL:
        return ASSIGN_BITXOR;
    case TOKEN_OP_LSHFT_EQUAL:
        return ASSIGN_LSHFT;
    case TOKEN_OP_RSHFT_EQUAL:
        return ASSIGN_RSHFT;
    default:
        return ASSIGN_INVALID;
    }
}

static Expr *parse_conditional_then(CompDriver *cd) {
    expect_token(cd, TOKEN_OP_QUESTION);
    Expr *cond_then = parse_expression(cd, 0);
    expect_token(cd, TOKEN_OP_COLON);
    return cond_then;
}

static Expr *parse_expression(CompDriver *cd, int min_prec) {
    Expr *left = parse_unary(cd);
    if (left != NULL && left->kind == EXPR_INVALID) return left;
    Token next_tok = peek_token(cd);
    while (TOKENKIND_IS_BINARY_OPERATOR(next_tok.kind) && precedence(next_tok) >= min_prec) {
        if (TOKENKIND_IS_ASSIGNMENT(next_tok.kind)) {
            //advance_token(cd);
            AssignOpKind assign_op = parse_assign_op(cd);
            Expr *right = parse_expression(cd, precedence(next_tok));
            Expr *new_left = Expr_create(EXPR_ASSIGN);
            new_left->pos = left->pos;
            new_left->as.assign.op = assign_op;
            new_left->as.assign.lhs = left;
            new_left->as.assign.rhs = right;

            left = new_left;
        } else if (next_tok.kind == TOKEN_OP_QUESTION) {
            Expr *cond_then = parse_conditional_then(cd);
            Expr *cond_else = parse_expression(cd, precedence(next_tok));
            Expr *new_left = Expr_create(EXPR_TERNARY);
            new_left->pos = left->pos;
            new_left->as.ternary.cond = left;
            new_left->as.ternary.then_expr = cond_then;
            new_left->as.ternary.else_expr = cond_else;

            left = new_left;
        } else {
            BinaryOpKind binop = parse_binop(cd);
            Expr *right = parse_expression(cd, precedence(next_tok)+1);

            Expr *new_left = Expr_create(EXPR_BINARY);
            new_left->pos = left->pos;
            new_left->as.binary.op = binop;
            new_left->as.binary.left = left;
            new_left->as.binary.right = right;

            left = new_left;
        }
        next_tok = peek_token(cd);
    }

    return left;
}

static Expr *parse_optional_expression(CompDriver *cd, TokenKind sentinel) {
    Token next_tok = peek_token(cd);
    if (next_tok.kind == sentinel) {
        advance_token(cd);
        return NULL;
    }
    Expr *res = parse_expression(cd, 0);
    expect_token(cd, sentinel);
    return res;
}

static StorageClass parse_type_storage(CompDriver *cd) {
    TokenList types = {0};
    TokenList storage_classes = {0};

    TokenList_init(&types);
    TokenList_init(&storage_classes);

    Token first_tok;
    Token tok = first_tok = peek_token(cd);
    while (TOKENKIND_IS_TYPE_STORAGE(tok.kind)) {
        if (TOKENKIND_IS_TYPE_SPECIFIER(tok.kind)) {
            TokenList_append(&types, tok);
        } else if (TOKENKIND_IS_STORAGE_CLASS(tok.kind)) {
            TokenList_append(&storage_classes, tok);
        }
        advance_token(cd);
        tok = peek_token(cd);
    }

    if (types.len < 1) {
        err_missing_type_specifier(&cd->errors, cd->tokenizer.src_path, first_tok.pos);
    } else if (types.len != 1) {
        err_invalid_type_specifier(&cd->errors, cd->tokenizer.src_path, first_tok.pos);
    }
    if (storage_classes.len > 1) {
        err_invalid_storage_class(&cd->errors, cd->tokenizer.src_path, first_tok.pos);
    }

    // TODO: eventually track type
    // WHEN: add more valid types supported by DCC

    StorageClass sc = SC_NONE;
    if (storage_classes.len == 1) {
        Token sc_token = storage_classes.toks[0];
        switch (sc_token.kind) {
        case TOKEN_KW_STATIC:
            sc = SC_STATIC;
            break;
        case TOKEN_KW_EXTERN:
            sc = SC_EXTERN;
            break;
        default:
            break;
        }
    }

    TokenList_deinit(&types);
    TokenList_deinit(&storage_classes);

    return sc;
}

static Decl *parse_declaration(CompDriver *cd);

static Stmt *parse_statement(CompDriver *cd) {
    Token next_tok = peek_token(cd);
    if (next_tok.kind == TOKEN_KW_RETURN) {
        advance_token(cd);
        Expr *expr = parse_expression(cd, 0);
        if (expr != NULL && expr->kind == EXPR_INVALID) {
            Stmt *bad_ret = Stmt_create(STMT_RET);
            bad_ret->pos = next_tok.pos;
            bad_ret->as.ret = expr;
            return bad_ret;
        }
        expect_token(cd, TOKEN_SEMICOLON);
        Stmt *ret_stmt = Stmt_create(STMT_RET);
        ret_stmt->pos = next_tok.pos;
        ret_stmt->as.ret = expr;
        return ret_stmt;
    } else if (next_tok.kind == TOKEN_SEMICOLON) {
        advance_token(cd);
        Stmt *null_stmt = Stmt_create(STMT_NULL);
        null_stmt->pos = next_tok.pos;
        return null_stmt;
    } else if (next_tok.kind == TOKEN_KW_IF) {
        advance_token(cd);
        Stmt *if_stmt = Stmt_create(STMT_IF);
        if_stmt->pos = next_tok.pos;
        expect_token(cd, TOKEN_LEFT_PAREN);
        Expr *cond = parse_expression(cd, 0);
        expect_token(cd, TOKEN_RIGHT_PAREN);
        Stmt *then_stmt = parse_statement(cd);
        Stmt *else_stmt = NULL;
        Token else_tok = peek_token(cd);
        if (else_tok.kind == TOKEN_KW_ELSE) {
            advance_token(cd);
            else_stmt = parse_statement(cd);
        }
        if_stmt->as.if_stmt.cond = cond;
        if_stmt->as.if_stmt.then_stmt = then_stmt;
        if_stmt->as.if_stmt.else_stmt = else_stmt;
        return if_stmt;
    } else if (next_tok.kind == TOKEN_KW_GOTO) {
        advance_token(cd);
        const String *lbl = parse_identifier(cd);
        expect_token(cd, TOKEN_SEMICOLON);
        Stmt *goto_stmt = Stmt_create(STMT_GOTO);
        goto_stmt->pos = next_tok.pos;
        goto_stmt->as.goto_stmt = lbl;
        return goto_stmt;
    } else if (next_tok.kind == TOKEN_IDENTIFIER) {
        Token peeked2_tok = peek2_token(cd);
        if (peeked2_tok.kind == TOKEN_OP_COLON) {
            advance2_token(cd);
            Stmt *stmt = parse_statement(cd);
            Stmt *lbl_stmt = Stmt_create(STMT_LABELED);
            lbl_stmt->pos = next_tok.pos;
            lbl_stmt->as.labeled_stmt.stmt = stmt;
            lbl_stmt->as.labeled_stmt.lbl = next_tok.text;
            return lbl_stmt;
        }
    } else if (next_tok.kind == TOKEN_LEFT_BRACE) {
        Block *stmt_block = parse_block(cd);
        Stmt *cmpnd_stmt = Stmt_create(STMT_COMPOUND);
        cmpnd_stmt->pos = next_tok.pos;
        cmpnd_stmt->as.compound_stmt = stmt_block;
        return cmpnd_stmt;
    } else if (next_tok.kind == TOKEN_KW_BREAK) {
        advance_token(cd);
        expect_token(cd, TOKEN_SEMICOLON);

        Stmt *break_stmt = Stmt_create(STMT_BREAK);
        break_stmt->pos = next_tok.pos;
        break_stmt->as.break_stmt = NULL; // Loop label; for semantic analysis
        return break_stmt;
    } else if (next_tok.kind == TOKEN_KW_CONTINUE) {
        advance_token(cd);
        expect_token(cd, TOKEN_SEMICOLON);

        Stmt *cont_stmt = Stmt_create(STMT_CONTINUE);
        cont_stmt->pos = next_tok.pos;
        cont_stmt->as.continue_stmt = NULL; // Loop label; for semantic analysis
        return cont_stmt;
    } else if (next_tok.kind == TOKEN_KW_WHILE) {
        advance_token(cd);
        expect_token(cd, TOKEN_LEFT_PAREN);
        Expr *cond = parse_expression(cd, 0);
        expect_token(cd, TOKEN_RIGHT_PAREN);
        Stmt *body = parse_statement(cd);

        Stmt *while_stmt = Stmt_create(STMT_WHILE);
        while_stmt->pos = next_tok.pos;
        while_stmt->as.while_stmt.cond = cond;
        while_stmt->as.while_stmt.body = body;
        while_stmt->as.while_stmt.lbl = NULL; // Loop label; for semantic analysis
        return while_stmt;
    } else if (next_tok.kind == TOKEN_KW_DO) {
        advance_token(cd);
        Stmt *body = parse_statement(cd);
        expect_token(cd, TOKEN_KW_WHILE);
        expect_token(cd, TOKEN_LEFT_PAREN);
        Expr *cond = parse_expression(cd, 0);
        expect_token(cd, TOKEN_RIGHT_PAREN);
        expect_token(cd, TOKEN_SEMICOLON);

        Stmt *dwhile_stmt = Stmt_create(STMT_DOWHILE);
        dwhile_stmt->pos = next_tok.pos;
        dwhile_stmt->as.do_while_stmt.body = body;
        dwhile_stmt->as.do_while_stmt.cond = cond;
        dwhile_stmt->as.do_while_stmt.lbl = NULL; // Loop label; for semantic analysis
        return dwhile_stmt;
    } else if (next_tok.kind == TOKEN_KW_FOR) { 
        advance_token(cd);
        expect_token(cd, TOKEN_LEFT_PAREN);
        Token init_tok = peek_token(cd);
        ForInit init = {0};
        if (TOKENKIND_IS_TYPE_STORAGE(init_tok.kind)) {
            init.kind = FOR_INIT_DECL;
            StorageClass sc = parse_type_storage(cd);
            const String *ident = parse_identifier(cd);
            Decl *init_decl = parse_var_declaration(cd);
            init_decl->pos = init_tok.pos;
            init_decl->as.variable.identifier = ident;
            init_decl->as.variable.origin_name = ident;
            init_decl->as.variable.sc = sc;

            init.as.decl = init_decl;
        } else {
            init.as.exp = parse_optional_expression(cd, TOKEN_SEMICOLON);
            if (init.as.exp == NULL) {
                init.kind = FOR_INIT_NULL;
            } else {
                init.kind = FOR_INIT_EXP;
            }
        }
        Expr *cond = parse_optional_expression(cd, TOKEN_SEMICOLON);
        Expr *post = parse_optional_expression(cd, TOKEN_RIGHT_PAREN);
        Stmt *body = parse_statement(cd);
        Stmt *for_stmt = Stmt_create(STMT_FOR);
        for_stmt->pos = next_tok.pos;
        for_stmt->as.for_stmt.init = init;
        for_stmt->as.for_stmt.cond = cond;
        for_stmt->as.for_stmt.post = post;
        for_stmt->as.for_stmt.body = body;
        for_stmt->as.for_stmt.lbl = NULL; // Loop label; for semantic analysis
        return for_stmt;
    } else if (next_tok.kind == TOKEN_KW_SWITCH) {
        advance_token(cd);
        expect_token(cd, TOKEN_LEFT_PAREN);
        Expr *control_expr = parse_expression(cd, 0);
        expect_token(cd, TOKEN_RIGHT_PAREN);
        Stmt *body = parse_statement(cd);
        
        Stmt *switch_stmt = Stmt_create(STMT_SWITCH);
        switch_stmt->pos = next_tok.pos;
        switch_stmt->as.switch_stmt.ctrl_expr = control_expr;
        switch_stmt->as.switch_stmt.body = body;
        switch_stmt->as.switch_stmt.cases = NULL; // case labels; filled in semantic analysis
        switch_stmt->as.switch_stmt.lbl = NULL; // switch label; for semantic analysis
        return switch_stmt;
    } else if (next_tok.kind == TOKEN_KW_CASE) {
        advance_token(cd);
        Expr *lbl = parse_expression(cd, 0);
        expect_token(cd, TOKEN_OP_COLON);
        Stmt *stmt = parse_statement(cd);

        Stmt *case_stmt = Stmt_create(STMT_CASE);
        case_stmt->pos = next_tok.pos;
        case_stmt->as.case_stmt.stmt = stmt;
        case_stmt->as.case_stmt.lbl_expr = lbl;
        case_stmt->as.case_stmt.lbl = NULL; // switch label; for semantic analysis to fill in
        return case_stmt;
    } else if (next_tok.kind == TOKEN_KW_DEFAULT) {
        advance_token(cd);
        expect_token(cd, TOKEN_OP_COLON);
        Stmt *stmt = parse_statement(cd);

        Stmt *def_stmt = Stmt_create(STMT_DEFAULT);
        def_stmt->pos = next_tok.pos;
        def_stmt->as.default_stmt.stmt = stmt;
        def_stmt->as.default_stmt.lbl = NULL; // switch label; for semantic analysis to fill in
        return def_stmt;
    }

    Expr *expr = parse_expression(cd, 0);
    expect_token(cd, TOKEN_SEMICOLON);
    Stmt *expr_stmt = Stmt_create(STMT_EXPR);
    expr_stmt->pos = next_tok.pos;
    expr_stmt->as.expr = expr;

    return expr_stmt;
}

static ParamArray *parse_param_list(CompDriver *cd, const String *fn_name) {
    Token begin_param_list = peek_prev_token(cd);

    Token next_token = peek_token(cd);
    if (next_token.kind == TOKEN_KW_VOID) {
        advance_token(cd);
    }

    ParamArray *param_list = ParamArray_create();
    next_token = peek_token(cd);
    if (next_token.kind == TOKEN_RIGHT_PAREN) goto finalize_param_list;
    
    // NOTE: do-while? (look at parse args function)
    int p_cnt = 0;
    while (next_token.kind != TOKEN_RIGHT_PAREN) {
        p_cnt += 1;
        if (next_token.kind == TOKEN_OP_COMMA) {
            advance_token(cd);
            next_token = peek_token(cd);
        }
        if (next_token.kind == TOKEN_KW_EXTERN || next_token.kind == TOKEN_KW_STATIC) {
            // TODO: error: storage class in parameter list
            err_storage_class_on_param(&cd->errors, cd->tokenizer.src_path, next_token.pos);
            advance_token(cd);
            next_token = peek_token(cd);
            continue;
        }
        if (next_token.kind == TOKEN_LEFT_BRACE || next_token.kind == TOKEN_SEMICOLON
            || next_token.kind == TOKEN_EOF
        ) {
            err_unclosed_param_list(&cd->errors, cd->tokenizer.src_path, begin_param_list.pos,
                fn_name);
            break;
        } else if (next_token.kind == TOKEN_RIGHT_PAREN) {
            err_expected_parameter(&cd->errors, cd->tokenizer.src_path, next_token.pos);
            break;
        } else if (next_token.kind == TOKEN_RIGHT_BRACE) {
            err_unexpected_token(&cd->errors, cd->tokenizer.src_path, next_token);
            break;
        }
        expect_token(cd, TOKEN_KW_INT);
        next_token = peek_token(cd);
        const String *param_name = parse_identifier(cd);
        ParamArray_append(param_list, param_name);
        next_token = peek_token(cd);
    }

finalize_param_list:
    return param_list;
}

static Decl *parse_var_declaration(CompDriver *cd) {
    Decl *var_decl = Decl_create(DECL_VARIABLE);
    var_decl->as.variable.init = NULL;

    Token next_tok = peek_token(cd);
    if (next_tok.kind == TOKEN_OP_EQUAL) {
        advance_token(cd);
        Expr *expr = parse_expression(cd, 0);
        var_decl->as.variable.init = expr;
    }
    expect_token(cd, TOKEN_SEMICOLON);

    return var_decl;
}

// TODO: maybe split this into different functions?
static Decl *parse_declaration(CompDriver *cd) {
    //ssize_t decl_start_idx = cd->parser.curr_idx;
    Token first_tok = peek_token(cd);
    StorageClass sc = parse_type_storage(cd);

    const String *ident = parse_identifier(cd);
    Token paren_tok = peek_token(cd);
    if (paren_tok.kind == TOKEN_LEFT_PAREN) {
        advance_token(cd);
        Decl *fn_decl = Decl_create(DECL_FUNCTION);
        fn_decl->pos = first_tok.pos;
        fn_decl->as.fn.name = ident;
        fn_decl->as.fn.sc = sc;
        // TODO: static and extern disallowed in param list
        fn_decl->as.fn.params = parse_param_list(cd, fn_decl->as.fn.name);
        expect_token(cd, TOKEN_RIGHT_PAREN);
        Token next_tok = peek_token(cd);
        if (next_tok.kind == TOKEN_SEMICOLON) {
            fn_decl->as.fn.body = NULL;
            advance_token(cd);
        } else if (next_tok.kind == TOKEN_LEFT_BRACE) {
            Block *fn_body = parse_block(cd);
            fn_decl->as.fn.body = fn_body;
        } else {
            // TODO: error: what is the next token?
            fn_decl->as.fn.body = NULL;
        }
        return fn_decl;
    } else {
        //Parser_set_pos(cd, decl_start_idx);
        Decl *var_decl = parse_var_declaration(cd);
        var_decl->pos = first_tok.pos;
        var_decl->as.variable.sc = sc;
        var_decl->as.variable.identifier = ident;
        var_decl->as.variable.origin_name = ident;
        return var_decl;
    }

    return Decl_create(DECL_INVALID);
}

static BlockItem parse_block_item(CompDriver *cd) {
    BlockItem item = (BlockItem){ .kind = BLOCKITEM_INVALID };
    Token tok = peek_token(cd);
    if (TOKENKIND_IS_TYPE_STORAGE(tok.kind)) {
        Decl *decl = parse_declaration(cd);
        item.kind = BLOCKITEM_DECLARATION;
        item.as.declaration = decl;
    } else {
        Stmt *stmt = parse_statement(cd);
        item.kind = BLOCKITEM_STATEMENT;
        item.as.statement = stmt;
    }

    return item;
}

static Block *parse_block(CompDriver *cd) {
    expect_token(cd, TOKEN_LEFT_BRACE);
    Block *block = Block_create();
    Token tok = peek_token(cd);
    while (tok.kind != TOKEN_RIGHT_BRACE && tok.kind != TOKEN_EOF) {
        BlockItem item = parse_block_item(cd);
        Block_append(block, item);
        tok = peek_token(cd);
    }
    expect_token(cd, TOKEN_RIGHT_BRACE);
    return block;
}

void parse(CompDriver *cd) {
    Token peeked = peek_token(cd);
    while (peeked.kind != TOKEN_EOF) {
        Decl *decl = parse_declaration(cd);
        DeclArray_append(&cd->parser.ast_tu->decls, decl);
        peeked = peek_token(cd);
    }
    expect_token(cd, TOKEN_EOF);
}

// Parser management

void Parser_init(Parser *p) {
    p->ast_tu = (AstTU *)malloc(sizeof(AstTU));
    AstTU_init(p->ast_tu);
    p->curr_idx = 0;
    p->prev_idx = p->curr_idx-1;
}

void Parser_destroy(Parser *p) {
    if (p == NULL) return;
    if (p->ast_tu == NULL) return;
    AstTU_deinit(p->ast_tu);
    free(p->ast_tu);
}

void Parser_print_ast(Parser *p) {
    AstTU_print(p->ast_tu);
}
