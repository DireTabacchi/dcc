#ifndef DD_ERROR_H
#define DD_ERROR_H

#include "common.h"
#include "token.h"
#include "dd_string.h"

typedef struct error_ {
    String desc;    // Generated description of the error
    String file;    // Filename of error
    Position pos;   // Location of error
} Error;

typedef struct errorList_ {
    Error *errors;
    size_t len;
    size_t cap;
} ErrorList;

void ErrorList_init(ErrorList *el);
void ErrorList_destroy(ErrorList* el);
void ErrorList_append(ErrorList* el, Error err);
void ErrorList_print(ErrorList *el);

// Tokenizer errors

void err_stray_char(ErrorList *el, String srcname, Position pos, char stray);
void err_invalid_const_ident(ErrorList *el, String srcname, Position pos, String invalid_name);

// Parser errors

void err_expected_token(ErrorList *el, String srcname, TokenKind expected, Token actual);
void err_expected_expression(ErrorList *el, String srcname, Token actual);

// Semantics errors

void err_redeclared_variable(ErrorList *el, String srcname, Position pos, String varname);
void err_undeclared_variable(ErrorList *el, String srcname, Position pos, String varname);
void err_assign_invalid_lvalue(ErrorList *el, String srcname, Position pos);

#endif // DD_ERROR_H
