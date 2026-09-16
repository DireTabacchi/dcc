#include <stdlib.h>
#include <string.h>

#include "param_array.h"

ParamArray *ParamArray_create(void) {
    ParamArray *pa = malloc(sizeof(ParamArray));
    pa->len = 0;
    pa->cap = 2;
    pa->params = calloc(pa->cap, sizeof(const String *));

    return pa;
}

void ParamArray_destroy(ParamArray *pa) {
    if (pa == NULL) return;
    if (pa->params == NULL) return;
    free(pa->params);
    free(pa);
}

void ParamArray_append(ParamArray *pa, const String *param) {
    if (pa == NULL) return;
    if (pa->params == NULL) return;

    if (pa->len == pa->cap) {
        size_t old_cap = pa->cap;
        size_t new_cap = old_cap / 2 + old_cap;
        const String **new_params = calloc(new_cap, sizeof(const String *));

        memcpy(new_params, pa->params, old_cap*sizeof(const String *));
        free(pa->params);
        pa->params = new_params;
        pa->cap = new_cap;
    }

    pa->params[pa->len] = param;
    pa->len += 1;
}

