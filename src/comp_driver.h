#ifndef DRIVER_H
#define DRIVER_H

#include "interner.h"
#include "tokenizer.h"
#include "parser.h"
#include "sem_analysis.h"
#include "codegen.h"
#include "dcc_error.h"

typedef enum buildflag_ {
    BF_NONE,
    BF_EMIT_PREPROCESSOR,
    BF_EMIT_ASSEMBLY,
} BuildFlag;

typedef enum drybuildflag_ {
    DBF_NONE,
    DBF_LEX,
    DBF_PARSE,
    DBF_VALIDATE,
    DBF_TACD,
    DBF_CODEGEN
} DryBuildFlag;

typedef struct options_ {
    BuildFlag bf;
    DryBuildFlag dbf;
    bool display_usage_f;
    char *filepath;
} Options;

typedef struct compDriver_ {
    Options opts;
    StrInterner str_table;
    Tokenizer tokenizer;
    Parser parser;
    Sema sema;
    CodegenDriver cgd;

    size_t uid_count;
    ErrorList errors;
} CompDriver;

void CompDriver_init(CompDriver *cd);
void CompDriver_deinit(CompDriver *cd);

int parse_command(Options *opts, int argc, char *argv[]);

#endif // DRIVER_H
