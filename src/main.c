#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#include <sys/stat.h>

#include "token.h"
#include "tokenizer.h"
#include "parser.h"
#include "codegen.h"
#include "codegen_x86_64.h"

#include "dcc_error.h"

// TODO: Write a compiler driver module to handle allocating and freeing memory,
//       cleaning up temporary files

typedef enum buildflag_ {
    BF_NONE,
    BF_EMIT_PREPROCESSOR,
    BF_EMIT_ASSEMBLY,
} BuildFlag;

typedef enum drybuildflag_ {
    DBF_NONE,
    DBF_LEX,
    DBF_PARSE,
    DBF_CODEGEN
} DryBuildFlag;

typedef struct options_ {
    BuildFlag bf;
    DryBuildFlag dbf;
    bool display_usage_f;
    char *filepath;
} Options;

void printOptions(Options *opts) {
    if (opts->bf == BF_EMIT_PREPROCESSOR) {
        puts("Build Flag: -E -- Emit preprocessor");
    } else if (opts->bf == BF_EMIT_ASSEMBLY) {
        puts("Build Flag: -S -- Emit assembly");
    }

    if (opts->dbf == DBF_LEX) {
        puts("Dry Build Flag: --lex -- Lexing stage");
    } else if (opts->dbf == DBF_PARSE) {
        puts("Dry Build Flag: --parse -- Parsing stage");
    } else if (opts->dbf == DBF_CODEGEN) {
        puts("Dry Build Flag: --codegen -- Code generation stage");
    }

    if (opts->bf == BF_NONE && opts->dbf == DBF_NONE) {
        puts("No build flags; emit executable");
    }

    printf("Source file: %s\n", opts->filepath);
}

void print_usage(char *argv0) {
    printf("Usage: %s [options] <srcfile.c>\n", argv0);
    puts("Options:");
    puts("  -h, --help\t\tDisplay this information.");
    puts("  -E\t\t\tEmit Preprocessor code.\n  -S\t\t\tEmit Assembly code.");
    puts("  --lex\t\t\tRun compiler up through the Lexing stage. Does not produce output.");
    puts("  --parse\t\tRun compiler up through the Parsinging stage. Does not produce output.");
    puts("  --codegen\t\tRun compiler up through the Codegen stage. Does not produce output.");
}

// Parse command arguments.
// On success, opts will hold options passed to the program, and 0 is returned.
int parseCommand(Options *opts, int argc, char *argv[]) {
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

int main(int argc, char *argv[]) {
    Options opts = {0};
    if (parseCommand(&opts, argc, argv) != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (opts.display_usage_f) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    struct stat src_stat;
    if (stat(opts.filepath, &src_stat) != 0) {
        printf("[Error] Cannot stat file %s\n", opts.filepath);
        return EXIT_FAILURE;
    }

    size_t path_len = strlen(opts.filepath); // Size of path string (not including null terminator byte)
    size_t basename_len = 0;
    // find the suffix (.c) to replace with the suffix (.i)
    if (opts.filepath[path_len-1] != 'c' && opts.filepath[path_len-2] != '.') {
        puts("[Error] Expected source file ending in `.c`");
        return EXIT_FAILURE;
    }
    for (size_t i = path_len-1; i >= 0; i--) {
        if (opts.filepath[i] == '.') {
            basename_len = i;
            break;
        }
    }
    if (basename_len == 0) {
        puts("[Error] Invalid filename");
        return EXIT_FAILURE;
    }

    String file_basename = String_init_length(basename_len);
    memcpy(file_basename.cstr, opts.filepath, basename_len);
    //printf("[debug] file basename is %s\n", file_basename);
    String preproc_filename = String_init_length(basename_len+2);
    memcpy(preproc_filename.cstr, file_basename.cstr, basename_len);
    memcpy(&preproc_filename.cstr[basename_len], ".i", 2);  // kinda sketchy, maybe find better way later
    //printf("[debug] preproc filename: %s\n", preproc_filename);

    // Preprocess file; Let GCC handle that
    // allocate space for command: 14 characters (exe, flags, spaces) + strlen(src) + strlen(preproc_filename)
    String preproc_command = String_init_length(14 + strlen(opts.filepath) + preproc_filename.len);
    sprintf(preproc_command.cstr, "gcc -E -P %s -o %s", opts.filepath, preproc_filename.cstr);
    //printf("[debug] Preproc_command:\n%s\n", preproc_command);
    system(preproc_command.cstr);
    String_free(&preproc_command); // preproc command no longer needed

    Parser driver = {0};
    Parser_init(&driver, preproc_filename.cstr);
    bool compiler_erred = false;
    if (opts.dbf >= DBF_LEX || opts.dbf == DBF_NONE) {
        tokenize(&driver.tokenizer);
        TokenList_print(&driver.tokenizer.tokens);
    }

    if (opts.dbf >= DBF_PARSE || opts.dbf == DBF_NONE) {
        parse(&driver);
        Parser_print_ast(&driver);
    }

    if (driver.tokenizer.errors.len > 0) {
        compiler_erred = true;
        ErrorList_print(&driver.tokenizer.errors);
    }

    if (driver.errors.len > 0) {
        compiler_erred = true;
        ErrorList_print(&driver.errors);
    }

    CodegenDriver cgd = {0};
    if (!compiler_erred && (opts.dbf >= DBF_CODEGEN || opts.dbf == DBF_NONE)) {
        // TODO: creates a whole compiler-driver structure and clean up main
        trx_ast_asm(&cgd, driver.program);
        CodegenDriver_print_gen_asm(&cgd);
    }

    cgd.dest = String_init_length(basename_len+2);
    memcpy(cgd.dest.cstr, file_basename.cstr, basename_len);
    memcpy(&cgd.dest.cstr[basename_len], ".s", 2);

    if (!compiler_erred && (opts.dbf >= DBF_CODEGEN || opts.dbf == DBF_NONE)) {
        emit_program(&cgd);
    }

    if (opts.bf != BF_EMIT_PREPROCESSOR) {
        //puts("removing preprocessor file");
        remove(preproc_filename.cstr);
    }

    if (opts.dbf < DBF_LEX) {
        // allocate space for command: 8 characters ((3)exe, (2)flags, (3)spaces) + strlen(src) + strlen(preproc_filename)
        String assemble_command = String_init_length(8 + cgd.dest.len + file_basename.len);
        sprintf(assemble_command.cstr, "gcc %s -o %s", cgd.dest.cstr, file_basename.cstr);
        //printf("[debug] assemble_command:\n%s\n", assemble_command);
        system(assemble_command.cstr);
        String_free(&assemble_command); // assemble command no longer needed
    }

    if (opts.bf != BF_EMIT_ASSEMBLY) {
        remove(cgd.dest.cstr);
    }

    AsmNode_destroy(cgd.program);
    Parser_destroy(&driver);

    String_free(&file_basename);
    String_free(&preproc_filename);
    String_free(&cgd.dest);

    if (compiler_erred) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
