#ifndef PARAM_ARRAY_H
#define PARAM_ARRAY_H

#include <stddef.h>

#include "dd_string.h"

typedef struct paramArray_ {
    const String **params;
    size_t len;
    size_t cap;
} ParamArray;

ParamArray *ParamArray_create(void);
void ParamArray_destroy(ParamArray *pa);
void ParamArray_append(ParamArray *pa, const String *param);


#endif // PARAM_ARRAY_H
