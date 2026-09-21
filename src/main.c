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
#include "sema.h"

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

    //printf("Source file: %s\n", opts->filepath);
}

int main(int argc, char *argv[]) {
    CompDriver driver = {0};
    CompDriver_init(&driver);
    if (parse_command(&driver, argc, argv) != 0) {
        print_usage(argv[0]);
        CompDriver_deinit(&driver);
        return EXIT_FAILURE;
    }

    if (driver.opts.display_usage_f) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    bool compiler_erred = false;
    // Create an object for each source file
    for (size_t src_idx = 0; src_idx < driver.src_paths.len; src_idx++) {
        String src_path = driver.src_paths.strs[src_idx];
        struct stat src_stat;
        if (stat(src_path.cstr, &src_stat) != 0) {
            printf("[Error] Cannot stat file %s\n", src_path.cstr);
            return EXIT_FAILURE;
        }

        // Size of path string (not including null terminator byte)
        size_t basename_len = 0;
        // find the suffix (.c) to replace with the suffix (.i)
        if (src_path.cstr[src_path.len-1] != 'c' && src_path.cstr[src_path.len-2] != '.') {
            puts("[Error] Expected source file ending in `.c`");
            return EXIT_FAILURE;
        }
        for (size_t i = src_path.len-1; i >= 0; i--) {
            if (src_path.cstr[i] == '.') {
                basename_len = i;
                break;
            }
        }
        if (basename_len == 0) {
            puts("[Error] Invalid filename");
            return EXIT_FAILURE;
        }

        String tu_name = String_init_length(basename_len);
        memcpy(tu_name.cstr, src_path.cstr, basename_len);
        StringArray_append(&driver.tu_names, tu_name);
        //printf("[debug] file basename is %s\n", file_basename);
        String preproc_filename = String_init_length(basename_len+2);
        memcpy(preproc_filename.cstr, tu_name.cstr, basename_len);
        memcpy(&preproc_filename.cstr[basename_len], ".i", 2);  // kinda sketchy, maybe find better way later
        // Preprocess file; Let GCC handle that
        // allocate space for command:
        //   14 characters (exe, flags, spaces) + src_path.len + strlen(preproc_filename)
        String preproc_command = String_init_length(14 + src_path.len + preproc_filename.len);
        sprintf(preproc_command.cstr, "gcc -E -P %s -o %s", src_path.cstr, preproc_filename.cstr);
        //printf("[debug] Preproc_command:\n%s\n", preproc_command);
        system(preproc_command.cstr);
        String_free(&preproc_command); // preproc command no longer needed

        Tokenizer_init(&driver, preproc_filename.cstr);
        Parser_init(&driver.parser);
        if (driver.opts.dbf >= DBF_LEX || driver.opts.dbf == DBF_NONE) {
            tokenize(&driver);
#ifdef DEBUG
            if (driver.opts.dev_debug_flags[DDF_PRINT_TOKENS] ||
                driver.opts.dev_debug_flags[DDF_PRINT_ALL]
            ) {
                TokenList_print(&driver.tokenizer.tokens);
            }
#endif
        }

        if (driver.opts.dbf >= DBF_PARSE || driver.opts.dbf == DBF_NONE) {
            // TODO: take in the whole driver
            parse(&driver);
#ifdef DEBUG
            if (driver.opts.dev_debug_flags[DDF_PRINT_AST] ||
                driver.opts.dev_debug_flags[DDF_PRINT_ALL]
            ) {
                puts("Generated AST Structure\n=======================");
                Parser_print_ast(&driver.parser);
            }
#endif
        }

        if (driver.opts.dbf >= DBF_VALIDATE || driver.opts.dbf == DBF_NONE) {
            sem_analyze(&driver);
#ifdef DEBUG
            if (driver.opts.dev_debug_flags[DDF_PRINT_AST] ||
                driver.opts.dev_debug_flags[DDF_PRINT_ALL]) {
                puts("Validated AST Structure\n=======================");
                Parser_print_ast(&driver.parser);
            }
#endif
        }

        if (driver.errors.len > 0) {
            compiler_erred = true;
            ErrorList_print(&driver.errors);
        }

        CodegenDriver_init(&driver.cgd);
        if (!compiler_erred && (driver.opts.dbf >= DBF_TACD || driver.opts.dbf == DBF_NONE)) {
            generate_tacd(&driver, driver.parser.ast_tu);
#ifdef DEBUG
            if (driver.opts.dev_debug_flags[DDF_PRINT_TACD] ||
                driver.opts.dev_debug_flags[DDF_PRINT_ALL]) {
                Tacd_print(driver.cgd.tacd_gen.tacd_tu);
            }
#endif
        }

        if (!compiler_erred && (driver.opts.dbf >= DBF_CODEGEN || driver.opts.dbf == DBF_NONE)) {
            // TODO: pass whole driver
            emit_asm(&driver, driver.cgd.tacd_gen.tacd_tu);
        }
        driver.cgd.dest = String_init_length(basename_len+2);
        memcpy(driver.cgd.dest.cstr, tu_name.cstr, basename_len);
        memcpy(&driver.cgd.dest.cstr[basename_len], ".s", 2);

        if (!compiler_erred && (driver.opts.dbf >= DBF_CODEGEN || driver.opts.dbf == DBF_NONE)) {
            emit_program(&driver);
        }

        if (driver.opts.bf != BF_EMIT_PREPROCESSOR) {
            //puts("removing preprocessor file");
            remove(preproc_filename.cstr);
        }

        if (!compiler_erred &&
            driver.opts.dbf == DBF_NONE && 
            (driver.opts.bf >= BF_EMIT_ASSEMBLY || driver.opts.bf == BF_NONE)
        ) {
            // allocate space for command:
            //     11 characters ((3)exe, (4)flags, (4)spaces, (2)file extension) +
            //     strlen(src) + strlen(preproc_filename)
            String assemble_command = String_init_length(13 + driver.cgd.dest.len + tu_name.len);
            snprintf(assemble_command.cstr, assemble_command.len+1,
                "gcc -c -o %s.o %s", tu_name.cstr, driver.cgd.dest.cstr);
            printf("[debug] assemble_command:\n%s\n", assemble_command.cstr);
            int obj_res = system(assemble_command.cstr);
            if (obj_res != 0) {
                puts("[DEBUG:ERROR] Error in building object.");
            }
            String_free(&assemble_command); // assemble command no longer needed
        }

        if (driver.opts.bf != BF_EMIT_ASSEMBLY) {
            remove(driver.cgd.dest.cstr);
        }

        String_free(&preproc_filename);
        //String_free(&tu_name);
    }

    if (!compiler_erred && driver.opts.bf != BF_EMIT_OBJECT && driver.opts.dbf < DBF_LEX) {
        // link all objects into executable
        printf("linking objects\n");
        size_t command_len = driver.tu_names.strs[0].len+7;
        size_t next_offset = command_len;
        for (size_t tu_idx = 0; tu_idx < driver.tu_names.len; tu_idx++) {
            command_len += driver.tu_names.strs[tu_idx].len + 3;
        }
        String link_command = String_init_length(command_len);
        snprintf(link_command.cstr, driver.tu_names.strs[0].len+8,
            "gcc -o %s", driver.tu_names.strs[0].cstr);
        for (size_t tu_idx = 0; tu_idx < driver.tu_names.len; tu_idx++) {
            size_t max_len = link_command.len-next_offset+1;
            snprintf(&link_command.cstr[next_offset], max_len,
                " %s.o", driver.tu_names.strs[tu_idx].cstr);
            next_offset += driver.tu_names.strs[tu_idx].len+3;
        }
        printf("[DEBUG] link command:\n%s\n", link_command.cstr);
        int link_res = system(link_command.cstr);
        printf("link_res was %d\n", link_res);
        String_free(&link_command);
    }

    if (driver.opts.bf < BF_EMIT_OBJECT) {
        for (size_t tu_idx = 0; tu_idx < driver.tu_names.len; tu_idx++) {
            String obj_path = String_init_length(driver.tu_names.strs[tu_idx].len+2);
            snprintf(obj_path.cstr, obj_path.len+1, "%s.o", driver.tu_names.strs[tu_idx].cstr);
            remove(obj_path.cstr);
            String_free(&obj_path);
        }
    } else {
        printf("Emitting objects.\n");
    }

#ifdef DEBUG
    printf("Final Interned Strings [%ld/%ld] (load/cap):\n",
        driver.str_table.load, driver.str_table.cap);
    for (size_t is_idx = 0; is_idx < driver.str_table.cap; is_idx++) {
        if (driver.str_table.strs[is_idx].status == ISS_OCCUPIED) {
            printf("\t[%4ld] `%s`\n", is_idx, driver.str_table.strs[is_idx].str->cstr);
        }
    }
#endif

    CompDriver_deinit(&driver);


    if (compiler_erred) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
