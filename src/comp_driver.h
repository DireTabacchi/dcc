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
typedef enum devDebugFlag_ {
    DDF_PRINT_SRC,
    DDF_PRINT_TOKENS,
    DDF_PRINT_AST,
    DDF_PRINT_TACD,
    DDF_PRINT_CODEGEN,
    DDF_PRINT_ALL,
    DDF_FLAGS_LEN
} DevDebugFlag;
#endif

typedef enum debugFlag_ {
    DF_COMMENT_ASM,
    DF_FLAGS_LEN
} DebugFlag;

typedef struct options_ {
    BuildFlag bf;
    DryBuildFlag dbf;
#ifdef DEBUG
    bool dev_debug_flags[DDF_FLAGS_LEN];
#endif
    bool debug_flags[DF_FLAGS_LEN];
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
