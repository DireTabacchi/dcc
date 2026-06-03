#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "dd_string.h"
#include "token.h"

typedef struct tokenizer_ {
    String src;

    char ch;            // current character (src[offset])
    long offset;        // current offset of member ch (src[offset])
    long read_offset;   // offset + 1
    long line;          // current line number
    long line_offset;   // offset of beginning of line

    TokenList tokens;
    long error_count;
} Tokenizer;

void Tokenizer_init(Tokenizer *t, const char *path);
void Tokenizer_destroy(Tokenizer *t);
void tokenize(Tokenizer *t);

#endif // TOKENIZER_H
