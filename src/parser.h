#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include <sys/types.h>
#include "sym_table.h"
#include "tokenizer.h"
#include "ast.h"

//typedef struct symTable_ SymTable;

typedef struct parser_ {
    // (prev_idx == curr_idx) -> end of input
    ssize_t prev_idx;
    ssize_t curr_idx;
    AstProgram *program;

    SymTable syms;
} Parser;

typedef struct compDriver_ CompDriver;

void Parser_init(Parser *p);
void Parser_destroy(Parser *p);
void Parser_print_ast(Parser *p);
void parse(CompDriver *cd);

#endif // PARSER_H
