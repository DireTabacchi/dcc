#ifndef DYN_ARRAY_H
#define DYN_ARRAY_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define GEN_DYN_ARRAY_DECL(ArrayTypeName, ElemType) \
    typedef struct { \
        size_t len; \
        size_t cap; \
        ElemType *elems; \
    } ArrayTypeName; \
    \
    ArrayTypeName *ArrayTypeName##_create(void); \
    void ArrayTypeName##_destroy(ArrayTypeName *arr); \
    void ArrayTypeName##_init(ArrayTypeName *arr); \
    void ArrayTypeName##_deinit(ArrayTypeName *arr); \
    void ArrayTypeName##_append(ArrayTypeName *arr, ElemType elem); \
    void ArrayTypeName##_insert(ArrayTypeName *arr, ElemType elem, size_t idx);

#define GEN_DYN_ARRAY_IMPL(ArrayTypeName, ElemType) \
    ArrayTypeName *ArrayTypeName##_create(void) { \
        ArrayTypeName *arr = malloc(sizeof(ArrayTypeName)); \
        arr->len = 0; \
        arr->cap = 2; \
        arr->elems = calloc(arr->cap, sizeof(ElemType)); \
        return arr; \
    } \
    void ArrayTypeName##_destroy(ArrayTypeName *arr) { \
        if (arr == NULL) return; \
        free(arr->elems); \
        free(arr); \
    } \
    void ArrayTypeName##_init(ArrayTypeName *arr) { \
        if (arr == NULL) return;\
        \
        arr->len = 0; \
        arr->cap = 2; \
        arr->elems = calloc(arr->cap, sizeof(ElemType)); \
    } \
    \
    void ArrayTypeName##_deinit(ArrayTypeName *arr) { \
        if (arr == NULL) return; \
        \
        free(arr->elems); \
        arr->elems = NULL; \
        arr->cap = 0; \
        arr->len = 0; \
    } \
    void ArrayTypeName##_append(ArrayTypeName *arr, ElemType elem) { \
        if (arr == NULL) return; \
        if (arr->elems == NULL) return; \
        if (arr->len == arr->cap) { \
            size_t old_cap = arr->cap; \
            size_t new_cap = old_cap / 2 + old_cap; \
            ElemType *new_elems = calloc(new_cap, sizeof(ElemType)); \
            memcpy(new_elems, arr->elems, old_cap*sizeof(ElemType)); \
            free(arr->elems); \
            arr->elems = new_elems; \
            arr->cap = new_cap; \
        } \
        memcpy(&arr->elems[arr->len], &elem, sizeof(ElemType)); \
        arr->len += 1; \
    } \
    void ArrayTypeName##_insert(ArrayTypeName *arr, ElemType elem, size_t idx) { \
        if (arr == NULL) return; \
        if (arr->elems == NULL) return; \
        if (arr->len == arr->cap) { \
            size_t old_cap = arr->cap; \
            size_t new_cap = (old_cap == 0) ? 2 : old_cap / 2 + old_cap; \
            ElemType *new_elems = realloc(arr->elems, new_cap*sizeof(ElemType)); \
            if (new_elems == NULL) return; \
            arr->elems = new_elems; \
            arr->cap = new_cap; \
        } \
        if (idx < arr->len) { \
            memmove(arr->elems+idx+1, arr->elems+idx, (arr->len - idx) * sizeof(ElemType)); \
        } \
        arr->elems[idx] = elem; \
        arr->len += 1; \
    }
#endif // DYN_ARRAY_H
