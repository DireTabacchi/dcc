#include <stdio.h>
#include <string.h>
#include "interner.h"
#include "token.h"
#include "comp_driver.h"

void CompDriver_init(CompDriver *cd) {
    cd->opts = (Options){0};
    cd->str_table = (StrInterner){0};
    StrInterner_init(&cd->str_table);
    cd->tokenizer = (Tokenizer){0};
    cd->parser = (Parser){0};
    cd->cgd = (CodegenDriver){0};
    ErrorList_init(&cd->errors);

    for (size_t i = 0; i < TOKENKIND_LEN; i++) {
        if (i == TOKEN_OPERATORS_BEGIN || i == TOKEN_OPERATORS_END || i == TOKEN_KEYWORDS_BEGIN || i == TOKEN_KEYWORDS_END) continue;
        StrInterner_intern(&cd->str_table, token_literals[i]);
    }

    //printf("Init Interned Strings [%ld/%ld] (load/cap):\n", cd->str_table.load, cd->str_table.cap);
    //for (size_t is_idx = 0; is_idx < cd->str_table.cap; is_idx++) {
    //    if (cd->str_table.strs[is_idx].status == ISS_OCCUPIED) {
    //        printf("\t[%2ld]`%s`\n", is_idx, cd->str_table.strs[is_idx].str->cstr);
    //    }
    //}
}

void CompDriver_deinit(CompDriver *cd) {
    StrInterner_deinit(&cd->str_table);
    Tokenizer_destroy(&cd->tokenizer);
    Parser_destroy(&cd->parser);
    CodegenDriver_deinit(&cd->cgd);
    ErrorList_destroy(&cd->errors);
}

// Parse command arguments.
// On success, opts will hold options passed to the program, and 0 is returned.
int parse_command(Options *opts, int argc, char *argv[]) {
    opts->bf = BF_NONE;
    opts->dbf = DBF_NONE;
    opts->filepath = NULL;
    opts->display_usage_f = false;
    bool found_error = false;
    if (argc < 2) {
        //printf("Usage: %s [options] <srcfile.c>\n", argv[0]);
        found_error = true;
    }

    for (int argi = 1; argi <= argc; argi++) {
        if (argv[argi] == NULL) {
            //puts("End of arguments.");
            break;
        }

        // TODO: this needs simplification
        if (strlen(argv[argi]) >= 2 && argv[argi][0] == '-') {
            // for non-dry-build flags, use the first that was found
            if (strcmp("-E", argv[argi]) == 0 && opts->bf == BF_NONE) {
                //puts("Found -E: output preprocessor.");
                opts->bf = BF_EMIT_PREPROCESSOR;
            } else if (strcmp("-S", argv[argi]) == 0 && opts->bf == BF_NONE) {
                //puts("Found -S: output assembly.");
                opts->bf = BF_EMIT_ASSEMBLY;
            } else if (strcmp("-h", argv[argi]) == 0) {
                opts->display_usage_f = true;
            } else if (strlen(argv[argi]) >= 3 && argv[argi][1] == '-') {
                if (strcmp("--lex", argv[argi]) == 0) {
                    if (opts->dbf == DBF_NONE) {
                        //puts("Found --lex: run lexer, do not parse, produce no output.");
                        opts->dbf = DBF_LEX;
                    } else {
                        puts("Found --lex, but a dry build was already selected.");
                        found_error = true;
                    }
                } else if (strcmp("--parse", argv[argi]) == 0) {
                    if (opts->dbf == DBF_NONE) {
                        //puts("Found --parse: run lexer and parser, do not generate assembly, produce no output.");
                        opts->dbf = DBF_PARSE;
                    } else {
                        puts("Found --parse, but a dry build was already selected.");
                        found_error = true;
                    }
                } else if (strcmp("--validate", argv[argi]) == 0) {
                    if (opts->dbf == DBF_NONE) {
                        opts->dbf = DBF_VALIDATE;
                    } else {
                        puts("Found --validate, but a dry build was already selected.");
                        found_error = true;
                    }
                } else if (strcmp("--tacky", argv[argi]) == 0) {
                    if (opts->dbf == DBF_NONE) {
                        opts->dbf = DBF_TACD;
                    } else {
                        puts("Found --tacky, but a dry build was already selected.");
                        found_error = true;
                    }
                } else if (strcmp("--codegen", argv[argi]) == 0) {
                    if (opts->dbf == DBF_NONE) {
                        //puts("Found --codegen: run lexer, parser, and generate assembly, produce no output.");
                        opts->dbf = DBF_CODEGEN;
                    } else {
                        puts("Found --codegen, but a dry build was already selected.");
                        found_error = true;
                    }
                } else if (strcmp("--help", argv[argi]) == 0) {
                    opts->display_usage_f = true;
                }
            } else {
                printf("Unknown option: %s\n", argv[argi]);
                found_error = true;
            }
        } else {
            opts->filepath = argv[argi];
        }
    }

    if (opts->filepath == NULL && !opts->display_usage_f) {
        puts("[Error] no input file.");
        //puts("Usage: %s [options] <srcfile.c>");
        found_error = true;
    }

    if (opts->bf != BF_NONE && opts->dbf != DBF_NONE && !opts->display_usage_f) {
        puts("[Error] Output flags are incompatible with dry build flags.");
        found_error = true;
    }

    if (found_error) {
        return 1;
    }

    return 0;
}

