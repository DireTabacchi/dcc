#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"
#include "dcc_error.h"

void ErrorList_init(ErrorList* tl) {
    tl->cap = 2;
    tl->len = 0;
    tl->errors = (Error*)calloc(tl->cap, sizeof(Error));
}

void ErrorList_destroy(ErrorList* el) {
    if (el == NULL) return;
    for (size_t i = 0; i < el->len; i++){
        String_free(&el->errors[i].file);
        String_free(&el->errors[i].desc);
    }
    free(el->errors);
}

void ErrorList_append(ErrorList* el, Error err) {
    if (el == NULL) return;

    if (el->len == el->cap) {
        size_t new_cap = el->cap / 2 + el->cap;
        Error* new_errs = (Error*)calloc(new_cap, sizeof(Error));
        memcpy(new_errs, el->errors, el->len*sizeof(Error));
        free(el->errors);
        el->errors = new_errs;
        el->cap = new_cap;
    }

    memcpy(&el->errors[el->len], &err, sizeof(Error));
    el->len += 1;
}

void ErrorList_print(ErrorList *el) {
    for (size_t el_idx = 0; el_idx < el->len; el_idx++) {
        Error err = el->errors[el_idx];
        printf("[%s:%d:%d] \x1B[1;31merror:\x1B[0m %s\n", err.file.cstr, err.pos.line, err.pos.column, err.desc.cstr);
    }
}
