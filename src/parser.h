#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include <sys/types.h>
#include "tokenizer.h"
#include "ast.h"

typedef struct parser_ {
    Tokenizer tokenizer;

    ssize_t prev_idx;
    ssize_t curr_idx;
    AstNode *program;
    
    ErrorList errors;
} Parser;

void Parser_init(Parser *p, const char *path);
void Parser_destroy(Parser *p);
void Parser_print_ast(Parser *p);
void parse(Parser *p);

#endif // PARSER_H
