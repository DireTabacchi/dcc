#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "comp_driver.h"
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
        fprintf(stderr, "[%s:%d:%d] \x1B[1;31merror:\x1B[0m %s\n", err.file.cstr, err.pos.line, err.pos.column, err.desc.cstr);
    }
}

void err_stray_char(ErrorList *el, String srcname, Position pos, char stray) {
    Error err = {0};
    err.pos = pos;
    err.file = String_copy(srcname);
    err.desc = String_init_length(20);
    snprintf(err.desc.cstr, err.desc.len+1, "stray `%c` in program", stray);
    ErrorList_append(el, err);
}

void err_invalid_const_ident(ErrorList *el, String srcname, Position pos, String invalid_name) {
    Error err = {0};
    err.file = String_copy(srcname);
    err.pos = pos;
    err.desc = String_init_length(invalid_name.len + 33);
    snprintf(err.desc.cstr, err.desc.len+1, "invalid constant or identifier `%s`", invalid_name.cstr);
    ErrorList_append(el, err);
}

void err_expected_token(ErrorList *el, String srcname, TokenKind expected, Token actual) {
    Error err = {0};
    err.file = String_copy(srcname);
    err.pos = actual.pos;
    String expected_token_kind = token_literals[expected];
    String found_token_kind;
    if (actual.kind == TOKEN_IDENTIFIER) {
        found_token_kind = String_init_length(actual.text->len + 13);
        snprintf(found_token_kind.cstr, found_token_kind.len+1, "identifier `%s`", actual.text->cstr);
    } else if (actual.kind == TOKEN_CONSTANT) { 
        found_token_kind = String_init_length(actual.text->len + 11);
        snprintf(found_token_kind.cstr, found_token_kind.len+1, "constant `%s`", actual.text->cstr);
    } else if (actual.kind == TOKEN_EOF) {
        found_token_kind = String_init_cstr("end of input");
    } else {
        found_token_kind = String_copy(token_literals[actual.kind]);
    }
    err.desc = String_init_length(expected_token_kind.len + found_token_kind.len + 21);
    snprintf(err.desc.cstr, err.desc.len+1, "expected %s, but found %s",
        expected_token_kind.cstr, found_token_kind.cstr);
    ErrorList_append(el, err);
    String_free(&found_token_kind);
}

void err_expected_expression(ErrorList *el, String srcname, Token actual) {
    Error exp_err = {0};
    exp_err.file = String_copy(srcname);
    exp_err.pos = actual.pos;
    exp_err.desc = String_init_length(34 + token_literals[actual.kind].len);
    snprintf(exp_err.desc.cstr, exp_err.desc.len+1, "expected an expression, but got `%s`", token_literals[actual.kind].cstr);
    ErrorList_append(el, exp_err);
}

void err_redeclared_variable(ErrorList *el, String srcname, Position pos, String varname) {
    Error redec_err = {0};
    redec_err.file = String_copy(srcname);
    redec_err.pos = pos;
    redec_err.desc = String_init_length(28 + varname.len);
    snprintf(redec_err.desc.cstr, redec_err.desc.len+1,
        "redeclaration of variable `%s`", varname.cstr);
    ErrorList_append(el, redec_err);
}

void err_undeclared_variable(ErrorList *el, String srcname, Position pos, String varname) {
    Error redec_err = {0};
    redec_err.file = String_copy(srcname);
    redec_err.pos = pos;
    redec_err.desc = String_init_length(27 + varname.len);
    snprintf(redec_err.desc.cstr, redec_err.desc.len+1,
        "identifier `%s` is undeclared", varname.cstr);
    ErrorList_append(el, redec_err);
}

void err_assign_invalid_lvalue(ErrorList *el, String srcname, Position pos) {
    Error redec_err = {0};
    redec_err.file = String_copy(srcname);
    redec_err.pos = pos;
    redec_err.desc = String_init_cstr("assignment requires a valid lvalue");
    ErrorList_append(el, redec_err);
}

void err_decr_not_lvalue(CompDriver *cd, Position pos) {
    Error lvalue_err = {0};
    lvalue_err.file = String_copy(cd->tokenizer.src_path);
    lvalue_err.pos = pos;
    lvalue_err.desc = String_init_cstr("lvalue required as decrement operand");
    ErrorList_append(&cd->errors, lvalue_err);
}

void err_incr_not_lvalue(CompDriver *cd, Position pos) {
    Error lvalue_err = {0};
    lvalue_err.file = String_copy(cd->tokenizer.src_path);
    lvalue_err.pos = pos;
    lvalue_err.desc = String_init_cstr("lvalue required as increment operand");
    ErrorList_append(&cd->errors, lvalue_err);
}

void err_duplicate_label(CompDriver *cd, Position pos, const SymEntry *lbl) {
    Error dup_lbl_err = {0};
    dup_lbl_err.file = String_copy(cd->tokenizer.src_path);
    dup_lbl_err.pos = pos;
    int line_len = integer_len(lbl->pos.line);
    int col_len = integer_len(lbl->pos.column);
    // dup_lbl_err.desc.len = 58 (msg) + lbl.key.len + numlen(lbl.pos.line) + numlen(lbl.pos.column)
    dup_lbl_err.desc = String_init_length(58+lbl->key->len+line_len+col_len);

    snprintf(dup_lbl_err.desc.cstr, dup_lbl_err.desc.len+1,
        "duplicate definition of label `%s`; label first defined at %d:%d",
        lbl->key->cstr, lbl->pos.line, lbl->pos.column);
    ErrorList_append(&cd->errors, dup_lbl_err);
}

void err_undefined_label(CompDriver *cd, Position pos, const String *lbl) {
    Error undef_err = {0};
    undef_err.file = String_copy(cd->tokenizer.src_path);
    undef_err.pos = pos;
    undef_err.desc = String_init_length(25+lbl->len);
    snprintf(undef_err.desc.cstr, undef_err.desc.len+1,
        "use of undefined label `%s`", lbl->cstr);
    ErrorList_append(&cd->errors, undef_err);
}
