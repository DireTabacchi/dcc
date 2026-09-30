#ifndef TYPING_H
#define TYPING_H

#include <stddef.h>

#include "dyn_array.h"

typedef enum {
    LINKAGE_NONE,
    LINKAGE_INTERNAL,
    LINKAGE_EXTERNAL
} LinkageKind;

typedef enum {
    DATATYPE_INVALID,
    DATATYPE_INT,
    DATATYPE_LONG
} DataType;

GEN_DYN_ARRAY_DECL(DataTypeArray, DataType)

typedef struct fnType_ {
    size_t arity;
    DataTypeArray *param_types; // Quantity of array == arity
    DataType ret_type;
} FnType;


#endif // TYPING_H
