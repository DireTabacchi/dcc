#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dd_string.h"
#include "token.h"

void TokenList_init(TokenList* tl) {
    tl->cap = 2;
    tl->len = 0;
    tl->toks = (Token *)calloc(tl->cap, sizeof(Token));
}

void TokenList_destroy(TokenList* tl) {
    if (tl == NULL) return;
    for (size_t i = 0; i < tl->len; i++){
        switch (tl->toks[i].kind) {
        case TOKEN_CONSTANT:
            String_free((String *)tl->toks[i].text);
            free((String *)tl->toks[i].text);
            break;
        case TOKEN_OPERATORS_BEGIN: case TOKEN_OPERATORS_END:
        case TOKEN_KEYWORDS_BEGIN: case TOKEN_KEYWORDS_END:
        case TOKEN_OPERATORS_ASSIGN_BEGIN: case TOKEN_OPERATORS_ASSIGN_END:
        case TOKEN_OPERATORS_BINARY_BEGIN: case TOKEN_OPERATORS_BINARY_END:
        case TOKEN_OPERATORS_UNARY_BEGIN: case TOKEN_OPERATORS_UNARY_END:
        case TOKEN_TYPE_SPECIFIERS_BEGIN: case TOKEN_TYPE_SPECIFIERS_END:
        case TOKEN_STORAGE_CLASS_BEGIN: case TOKEN_STORAGE_CLASS_END:
        case TOKENKIND_LEN:
            /* Above aren't interned, nor do they have any string memory */
        case TOKEN_INVALID: case TOKEN_UNKNOWN:
        case TOKEN_KW_INT: case TOKEN_KW_VOID: case TOKEN_KW_RETURN:
        case TOKEN_IDENTIFIER:
        case TOKEN_LEFT_PAREN: case TOKEN_RIGHT_PAREN:
        case TOKEN_LEFT_BRACE: case TOKEN_RIGHT_BRACE:
        case TOKEN_SEMICOLON:
        case TOKEN_OP_EQUAL: case TOKEN_OP_PLUS_EQUAL:
        case TOKEN_OP_MINUS_EQUAL: case TOKEN_OP_ASTERISK_EQUAL: case TOKEN_OP_SLASH_EQUAL:
        case TOKEN_OP_PERCENT_EQUAL: case TOKEN_OP_AMPERSAND_EQUAL: case TOKEN_OP_BAR_EQUAL:
        case TOKEN_OP_CARET_EQUAL: case TOKEN_OP_LSHFT_EQUAL: case TOKEN_OP_RSHFT_EQUAL:
        case TOKEN_OP_COMPLEMENT:
        case TOKEN_OP_PLUS: case TOKEN_OP_ASTERISK: case TOKEN_OP_SLASH: case TOKEN_OP_PERCENT:
        case TOKEN_OP_DECREMENT: case TOKEN_OP_INCREMENT: case TOKEN_OP_MINUS:
        case TOKEN_OP_DOUBLE_EQUAL: case TOKEN_OP_EXCLAMATION_EQUAL: case TOKEN_OP_EXCLAMATION:
        case TOKEN_OP_DOUBLE_AMP: case TOKEN_OP_AMPERSAND: case TOKEN_OP_DOUBLE_BAR:
        case TOKEN_OP_BAR: case TOKEN_OP_CARET: case TOKEN_OP_LSHFT: case TOKEN_OP_LTE:
        case TOKEN_OP_LT: case TOKEN_OP_RSHFT: case TOKEN_OP_GTE: case TOKEN_OP_GT:
        case TOKEN_OP_QUESTION: case TOKEN_OP_COLON: case TOKEN_OP_COMMA: case TOKEN_KW_IF:
        case TOKEN_KW_ELSE: case TOKEN_KW_DO: case TOKEN_KW_WHILE: case TOKEN_KW_FOR:
        case TOKEN_KW_BREAK: case TOKEN_KW_CONTINUE: case TOKEN_KW_SWITCH: case TOKEN_KW_CASE:
        case TOKEN_KW_DEFAULT: case TOKEN_KW_GOTO:
        case TOKEN_KW_STATIC: case TOKEN_KW_EXTERN:
        case TOKEN_EOF:
            /* Interned String */
            break;
        }
    }
    free(tl->toks);
}

void TokenList_append(TokenList* tl, Token tok) {
    if (tl == NULL) return;

    if (tl->len == tl->cap) {
        size_t new_cap = tl->cap / 2 + tl->cap;
        Token* new_toks = (Token*)calloc(new_cap, sizeof(Token));
        memcpy(new_toks, tl->toks, tl->len*sizeof(Token));
        free(tl->toks);
        tl->toks = new_toks;
        tl->cap = new_cap;
    }

    memcpy(&tl->toks[tl->len], &tok, sizeof(Token));
    tl->len += 1;
}

void TokenList_print(TokenList *tl) {
    printf("%-24s | %-32s | %-11s | %-6s | %-6s\n", "Token Kind", "Token Text", "File Offset", "Line", "Column");
    printf("===========================================================================================\n");
    for (size_t tl_idx = 0; tl_idx < tl->len; tl_idx++) {
        Token tok = tl->toks[tl_idx];
        printf("%-24s | %-32.32s | %-11d | %-6d | %-6d\n", token_literals[tok.kind].cstr, tok.text->cstr, tok.pos.offset, tok.pos.line, tok.pos.column);
    }
}
