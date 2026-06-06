#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "dd_string.h"

#include "token.h"
#include "tokenizer.h"

static char advance(Tokenizer *t) {
    if (t->read_offset < t->src.len) {
        t->offset = t->read_offset;
        if (t->ch == '\n') {
            t->line++;
            t->line_offset = t->offset;
        }
        t->read_offset++;
        t->ch = t->src.cstr[t->offset];
    } else {
        t->offset = t->read_offset;
        t->ch = -1;
    }
    return t->ch;
}

void Tokenizer_init(Tokenizer *t, const char *src_path) {
    int src_fd = open(src_path, O_RDONLY);
    if (src_fd == -1) {
        printf("Unable to open file %s\n", src_path);
        perror("Error: ");
        return;
    }

    struct stat src_stat = {0};
    fstat(src_fd, &src_stat);

    t->src = String_init_length(src_stat.st_size);
    read(src_fd, t->src.cstr, t->src.len);
    t->src_path = String_init_cstr(src_path);

    puts("File src -----------------------------------------------------------------------");
    printf("%s", t->src.cstr);
    puts("--------------------------------------------------------------------------------");

    TokenList_init(&t->tokens);
    ErrorList_init(&t->errors);

    t->ch = 0;
    t->offset = -1;
    t->read_offset = 0;
    t->line = (t->src.len > 0) ? 1 : 0;
    t->line_offset = 0;

    advance(t);

    close(src_fd);
}

void Tokenizer_destroy(Tokenizer *t) {
    String_free(&t->src);
    String_free(&t->src_path);
    TokenList_destroy(&t->tokens);
    ErrorList_destroy(&t->errors);
}

// Check if a char `c` is a whitespace character.
#define ISWHITESPACE(c) ((c) == '\n' || (c) == '\r' || (c) == '\t' || (c) == ' ')
// Check if a char `c` is a digit character.
#define ISDIGIT(c) ('0' <= (c) && (c) <= '9')
// Check if a char `c` is an alphabetic character or underscore.
#define ISALPHA(c) \
    ('a' <= (c) && (c) <= 'z' || 'A' <= (c) && (c) <= 'Z' || (c) == '_')
// Check if a char `c` is a word character.
#define ISWORD(c) (ISALPHA(c) || ISDIGIT(c))

static char peek(Tokenizer *t) {
    return t->src.cstr[t->read_offset];
}

static bool is_keyword(String kw, long start, long rest_len, const char *rest) {
    long expected_len = start + rest_len;
    if (expected_len != kw.len) {
        return false;
    }

    if (strcmp(&kw.cstr[start], rest) == 0) {
        return true;
    }

    return false;
}

static void scan_identifier(Tokenizer *t, long offset) {
    TokenPos ident_pos = { .offset = offset, .line = t->line, .column = offset - t->line_offset + 1 };
    while (ISALPHA(t->ch) || ISDIGIT(t->ch)) advance(t);

    long ident_len = t->offset - offset;
    String ident = String_init_length(ident_len);
    memcpy(ident.cstr, &t->src.cstr[offset], ident_len);

    Token tok = {0};
    tok.pos = ident_pos;
    tok.text = ident;
    tok.kind = TOKEN_IDENTIFIER;

    // Trie to check if identifier is a keyword
    switch (tok.text.cstr[0]) {
    case 'i':
        if (is_keyword(tok.text, 1, 2, "nt")) { // int
            tok.kind = TOKEN_KW_INT;
        }
        break;

    case 'r':
        if (is_keyword(tok.text, 1, 5, "eturn")) { // return
            tok.kind = TOKEN_KW_RETURN;
        }
        break;
    case 'v':
        if (is_keyword(tok.text, 1, 3, "oid")) { // void
            tok.kind = TOKEN_KW_VOID;
        }
        break;
    }

    TokenList_append(&t->tokens, tok);
}

static void scan_number(Tokenizer *t, long offset) {
    TokenPos tok_pos = { .offset = offset, .line = t->line, .column = offset - t->line_offset + 1 };
    while(ISDIGIT(t->ch)) advance(t);
    // TODO: maybe advance to the next token? like space or some non-word character?
    if (ISALPHA(t->ch)) {
        // Swallow the remaining characters
        while (ISALPHA(t->ch)) advance(t);
        Error err = {0};
        err.file = String_init_cstr(t->src_path.cstr);
        err.pos = tok_pos;
        String err_scan = String_init_length(t->offset - offset);
        memcpy(err_scan.cstr, &t->src.cstr[offset], err_scan.len);
        err.desc = String_init_length(err_scan.len + 33);
        snprintf(err.desc.cstr, err.desc.len+1, "invalid constant or identifier '%s'", err_scan.cstr);
        String_free(&err_scan);
        ErrorList_append(&t->errors, err);
        return;
    }
    
    long num_len = t->offset - offset;
    String num_str = String_init_length(num_len);
    memcpy(num_str.cstr, &t->src.cstr[offset], num_len);

    Token tok = {0};
    tok.pos = tok_pos;
    tok.text = num_str;
    tok.kind = TOKEN_CONSTANT;

    TokenList_append(&t->tokens, tok);
}

void tokenize(Tokenizer *t) {
    while (t->offset < t->src.len) {
        while (ISWHITESPACE(t->ch) && t->offset < t->src.len) advance(t);

        long offset = t->offset;
        if (ISDIGIT(t->ch)) {
            scan_number(t, offset);
        } else if (ISALPHA(t->ch)) {
            scan_identifier(t, offset);
        } else {
            Token tok = {0};
            tok.pos = (TokenPos){ .offset = t->offset, .line = t->line, .column = t->offset - t->line_offset + 1 };
            switch(t->ch) {
            case '(':
                tok.kind = TOKEN_LEFT_PAREN;
                tok.text = String_init_length(1);
                memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                TokenList_append(&t->tokens, tok);
                break;
                
            case ')':
                tok.kind = TOKEN_RIGHT_PAREN;
                tok.text = String_init_length(1);
                memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                TokenList_append(&t->tokens, tok);
                break;

            case '{':
                tok.kind = TOKEN_LEFT_BRACE;
                tok.text = String_init_length(1);
                memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                TokenList_append(&t->tokens, tok);
                break;

            case '}':
                tok.kind = TOKEN_RIGHT_BRACE;
                tok.text = String_init_length(1);
                memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                TokenList_append(&t->tokens, tok);
                break;

            case ';':
                tok.kind = TOKEN_SEMICOLON;
                tok.text = String_init_length(1);
                memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                TokenList_append(&t->tokens, tok);
                break;

            case -1:    // EOF
                tok.kind = TOKEN_EOF;
                tok.text = String_init_cstr(token_literal_list[TOKEN_EOF]);
                TokenList_append(&t->tokens, tok);
                break;

            default: {
                // Found an unknown character; need to err
                Error err = {0};
                err.pos = tok.pos;
                err.file = String_init_cstr(t->src_path.cstr);
                err.desc = String_init_length(20);
                snprintf(err.desc.cstr, err.desc.len+1, "stray '%c' in program", t->ch);
                ErrorList_append(&t->errors, err);
                break;
            }
            }
            advance(t);
        }

    }
}

