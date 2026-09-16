#ifndef DD_ERROR_H
#define DD_ERROR_H

#include "common.h"
#include "token.h"
#include "dd_string.h"
#include "sym_table.h"

typedef enum errLevel_ {
    LVL_INFO,
    LVL_ERROR
} ErrorLevel;

typedef struct error_ Error;
struct error_ {
    ErrorLevel err_lvl; // Level of error (Info, Error)
    String desc;        // Generated description of the error
    String file;        // Filename of error
    Position pos;       // Location of error
    Error *note;        // Additional error info relevant to this error
};

typedef struct errorList_ {
    Error *errors;
    size_t len;
    size_t cap;
} ErrorList;

void ErrorList_init(ErrorList *el);
void ErrorList_destroy(ErrorList* el);
void ErrorList_append(ErrorList* el, Error err);
void ErrorList_print(ErrorList *el);

typedef struct compDriver_ CompDriver;

// Tokenizer errors

void err_stray_char(ErrorList *el, String srcname, Position pos, char stray);
void err_invalid_const_ident(ErrorList *el, String srcname, Position pos, String invalid_name);

// Parser errors

void err_expected_token(ErrorList *el, String srcname, TokenKind expected, Token actual);
void err_expected_expression(ErrorList *el, String srcname, Token actual);
void err_unclosed_param_list(ErrorList *el, String srcname, Position pos, const String *fn_name);
void err_expected_parameter(ErrorList *el, String srcname, Position pos);
void err_unexpected_token(ErrorList *el, String srcname, Token tok);

// Semantics errors

void err_redeclared_variable(ErrorList *el, String srcname, Position pos, String varname);
void err_undeclared_variable(ErrorList *el, String srcname, Position pos, String varname);
void err_assign_invalid_lvalue(ErrorList *el, String srcname, Position pos);

void err_decr_not_lvalue(CompDriver *cd, Position pos);
void err_incr_not_lvalue(CompDriver *cd, Position pos);

void err_duplicate_label(CompDriver *cd, Position pos, const SymEntry *lbl);
void err_undefined_label(CompDriver *cd, Position pos, const String *lbl);

void err_break_not_in_loop_switch(CompDriver *cd, Position pos);
void err_continue_not_in_loop(CompDriver *cd, Position pos);
void err_case_not_in_switch(CompDriver *cd, Position pos);
void err_default_not_in_switch(CompDriver *cd, Position pos);
void err_case_lbl_not_constant(CompDriver *cd, Position pos);
void err_duplicate_case(CompDriver *cd, Position pos, int val);
void err_duplicate_default(CompDriver *cd, Position pos);

void err_redefined_param(ErrorList *el, String srcname, Position pos, const String *param,
    const String *fn_name);
void err_undeclared_function(ErrorList *el, String srcname, Position pos, const String *fn_name);
void err_var_redeclared_as_fn(ErrorList *el, String srcname, Position pos, const String *fn_name);

// Type errors

void err_object_not_function(ErrorList *el, String srcname, Position pos, const String *name);
void err_too_few_args(ErrorList *el, String srcname, Position pos, const String *fn_name, size_t expected, size_t actual);
void err_too_many_args(ErrorList *el, String srcname, Position pos, const String *fn_name, size_t expected, size_t actual);
void err_object_not_variable(ErrorList *el, String srcname, Position pos, const String *obj_name);
void err_nested_fn_definition(ErrorList *el, String srcname, Position pos);
void err_assign_to_fn(ErrorList *el, String srcname, Position pos, const String *obj_name);
void err_assign_fn_to_var(ErrorList *el, String srcname, Position pos, const String *obj_name);
void err_incompatible_fn_types(ErrorList *el, String srcname, Position pos, const String *fn_name);
void err_fn_redefinition(ErrorList *el, String srcname, Position pos, const String *fn_name,
    Position prev_pos);

#endif // DD_ERROR_H
