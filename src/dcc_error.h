#ifndef DCC_ERROR_H
#define DCC_ERROR_H

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

void err_stray_char(ErrorList *el, String srcname, Position pos, char stray); // E001
void err_invalid_const_ident(ErrorList *el, String srcname, Position pos, String invalid_name); // E002

// Parser errors

void err_expected_token(ErrorList *el, String srcname, TokenKind expected, Token actual); // E003
void err_expected_expression(ErrorList *el, String srcname, Token actual); // E004
void err_unclosed_param_list(ErrorList *el, String srcname, Position pos, const String *fn_name); // E005
void err_expected_parameter(ErrorList *el, String srcname, Position pos); // E006
void err_unexpected_token(ErrorList *el, String srcname, Token tok); // E007
void err_invalid_type_specifier(ErrorList *el, String srcname, Position pos); // E034
void err_missing_type_specifier(ErrorList *el, String srcname, Position pos); // E035
void err_storage_class_on_param(ErrorList *el, String srcname, Position pos); // E036
void err_invalid_storage_class(ErrorList *el, String srcname, Position pos); // E037

// Semantics errors

void err_redeclared_variable(ErrorList *el, String srcname, Position pos, String varname); // E008 (retired)
void err_undeclared_variable(ErrorList *el, String srcname, Position pos, String varname); // E009
void err_assign_invalid_lvalue(ErrorList *el, String srcname, Position pos); // E010

void err_decr_not_lvalue(CompDriver *cd, Position pos); // E011
void err_incr_not_lvalue(CompDriver *cd, Position pos); // E012

void err_duplicate_label(CompDriver *cd, Position pos, const SymEntry *lbl); // E013
void err_undefined_label(CompDriver *cd, Position pos, const String *lbl); // E014

void err_break_not_in_loop_switch(CompDriver *cd, Position pos); // E015
void err_continue_not_in_loop(CompDriver *cd, Position pos); // E016
void err_case_not_in_switch(CompDriver *cd, Position pos); // E017
void err_default_not_in_switch(CompDriver *cd, Position pos); // E018
void err_case_lbl_not_constant(CompDriver *cd, Position pos); // E019
void err_duplicate_case(CompDriver *cd, Position pos, int val); // E020
void err_duplicate_default(CompDriver *cd, Position pos); // E021

void err_redefined_param(ErrorList *el, String srcname, Position pos, const String *param,
    const String *fn_name); // E022
void err_undeclared_function(ErrorList *el, String srcname, Position pos, const String *fn_name); // E023
void err_var_redeclared_as_fn(ErrorList *el, String srcname, Position pos, const String *fn_name); // E024
void err_conflicting_var_decl(ErrorList *el, String srcname, Position pos, const String *var_name,
    Position prev_pos); // E038
void err_block_static_fn_decl(ErrorList *el, String srcname, Position pos, const String *fn_name); // E039

// Type errors

void err_object_not_function(ErrorList *el, String srcname, Position pos, const String *name,
    Position prev_pos); // E025
void err_too_few_args(ErrorList *el, String srcname, Position pos, const String *fn_name, size_t expected, size_t actual); // E026
void err_too_many_args(ErrorList *el, String srcname, Position pos, const String *fn_name, size_t expected, size_t actual); // E027
void err_object_not_variable(ErrorList *el, String srcname, Position pos, const String *obj_name); // E028
void err_nested_fn_definition(ErrorList *el, String srcname, Position pos); // E029
void err_assign_to_fn(ErrorList *el, String srcname, Position pos, const String *obj_name); // E030
void err_assign_fn_to_var(ErrorList *el, String srcname, Position pos, const String *obj_name); // E031
void err_incompatible_fn_types(ErrorList *el, String srcname, Position pos, const String *fn_name,
    Position prev_pos); // E032
void err_fn_redefinition(ErrorList *el, String srcname, Position pos, const String *fn_name,
    Position prev_pos); // E033
void err_static_fn_non_static(ErrorList *el, String srcname, Position pos, const String *fn_name,
    Position prev_pos); // E040
void err_conflicting_file_defs(ErrorList *el, String srcname, Position pos, const String *def_name,
    Position prev_pos); // E041
void err_conflicting_var_linkage(ErrorList *el, String srcname, Position pos,
    const String *def_name, Position prev_pos); // E042
void err_extern_var_initializer(ErrorList *el, String srcname, Position pos, const String *var_name); // E043
void err_file_var_non_const_init(ErrorList *el, String srcname, Position pos,
    const String *var_name); // E044
void err_lcl_var_stc_non_const_init(ErrorList *el, String srcname, Position pos,
    const String *var_name); // E045
void err_fn_redeclared_as_var(ErrorList *el, String srcname, Position pos, const String *var_name,
    Position prev_pos); // E046
void err_storage_in_for_init(ErrorList *el, String srcname, Position pos, const String *var_name); // E047

#endif // DCC_ERROR_H
