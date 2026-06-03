#include <stdlib.h>
#include <string.h>

#include "dd_string.h"
#include "token.h"

void TokenList_init(TokenList* tl) {
    tl->cap = 2;
    tl->len = 0;
    tl->toks = (Token*)calloc(tl->cap, sizeof(Token));
}

void TokenList_destroy(TokenList* tl) {
    if (tl == NULL) return;
    for (size_t i = 0; i < tl->len; i++){
        String_free(&tl->toks[i].text);
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
