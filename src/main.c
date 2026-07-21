#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#include <sys/stat.h>

#include "tokenizer.h"
#include "parser.h"
#include "codegen.h"
#include "codegen_x86_64.h"
#include "tacd.h"
#include "comp_driver.h"
#include "sem_analysis.h"

#include "dcc_error.h"

// TODO: Write a compiler driver module to handle allocating and freeing memory,
//       cleaning up temporary files

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
    puts("  --parse\t\tRun compiler up through the Parsing stage. Does not produce output.");
    puts("  --validate\t\tRun compiler up through the semantic analysis stage. Does not produce output.");
    puts("  --tacky\t\tRun compiler up through the TACD gen stage. Does not produce output.");
    puts("  --codegen\t\tRun compiler up through the Codegen stage. Does not produce output.");
}

int main(int argc, char *argv[]) {
    CompDriver driver = {0};
    CompDriver_init(&driver);
    if (parse_command(&driver.opts, argc, argv) != 0) {
        print_usage(argv[0]);
        CompDriver_deinit(&driver);
        return EXIT_FAILURE;
    }

    if (driver.opts.display_usage_f) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    struct stat src_stat;
    if (stat(driver.opts.filepath, &src_stat) != 0) {
        printf("[Error] Cannot stat file %s\n", driver.opts.filepath);
        return EXIT_FAILURE;
    }
    // Size of path string (not including null terminator byte)
    size_t path_len = strlen(driver.opts.filepath);
    size_t basename_len = 0;
    // find the suffix (.c) to replace with the suffix (.i)
    if (driver.opts.filepath[path_len-1] != 'c' && driver.opts.filepath[path_len-2] != '.') {
        puts("[Error] Expected source file ending in `.c`");
        return EXIT_FAILURE;
    }
    for (size_t i = path_len-1; i >= 0; i--) {
        if (driver.opts.filepath[i] == '.') {
            basename_len = i;
            break;
        }
    }
    if (basename_len == 0) {
        puts("[Error] Invalid filename");
        return EXIT_FAILURE;
    }

    String file_basename = String_init_length(basename_len);
    memcpy(file_basename.cstr, driver.opts.filepath, basename_len);
    //printf("[debug] file basename is %s\n", file_basename);
    String preproc_filename = String_init_length(basename_len+2);
    memcpy(preproc_filename.cstr, file_basename.cstr, basename_len);
    memcpy(&preproc_filename.cstr[basename_len], ".i", 2);  // kinda sketchy, maybe find better way later
    //printf("[debug] preproc filename: %s\n", preproc_filename);

    // Preprocess file; Let GCC handle that
    // allocate space for command: 14 characters (exe, flags, spaces) + strlen(src) + strlen(preproc_filename)
    String preproc_command = String_init_length(14 + strlen(driver.opts.filepath) + preproc_filename.len);
    sprintf(preproc_command.cstr, "gcc -E -P %s -o %s", driver.opts.filepath, preproc_filename.cstr);
    //printf("[debug] Preproc_command:\n%s\n", preproc_command);
    system(preproc_command.cstr);
    String_free(&preproc_command); // preproc command no longer needed

    Tokenizer_init(&driver.tokenizer, preproc_filename.cstr);
    Parser_init(&driver.parser);
    bool compiler_erred = false;
    if (driver.opts.dbf >= DBF_LEX || driver.opts.dbf == DBF_NONE) {
        tokenize(&driver);
#ifdef DEBUG
        TokenList_print(&driver.tokenizer.tokens);
#endif
    }

    if (driver.opts.dbf >= DBF_PARSE || driver.opts.dbf == DBF_NONE) {
        // TODO: take in the whole driver
        parse(&driver);
#ifdef DEBUG
        puts("Generated AST Structure\n=======================");
        Parser_print_ast(&driver.parser);
#endif
    }

    if (driver.opts.dbf >= DBF_VALIDATE || driver.opts.dbf == DBF_NONE) {
        sem_analyze(&driver);
#ifdef DEBUG
        puts("Validated AST Structure\n=======================");
        Parser_print_ast(&driver.parser);
#endif
    }

    if (driver.errors.len > 0) {
        compiler_erred = true;
        ErrorList_print(&driver.errors);
    }

    CodegenDriver_init(&driver.cgd);
    if (!compiler_erred && (driver.opts.dbf >= DBF_TACD || driver.opts.dbf == DBF_NONE)) {
        generate_tacd(&driver, driver.parser.program);
#ifdef DEBUG
        Tacd_print(driver.cgd.tacd_gen.program, 0);
#endif
    }

    if (!compiler_erred && (driver.opts.dbf >= DBF_CODEGEN || driver.opts.dbf == DBF_NONE)) {
        // TODO: pass whole driver
        emit_asm(&driver.cgd, driver.cgd.tacd_gen.program);
    }

    driver.cgd.dest = String_init_length(basename_len+2);
    memcpy(driver.cgd.dest.cstr, file_basename.cstr, basename_len);
    memcpy(&driver.cgd.dest.cstr[basename_len], ".s", 2);

    if (!compiler_erred && (driver.opts.dbf >= DBF_CODEGEN || driver.opts.dbf == DBF_NONE)) {
        emit_program(&driver.cgd);
    }

    if (driver.opts.bf != BF_EMIT_PREPROCESSOR) {
        //puts("removing preprocessor file");
        remove(preproc_filename.cstr);
    }

    if (!compiler_erred && driver.opts.dbf < DBF_LEX && driver.opts.bf < BF_EMIT_PREPROCESSOR) {
        // allocate space for command: 8 characters ((3)exe, (2)flags, (3)spaces) + strlen(src) + strlen(preproc_filename)
        String assemble_command = String_init_length(8 + driver.cgd.dest.len + file_basename.len);
        sprintf(assemble_command.cstr, "gcc %s -o %s", driver.cgd.dest.cstr, file_basename.cstr);
        //printf("[debug] assemble_command:\n%s\n", assemble_command);
        system(assemble_command.cstr);
        String_free(&assemble_command); // assemble command no longer needed
    }

    if (driver.opts.bf != BF_EMIT_ASSEMBLY) {
        remove(driver.cgd.dest.cstr);
    }

    printf("Final Interned Strings [%ld/%ld] (load/cap):\n", driver.str_table.load, driver.str_table.cap);
    for (size_t is_idx = 0; is_idx < driver.str_table.cap; is_idx++) {
        if (driver.str_table.strs[is_idx].status == ISS_OCCUPIED) {
            printf("\t[%4ld] `%s`\n", is_idx, driver.str_table.strs[is_idx].str->cstr);
        }
    }

    CompDriver_deinit(&driver);

    String_free(&file_basename);
    String_free(&preproc_filename);

    if (compiler_erred) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
