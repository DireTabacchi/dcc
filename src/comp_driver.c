#include <stdio.h>
#include <string.h>
#include "interner.h"
#include "token.h"
#include "sema.h"
#include "string_array.h"
#include "comp_driver.h"

#define VERSION_STRING "0.0.8"

void CompDriver_init(CompDriver *cd) {
    cd->opts = (Options){0};
    cd->str_table = (StrInterner){0};
    StrInterner_init(&cd->str_table);
    cd->tokenizer = (Tokenizer){0};
    cd->parser = (Parser){0};
    cd->cgd = (CodegenDriver){0};
    cd->uid_count = 0;
    ErrorList_init(&cd->errors);
    Sema_init(&cd->sema);
    StringArray_init(&cd->src_paths);
    StringArray_init(&cd->tu_names);

    for (size_t i = 0; i < TOKENKIND_LEN; i++) {
        if (i == TOKEN_OPERATORS_BEGIN || i == TOKEN_OPERATORS_END ||
            i == TOKEN_KEYWORDS_BEGIN || i == TOKEN_KEYWORDS_END) continue;
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
    Sema_deinit(&cd->sema);
    CodegenDriver_deinit(&cd->cgd);
    ErrorList_destroy(&cd->errors);
    StringArray_deinit(&cd->src_paths);
    StringArray_deinit(&cd->tu_names);
}

// Parse command arguments.
// On success, opts will hold options passed to the program, and 0 is returned.
int parse_command(CompDriver *cd, int argc, char *argv[]) {
    cd->opts = (Options){0};
    bool found_error = false;
    if (argc < 2) {
        //printf("Usage: %s [options] <srcfile.c>\n", argv[0]);
        found_error = true;
    }

    Options *opts = &cd->opts;

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
            } else if (strcmp("-c", argv[argi]) == 0 && opts->bf == BF_NONE) {
                opts->bf = BF_EMIT_OBJECT;
            } else if (strlen(argv[argi]) >= 3 && argv[argi][1] == '-') {
#ifdef DEBUG
                if (strcmp("--print-source", argv[argi]) == 0) {
                    opts->debug_flags[DF_PRINT_SRC] = true;
                } else if (strcmp("--print-tokenlist", argv[argi]) == 0) {
                    opts->debug_flags[DF_PRINT_TOKENS] = true;
                } else if (strcmp("--print-ast", argv[argi]) == 0) {
                    opts->debug_flags[DF_PRINT_AST] = true;
                } else if (strcmp("--print-tacd", argv[argi]) == 0) {
                    opts->debug_flags[DF_PRINT_TACD] = true;
                } else if (strcmp("--print-codegen", argv[argi]) == 0) {
                    opts->debug_flags[DF_PRINT_CODEGEN] = true;
                } else if (strcmp("--print-all", argv[argi]) == 0) {
                    opts->debug_flags[DF_PRINT_ALL] = true;
                } else
#endif
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
                } else {
                    goto bad_option;
                }
            } else {
bad_option:
                printf("Unknown option: %s\n", argv[argi]);
                found_error = true;
            }
        } else {
            size_t path_len = strlen(argv[argi]);
            String path_str = String_init_length(path_len);
            memcpy(path_str.cstr, argv[argi], path_str.len);
            StringArray_append(&cd->src_paths, path_str);
            //String_free(&path_str);
        }
    }

    if (cd->src_paths.len == 0 && !opts->display_usage_f) {
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

void print_usage(char *argv0) {
    printf("Dire C Compiler, DCC version %s\n", VERSION_STRING);
    printf("Usage: %s [options] <file.c>...\n", argv0);
    puts("Options:");
    puts("  -h, --help\t\tDisplay this information.");
    puts("  -E\t\t\tEmit Preprocessor code.\n  -S\t\t\tEmit Assembly code.");
    puts("  -c\t\t\tCompile and assemble. Do not link.");
    puts("  --lex\t\t\tRun compiler up through the Lexing stage. Does not produce output.");
    puts("  --parse\t\tRun compiler up through the Parsing stage. Does not produce output.");
    puts("  --validate\t\tRun compiler up through the semantic analysis stage. Does not produce output.");
    puts("  --tacky\t\tRun compiler up through the TACD gen stage. Does not produce output.");
    puts("  --codegen\t\tRun compiler up through the Codegen stage. Does not produce output.");
#ifdef DEBUG
    puts("Debug Options:");
    puts("  --print-source\tPrint the source file the compiler is processing.");
    puts("  --print-tokenlist\tPrint the token list after lexing.");
    puts("  --print-ast\t\tPrint the AST after parsing.");
    puts("  --print-tacd\t\tPrint the TACD code after generation.");
    puts("  --print-codegen\tPrint the generated ASM after codegen.");
    puts("  --print-all\t\tPrint the source file, token list, AST, TACD, and ASM during compilation.");
#endif
}

