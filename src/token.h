#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>

#include "dd_string.h"

#define TOKENKINDS \
    TOKENKIND(TOKEN_UNKNOWN, "UNKNOWN"), \
    TOKENKIND(TOKEN_EOF, "EOF"), \
    TOKENKIND(TOKEN_LEFT_PAREN, "("), \
    TOKENKIND(TOKEN_RIGHT_PAREN, ")"), \
    TOKENKIND(TOKEN_LEFT_BRACE, "{"), \
    TOKENKIND(TOKEN_RIGHT_BRACE, "}"), \
    TOKENKIND(TOKEN_SEMICOLON, ";"), \
\
    TOKENKIND(TOKEN_OPERATORS_BEGIN, ""), \
    TOKENKIND(TOKEN_OP_EQUAL, "="), \
    TOKENKIND(TOKEN_OP_COMPLEMENT, "~"), \
    TOKENKIND(TOKEN_OP_MINUS, "-"), \
    TOKENKIND(TOKEN_OP_EXCLAMATION, "!"), \
    TOKENKIND(TOKEN_OP_DECREMENT, "--"), \
    TOKENKIND(TOKEN_OP_PLUS, "+"), \
    TOKENKIND(TOKEN_OP_ASTERISK, "*"), \
    TOKENKIND(TOKEN_OP_SLASH, "/"), \
    TOKENKIND(TOKEN_OP_PERCENT, "%"), \
    TOKENKIND(TOKEN_OP_DOUBLE_AMP, "&&"), \
    TOKENKIND(TOKEN_OP_DOUBLE_BAR, "||"), \
    TOKENKIND(TOKEN_OP_DOUBLE_EQUAL, "=="), \
    TOKENKIND(TOKEN_OP_EXCLAMATION_EQUAL, "!="), \
    TOKENKIND(TOKEN_OP_LT, "<"), \
    TOKENKIND(TOKEN_OP_GT, ">"), \
    TOKENKIND(TOKEN_OP_LTE, "<="), \
    TOKENKIND(TOKEN_OP_GTE, ">="), \
    TOKENKIND(TOKEN_OP_AMPERSAND, "&"), \
    TOKENKIND(TOKEN_OP_BAR, "|"), \
    TOKENKIND(TOKEN_OP_CARET, "^"), \
    TOKENKIND(TOKEN_OP_LSHFT, "<<"), \
    TOKENKIND(TOKEN_OP_RSHFT, ">>"), \
    TOKENKIND(TOKEN_OPERATORS_END, ""), \
\
    TOKENKIND(TOKEN_KEYWORDS_BEGIN, ""), \
    TOKENKIND(TOKEN_KW_INT, "int"), \
    TOKENKIND(TOKEN_KW_VOID, "void"), \
    TOKENKIND(TOKEN_KW_RETURN, "return"), \
    TOKENKIND(TOKEN_KEYWORDS_END, ""), \
\
    TOKENKIND(TOKEN_IDENTIFIER, "identifier"), \
    TOKENKIND(TOKEN_CONSTANT, "constant"), \
    TOKENKIND(TOKENKIND_LEN, "")

typedef enum tokenkind_ {
#define TOKENKIND(tk, sl) tk
    TOKENKINDS
#undef TOKENKIND
} TokenKind;

static char *token_literal_list[] = {
#define TOKENKIND(tk, sl) (char *)sl
    TOKENKINDS
#undef TOKENKIND
};

typedef struct pos_ {
    int offset;
    int line;
    int column;
} TokenPos;

typedef struct token_ {
    TokenKind kind;
    TokenPos pos;
    String text;
} Token;

bool Token_is_operator(Token t);

typedef struct tokenList_ {
    Token *toks;
    size_t len;
    size_t cap;
} TokenList;

void TokenList_init(TokenList *tl);
void TokenList_destroy(TokenList* tl);
void TokenList_append(TokenList* tl, Token tok);
void TokenList_print(TokenList *tl);

#endif // TOKEN_H
