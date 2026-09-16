#ifndef DRIVER_H
#define DRIVER_H

#include "interner.h"
#include "string_array.h"
#include "tokenizer.h"
#include "parser.h"
#include "sema.h"
#include "codegen.h"
#include "dcc_error.h"

typedef enum buildflag_ {
    BF_NONE,
    BF_EMIT_PREPROCESSOR,
    BF_EMIT_ASSEMBLY,
    BF_EMIT_OBJECT
} BuildFlag;

typedef enum drybuildflag_ {
    DBF_NONE,
    DBF_LEX,
    DBF_PARSE,
    DBF_VALIDATE,
    DBF_TACD,
    DBF_CODEGEN
} DryBuildFlag;

#ifdef DEBUG
typedef enum debugFlag_ {
    DF_PRINT_SRC,
    DF_PRINT_TOKENS,
    DF_PRINT_AST,
    DF_PRINT_TACD,
    DF_PRINT_CODEGEN,
    DF_PRINT_ALL,
    DF_FLAGS_LEN
} DebugFlag;
#endif

typedef struct options_ {
    BuildFlag bf;
    DryBuildFlag dbf;
#ifdef DEBUG
    bool debug_flags[DF_FLAGS_LEN];
#endif
    bool display_usage_f;
} Options;

typedef struct compDriver_ {
    Options opts;
    StrInterner str_table;
    Tokenizer tokenizer;
    Parser parser;
    Sema sema;
    CodegenDriver cgd;

    StringArray src_paths;
    StringArray tu_names;   // Translation-Unit names (ext-stripped paths)

    size_t uid_count;
    ErrorList errors;
} CompDriver;

void CompDriver_init(CompDriver *cd);
void CompDriver_deinit(CompDriver *cd);

int parse_command(CompDriver *cd, int argc, char *argv[]);
void print_usage(char *argv0);

#endif // DRIVER_H
