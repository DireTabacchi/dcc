#ifndef DD_ERROR_H
#define DD_ERROR_H

#include "token.h"
#include "dd_string.h"

typedef struct error_ {
    String desc;    // Generated description of the error
    String file;    // Filename of error
    TokenPos pos;   // Location of error
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

#endif // DD_ERROR_H
