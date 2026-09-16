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
        if (el->errors[i].note != NULL) {
            String_free(&el->errors[i].note->file);
            String_free(&el->errors[i].note->desc);
            free(el->errors[i].note);
        }
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

static void Error_print(Error *err) {
    switch (err->err_lvl) {
    case LVL_ERROR:
        fprintf(stderr, "\x1B[1m[%s:%d:%d] \x1B[1;31merror:\x1B[0m %s\n", err->file.cstr, err->pos.line, err->pos.column, err->desc.cstr);
        break;
    case LVL_INFO:
        fprintf(stderr, "\x1B[1m[%s:%d:%d] \x1B[1;36mnote:\x1B[0m %s\n", err->file.cstr, err->pos.line, err->pos.column, err->desc.cstr);
        break;
    }
}

void ErrorList_print(ErrorList *el) {
    for (size_t el_idx = 0; el_idx < el->len; el_idx++) {
        Error err = el->errors[el_idx];
        Error_print(&err);
        if (err.note != NULL) Error_print(err.note);
    }
}

// Tokenizer errors

void err_stray_char(ErrorList *el, String srcname, Position pos, char stray) {
    Error err = {0};
    err.err_lvl = LVL_ERROR;
    err.note = NULL;
    err.pos = pos;
    err.file = String_copy(srcname);
    err.desc = String_init_length(20);
    snprintf(err.desc.cstr, err.desc.len+1, "stray `%c` in program", stray);
    ErrorList_append(el, err);
}

void err_invalid_const_ident(ErrorList *el, String srcname, Position pos, String invalid_name) {
    Error err = {0};
    err.err_lvl = LVL_ERROR;
    err.note = NULL;
    err.file = String_copy(srcname);
    err.pos = pos;
    err.desc = String_init_length(invalid_name.len + 33);
    snprintf(err.desc.cstr, err.desc.len+1, "invalid constant or identifier `%s`", invalid_name.cstr);
    ErrorList_append(el, err);
}

// Parser errors

void err_expected_token(ErrorList *el, String srcname, TokenKind expected, Token actual) {
    Error err = {0};
    err.err_lvl = LVL_ERROR;
    err.note = NULL;
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
    exp_err.err_lvl = LVL_ERROR;
    exp_err.note = NULL;
    exp_err.file = String_copy(srcname);
    exp_err.pos = actual.pos;
    exp_err.desc = String_init_length(34 + token_literals[actual.kind].len);
    snprintf(exp_err.desc.cstr, exp_err.desc.len+1, "expected an expression, but got `%s`", token_literals[actual.kind].cstr);
    ErrorList_append(el, exp_err);
}

void err_unclosed_param_list(ErrorList *el, String srcname, Position pos, const String *fn_name) {
    Error unc_err = {0};
    unc_err.err_lvl = LVL_ERROR;
    unc_err.note = NULL;
    unc_err.file = String_copy(srcname);
    unc_err.pos = pos;
    unc_err.desc = String_init_length(44 + fn_name->len);
    snprintf(unc_err.desc.cstr, unc_err.desc.len+1,
        "parameter list for function `%s` is not closed", fn_name->cstr);
    ErrorList_append(el, unc_err);
}

void err_expected_parameter(ErrorList *el, String srcname, Position pos) {
    Error exp_err = {0};
    exp_err.err_lvl = LVL_ERROR;
    exp_err.note = NULL;
    exp_err.file = String_copy(srcname);
    exp_err.pos = pos;
    exp_err.desc = String_init_length(43);
    snprintf(exp_err.desc.cstr, exp_err.desc.len+1,
        "expected a parameter declaration before `)`");
    ErrorList_append(el, exp_err);
}
void err_unexpected_token(ErrorList *el, String srcname, Token tok) {
    Error unexp_err = {0};
    unexp_err.err_lvl = LVL_ERROR;
    unexp_err.note = NULL;
    unexp_err.file = String_copy(srcname);
    unexp_err.pos = tok.pos;
    unexp_err.desc = String_init_length(33+tok.text->len);
    snprintf(unexp_err.desc.cstr, unexp_err.desc.len+1,
        "encountered unexpected token `%s`", tok.text->cstr);
    ErrorList_append(el, unexp_err);
}

// Semantics errors

void err_redeclared_variable(ErrorList *el, String srcname, Position pos, String varname) {
    Error redec_err = {0};
    redec_err.err_lvl = LVL_ERROR;
    redec_err.note = NULL;
    redec_err.file = String_copy(srcname);
    redec_err.pos = pos;
    redec_err.desc = String_init_length(28 + varname.len);
    snprintf(redec_err.desc.cstr, redec_err.desc.len+1,
        "redeclaration of variable `%s`", varname.cstr);
    ErrorList_append(el, redec_err);
}

void err_undeclared_variable(ErrorList *el, String srcname, Position pos, String varname) {
    Error undec_err = {0};
    undec_err.err_lvl = LVL_ERROR;
    undec_err.note = NULL;
    undec_err.file = String_copy(srcname);
    undec_err.pos = pos;
    undec_err.desc = String_init_length(27 + varname.len);
    snprintf(undec_err.desc.cstr, undec_err.desc.len+1,
        "identifier `%s` is undeclared", varname.cstr);
    ErrorList_append(el, undec_err);
}

void err_assign_invalid_lvalue(ErrorList *el, String srcname, Position pos) {
    Error inv_err = {0};
    inv_err.err_lvl = LVL_ERROR;
    inv_err.note = NULL;
    inv_err.file = String_copy(srcname);
    inv_err.pos = pos;
    inv_err.desc = String_init_cstr("assignment requires a valid lvalue");
    ErrorList_append(el, inv_err);
}

void err_decr_not_lvalue(CompDriver *cd, Position pos) {
    Error lvalue_err = {0};
    lvalue_err.err_lvl = LVL_ERROR;
    lvalue_err.note = NULL;
    lvalue_err.file = String_copy(cd->tokenizer.src_path);
    lvalue_err.pos = pos;
    lvalue_err.desc = String_init_cstr("lvalue required as decrement operand");
    ErrorList_append(&cd->errors, lvalue_err);
}

void err_incr_not_lvalue(CompDriver *cd, Position pos) {
    Error lvalue_err = {0};
    lvalue_err.err_lvl = LVL_ERROR;
    lvalue_err.note = NULL;
    lvalue_err.file = String_copy(cd->tokenizer.src_path);
    lvalue_err.pos = pos;
    lvalue_err.desc = String_init_cstr("lvalue required as increment operand");
    ErrorList_append(&cd->errors, lvalue_err);
}

void err_duplicate_label(CompDriver *cd, Position pos, const SymEntry *lbl) {
    Error dup_lbl_err = {0};
    dup_lbl_err.err_lvl = LVL_ERROR;
    dup_lbl_err.note = NULL;
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
    undef_err.err_lvl = LVL_ERROR;
    undef_err.note = NULL;
    undef_err.file = String_copy(cd->tokenizer.src_path);
    undef_err.pos = pos;
    undef_err.desc = String_init_length(25+lbl->len);
    snprintf(undef_err.desc.cstr, undef_err.desc.len+1,
        "use of undefined label `%s`", lbl->cstr);
    ErrorList_append(&cd->errors, undef_err);
}

void err_break_not_in_loop_switch(CompDriver *cd, Position pos) {
    Error break_err = {0};
    break_err.err_lvl = LVL_ERROR;
    break_err.note = NULL;
    break_err.file = String_copy(cd->tokenizer.src_path);
    break_err.pos = pos;
    break_err.desc = String_init_cstr("`break` not used in loop or switch");
    ErrorList_append(&cd->errors, break_err);
}

void err_continue_not_in_loop(CompDriver *cd, Position pos) {
    Error continue_err = {0};
    continue_err.err_lvl = LVL_ERROR;
    continue_err.note = NULL;
    continue_err.file = String_copy(cd->tokenizer.src_path);
    continue_err.pos = pos;
    continue_err.desc = String_init_cstr("`continue` not used in loop");
    ErrorList_append(&cd->errors, continue_err);
}

void err_case_not_in_switch(CompDriver *cd, Position pos) {
    Error case_err = {0};
    case_err.err_lvl = LVL_ERROR;
    case_err.note = NULL;
    case_err.file = String_copy(cd->tokenizer.src_path);
    case_err.pos = pos;
    case_err.desc = String_init_cstr("`case` not used in `switch`");
    ErrorList_append(&cd->errors, case_err);
}

void err_default_not_in_switch(CompDriver *cd, Position pos) {
    Error def_err = {0};
    def_err.err_lvl = LVL_ERROR;
    def_err.note = NULL;
    def_err.file = String_copy(cd->tokenizer.src_path);
    def_err.pos = pos;
    def_err.desc = String_init_cstr("`default` not used in `switch`");
    ErrorList_append(&cd->errors, def_err);
}

void err_case_lbl_not_constant(CompDriver *cd, Position pos) {
    Error const_err = {0};
    const_err.err_lvl = LVL_ERROR;
    const_err.note = NULL;
    const_err.file = String_copy(cd->tokenizer.src_path);
    const_err.pos = pos;
    const_err.desc = String_init_cstr("case label must be an integer constant\n"
        "\tNOTE: constant expressions are not yet supported.\n");
    ErrorList_append(&cd->errors, const_err);
}

void err_duplicate_case(CompDriver *cd, Position pos, int val) {
    Error dup_err = {0};
    dup_err.err_lvl = LVL_ERROR;
    dup_err.note = NULL;
    dup_err.file = String_copy(cd->tokenizer.src_path);
    dup_err.pos = pos;
    int case_len = integer_len(val);
    dup_err.desc = String_init_length(28 + case_len);
    snprintf(dup_err.desc.cstr, dup_err.desc.len+1, "duplicate case with value `%d`", val);
    ErrorList_append(&cd->errors, dup_err);
}

void err_duplicate_default(CompDriver *cd, Position pos) {
    Error dup_err = {0};
    dup_err.err_lvl = LVL_ERROR;
    dup_err.note = NULL;
    dup_err.file = String_copy(cd->tokenizer.src_path);
    dup_err.pos = pos;
    dup_err.desc = String_init_cstr("default case is a duplicate");
    ErrorList_append(&cd->errors, dup_err);
}

void err_redefined_param(
    ErrorList *el,
    String srcname,
    Position pos,
    const String *param,
    const String *fn_name
) {
    Error redef_err = {0};
    redef_err.err_lvl = LVL_ERROR;
    redef_err.note = NULL;
    redef_err.file = String_copy(srcname);
    redef_err.pos = pos;
    redef_err.desc = String_init_length(44 + param->len + fn_name->len);
    snprintf(redef_err.desc.cstr, redef_err.desc.len+1,
        "redefinition of parameter `%s` for function `%s`",
        param->cstr, fn_name->cstr);
    ErrorList_append(el, redef_err);
}

void err_undeclared_function(ErrorList *el, String srcname, Position pos, const String *fn_name) {
    Error unfn_err = {0};
    unfn_err.err_lvl = LVL_ERROR;
    unfn_err.note = NULL;
    unfn_err.file = String_copy(srcname);
    unfn_err.pos = pos;
    unfn_err.desc = String_init_length(29 + fn_name->len);
    snprintf(unfn_err.desc.cstr, unfn_err.desc.len+1,
        "use of undeclared function `%s`", fn_name->cstr);
    ErrorList_append(el, unfn_err);
}

void err_var_redeclared_as_fn(ErrorList *el, String srcname, Position pos, const String *fn_name) {
    Error redec_err = {0};
    redec_err.err_lvl = LVL_ERROR;
    redec_err.note = NULL;
    redec_err.file = String_copy(srcname);
    redec_err.pos = pos;
    redec_err.desc = String_init_length(45 + fn_name->len);
    snprintf(redec_err.desc.cstr, redec_err.desc.len+1,
        "function `%s` previously declared as a variable", fn_name->cstr);
    ErrorList_append(el, redec_err);
}

void err_object_not_function(ErrorList *el, String srcname, Position pos, const String *name) {
    Error not_fn_err = {0};
    not_fn_err.err_lvl = LVL_ERROR;
    not_fn_err.note = NULL;
    not_fn_err.file = String_copy(srcname);
    not_fn_err.pos = pos;
    not_fn_err.desc = String_init_length(53+name->len);
    snprintf(not_fn_err.desc.cstr, not_fn_err.desc.len+1,
        "object `%s` called as a function, but is not a function", name->cstr);
    ErrorList_append(el, not_fn_err);
}

void err_too_few_args(
    ErrorList *el,
    String srcname,
    Position pos,
    const String *fn_name,
    size_t expected,
    size_t actual
) {
    Error arity_err = {0};
    arity_err.err_lvl = LVL_ERROR;
    arity_err.note = NULL;
    arity_err.file = String_copy(srcname);
    arity_err.pos = pos;

    int expected_len = integer_len(expected);
    int actual_len = integer_len(actual);

    arity_err.desc = String_init_length(55+fn_name->len+expected_len+actual_len);
    snprintf(arity_err.desc.cstr, arity_err.desc.len+1,
        "too few arguments for function `%s`; expected %ld, but have %ld",
        fn_name->cstr, expected, actual);
    ErrorList_append(el, arity_err);
}

void err_too_many_args(
    ErrorList *el,
    String srcname,
    Position pos,
    const String *fn_name,
    size_t expected,
    size_t actual
) {
    Error arity_err = {0};
    arity_err.err_lvl = LVL_ERROR;
    arity_err.note = NULL;
    arity_err.file = String_copy(srcname);
    arity_err.pos = pos;

    int expected_len = integer_len(expected);
    int actual_len = integer_len(actual);

    arity_err.desc = String_init_length(56+fn_name->len+expected_len+actual_len);
    snprintf(arity_err.desc.cstr, arity_err.desc.len+1,
        "too many arguments for function `%s`; expected %ld, but have %ld",
        fn_name->cstr, expected, actual);
    ErrorList_append(el, arity_err);
}

void err_object_not_variable(ErrorList *el, String srcname, Position pos, const String *obj_name) {
    Error not_var_err = {0};
    not_var_err.err_lvl = LVL_ERROR;
    not_var_err.note = NULL;
    not_var_err.file = String_copy(srcname);
    not_var_err.pos = pos;
    not_var_err.desc = String_init_length(20+obj_name->len);

    snprintf(not_var_err.desc.cstr, not_var_err.desc.len+1, "`%s` is not a variable",
        obj_name->cstr);
    ErrorList_append(el, not_var_err);
}

void err_nested_fn_definition(ErrorList *el, String srcname, Position pos) {
    Error nested_err = {0};
    nested_err.err_lvl = LVL_ERROR;
    nested_err.note = NULL;
    nested_err.file = String_copy(srcname);
    nested_err.pos = pos;
    nested_err.desc = String_init_cstr("ISO C does not allow for nested function definitions");
    ErrorList_append(el, nested_err);
}

void err_assign_to_fn(ErrorList *el, String srcname, Position pos, const String *obj_name) {
    Error assign_fn_err = {0};
    assign_fn_err.err_lvl = LVL_ERROR;
    assign_fn_err.note = NULL;
    assign_fn_err.file = String_copy(srcname);
    assign_fn_err.pos = pos;
    assign_fn_err.desc = String_init_length(48+obj_name->len);
    snprintf(assign_fn_err.desc.cstr, assign_fn_err.desc.len+1,
        "cannot assign a value to `%s`, which is a function", obj_name->cstr);
    ErrorList_append(el, assign_fn_err);
}

void err_assign_fn_to_var(ErrorList *el, String srcname, Position pos, const String *obj_name) {
    Error assign_fn_err = {0};
    assign_fn_err.err_lvl = LVL_ERROR;
    assign_fn_err.note = NULL;
    assign_fn_err.file = String_copy(srcname);
    assign_fn_err.pos = pos;
    assign_fn_err.desc = String_init_length(39+obj_name->len);
    snprintf(assign_fn_err.desc.cstr, assign_fn_err.desc.len+1,
        "cannot assign function `%s` to a variable", obj_name->cstr);
    ErrorList_append(el, assign_fn_err);
}

void err_incompatible_fn_types(ErrorList *el, String srcname, Position pos, const String *fn_name) {
    Error incom_fn_err = {0};
    incom_fn_err.err_lvl = LVL_ERROR;
    incom_fn_err.note = NULL;
    incom_fn_err.file = String_copy(srcname);
    incom_fn_err.pos = pos;
    incom_fn_err.desc = String_init_length(33+fn_name->len);
    snprintf(incom_fn_err.desc.cstr, incom_fn_err.desc.len+1,
        "conflicting types for function `%s`", fn_name->cstr);
    ErrorList_append(el, incom_fn_err);
}

// TODO: give Errors a note?
void err_fn_redefinition(
    ErrorList *el,
    String srcname,
    Position pos,
    const String *fn_name,
    Position prev_pos
) {
    Error redef_err = {0};
    redef_err.err_lvl = LVL_ERROR;
    redef_err.file = String_copy(srcname);
    redef_err.pos = pos;
    redef_err.desc = String_init_length(27+fn_name->len);

    snprintf(redef_err.desc.cstr, redef_err.desc.len+1,
        "redefinition of function `%s`", fn_name->cstr);

    Error *note_err = malloc(sizeof(Error));
    note_err->err_lvl = LVL_INFO;
    note_err->note = NULL;
    note_err->file = String_copy(srcname);
    note_err->pos = prev_pos;
    note_err->desc = String_init_length(35+fn_name->len);

    snprintf(note_err->desc.cstr, note_err->desc.len+1,
        "function `%s` previously defined here", fn_name->cstr);
    redef_err.note = note_err;

    ErrorList_append(el, redef_err);
}
