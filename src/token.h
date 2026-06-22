#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>

#include "dd_string.h"

typedef enum tokenKind_ {
    TOKEN_UNKOWN,           // Unkown token
    TOKEN_EOF,              // EOF (End Of File)
    // Punctuation
    TOKEN_LEFT_PAREN,       // (
    TOKEN_RIGHT_PAREN,      // )
    TOKEN_LEFT_BRACE,       // {
    TOKEN_RIGHT_BRACE,      // }
    TOKEN_SEMICOLON,        // ;
    // -- Operators
    TOKEN_OP_COMPLEMENT,    // ~
    TOKEN_OP_MINUS,         // -
    TOKEN_OP_DECREMENT,     // --
    TOKEN_OP_PLUS,          // +
    TOKEN_OP_ASTERISK,      // *
    TOKEN_OP_SLASH,         // /
    TOKEN_OP_PERCENT,       // %
    // Keywords
    TOKEN_KW_INT,           // int
    TOKEN_KW_VOID,          // void
    TOKEN_KW_RETURN,        // return
    // Misc (?)
    TOKEN_IDENTIFIER,       // _exampleIdentifier_123
    TOKEN_CONSTANT,         // 123

    TOKENKIND_LEN           // length of this list of TokenKinds
} TokenKind;

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

typedef struct tokenList_ {
    Token *toks;
    size_t len;
    size_t cap;
} TokenList;

void TokenList_init(TokenList *tl);
void TokenList_destroy(TokenList* tl);
void TokenList_append(TokenList* tl, Token tok);
void TokenList_print(TokenList *tl);

static char *token_literal_list[TOKENKIND_LEN] = {
    (char *)"UNKNOWN",
    (char *)"EOF",
    // Punctuation
    (char *)"(",            // TOKEN_LEFT_PAREN
    (char *)")",            // TOKEN_RIGHT_PAREN
    (char *)"{",            // TOKEN_LEFT_BRACE
    (char *)"}",            // TOKEN_RIGHT_BRACE
    (char *)";",            // TOKEN_SEMICOLON
    // -- Operators
    (char *)"~",            // TOKEN_OP_COMPLEMENT
    (char *)"-",            // TOKEN_OP_MINUS
    (char *)"--",           // TOKEN_OP_DECREMENT
    (char *)"+",            // TOKEN_OP_PLUS
    (char *)"*",            // TOKEN_OP_ASTERISK
    (char *)"/",            // TOKEN_OP_SLASH
    (char *)"%",            // TOKEN_OP_PERCENT
    // Keywords
    (char *)"int",          // TOKEN_KW_INT
    (char *)"void",         // TOKEN_KW_VOID
    (char *)"return",       // TOKEN_KW_RETURN
    // Misc (?)
    (char *)"identifier",   // TOKEN_IDENTIFIER
    (char *)"constant",     // TOKEN_CONSTANT
};

#endif // TOKEN_H
