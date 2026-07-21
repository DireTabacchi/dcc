#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "dd_string.h"

#include "common.h"
#include "comp_driver.h"
#include "token.h"
#include "tokenizer.h"

// TODO: string interning

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

#ifdef DEBUG
    puts("File src -----------------------------------------------------------------------");
    printf("%s", t->src.cstr);
    puts("--------------------------------------------------------------------------------");
#endif

    TokenList_init(&t->tokens);

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

static char peek2(Tokenizer *t) {
    return t->src.cstr[t->read_offset+1];
}

static bool is_keyword(String kw, long start, long rest_len, const char *rest) {
    long expected_len = start + rest_len;
    if (expected_len != kw.len) {
        return false;
    }

    if (strncmp(&kw.cstr[start], rest, rest_len) == 0) {
    //if (strcmp(&kw.cstr[start], rest) == 0) {
        return true;
    }

    return false;
}

static void scan_identifier(CompDriver *cd, long offset) {
    Position ident_pos = {
        .offset = offset,
        .line = cd->tokenizer.line,
        .column = offset - cd->tokenizer.line_offset + 1
    };
    while (ISALPHA(cd->tokenizer.ch) || ISDIGIT(cd->tokenizer.ch)) advance(&cd->tokenizer);

    long ident_len = cd->tokenizer.offset - offset;
    String ident = (String){ .cstr = &cd->tokenizer.src.cstr[offset], .len = ident_len };

    Token tok = {0};
    tok.pos = ident_pos;
    tok.kind = TOKEN_IDENTIFIER;

    // Trie to check if identifier is a keyword
    switch (ident.cstr[0]) {
    case 'i':
        if (is_keyword(ident, 1, 2, "nt")) { // int
            tok.kind = TOKEN_KW_INT;
        }
        break;

    case 'r':
        if (is_keyword(ident, 1, 5, "eturn")) { // return
            tok.kind = TOKEN_KW_RETURN;
        }
        break;
    case 'v':
        if (is_keyword(ident, 1, 3, "oid")) { // void
            tok.kind = TOKEN_KW_VOID;
        }
        break;
    }

    tok.text = (String *)StrInterner_intern(&cd->str_table, ident);
    TokenList_append(&cd->tokenizer.tokens, tok);
}

static void scan_number(CompDriver *cd, Tokenizer *t, long offset) {
    Position tok_pos = { .offset = offset, .line = t->line, .column = offset - t->line_offset + 1 };
    while(ISDIGIT(t->ch)) advance(t);
    // TODO: maybe advance to the next token? like space or some non-word character?
    if (ISALPHA(t->ch)) {
        // Swallow the remaining characters
        while (ISALPHA(t->ch)) advance(t);
        String err_scan = String_init_length(t->offset - offset);
        memcpy(err_scan.cstr, &t->src.cstr[offset], err_scan.len);
        err_invalid_const_ident(&cd->errors, t->src_path, tok_pos, err_scan);
        String_free(&err_scan);
        return;
    }
    
    long num_len = t->offset - offset;
    String *num_str = (String *)malloc(sizeof(String));
    *num_str = String_init_length(num_len);
    memcpy(num_str->cstr, &t->src.cstr[offset], num_len);

    Token tok = {0};
    tok.pos = tok_pos;
    tok.text = num_str;
    tok.kind = TOKEN_CONSTANT;

    TokenList_append(&t->tokens, tok);
}

void tokenize(CompDriver *cd) {
    Tokenizer *t = &cd->tokenizer;
    while (cd->tokenizer.offset < cd->tokenizer.src.len) {
        while (ISWHITESPACE(t->ch) && t->offset < t->src.len) advance(t);

        long offset = t->offset;
        if (ISDIGIT(t->ch)) {
            scan_number(cd, t, offset);
        } else if (ISALPHA(t->ch)) {
            scan_identifier(cd, offset);
        } else {
            Token tok = {0};
            String lit = {0};
            tok.pos = (Position){ .offset = t->offset, .line = t->line, .column = t->offset - t->line_offset + 1 };
            switch(t->ch) {
            case '(':
                tok.kind = TOKEN_LEFT_PAREN;
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;
                
            case ')':
                tok.kind = TOKEN_RIGHT_PAREN;
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;

            case '{':
                tok.kind = TOKEN_LEFT_BRACE;
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;

            case '}':
                tok.kind = TOKEN_RIGHT_BRACE;
                //tok.text = String_init_length(1);
                //memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;

            case ';':
                tok.kind = TOKEN_SEMICOLON;
                //tok.text = String_init_length(1);
                //memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;

            case '=':
                if (peek(t) == '=') {
                    tok.kind = TOKEN_OP_DOUBLE_EQUAL;
                    //tok.text = String_init_length(2);
                    //memcpy(tok.text.cstr, &t->src.cstr[t->offset], 2);
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }
                tok.kind = TOKEN_OP_EQUAL;
                //tok.text = String_init_length(1);
                //memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;

            case '~':
                tok.kind = TOKEN_OP_COMPLEMENT;
                //tok.text = String_init_length(1);
                //memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;

            case '+': {
                char peeked = peek(t);

                if (peeked == '+') {
                    tok.kind = TOKEN_OP_INCREMENT;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                } else if (peeked == '=') {
                    tok.kind = TOKEN_OP_PLUS_EQUAL;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }

                tok.kind = TOKEN_OP_PLUS;
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;
            }

            case '*':
                if (peek(t) == '=') {
                    tok.kind = TOKEN_OP_ASTERISK_EQUAL;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }
                tok.kind = TOKEN_OP_ASTERISK;
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;

            case '/':
                if (peek(t) == '=') {
                    tok.kind = TOKEN_OP_SLASH_EQUAL;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }
                tok.kind = TOKEN_OP_SLASH;
                //tok.text = String_init_length(1);
                //memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;

            case '%':
                if (peek(t) == '=') {
                    tok.kind = TOKEN_OP_PERCENT_EQUAL;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }
                tok.kind = TOKEN_OP_PERCENT;
                //tok.text = String_init_length(1);
                //memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;

            case '-': {
                char peeked = peek(t);
                if (peeked == '-') {
                    tok.kind = TOKEN_OP_DECREMENT;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                } else if (peeked == '=') {
                    tok.kind = TOKEN_OP_MINUS_EQUAL;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }
                tok.kind = TOKEN_OP_MINUS;
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;
            }

            case '!':
                if (peek(t) == '=') {
                    tok.kind = TOKEN_OP_EXCLAMATION_EQUAL;
                    //tok.text = String_init_length(2);
                    //memcpy(tok.text.cstr, &t->src.cstr[t->offset], 2);
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }
                tok.kind = TOKEN_OP_EXCLAMATION;
                //tok.text = String_init_length(1);
                //memcpy(tok.text.cstr, &t->src.cstr[t->offset], 1);
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;

            case '&': {
                char peeked = peek(t);
                if (peeked == '&') {
                    tok.kind = TOKEN_OP_DOUBLE_AMP;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                } else if (peeked == '=') {
                    tok.kind = TOKEN_OP_AMPERSAND_EQUAL;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }
                tok.kind = TOKEN_OP_AMPERSAND;
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;
            }

            case '|': {
                char peeked = peek(t);
                if (peeked == '|') {
                    tok.kind = TOKEN_OP_DOUBLE_BAR;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                } else if (peeked == '=') {
                    tok.kind = TOKEN_OP_BAR_EQUAL;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }
                tok.kind = TOKEN_OP_BAR;
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;
            }

            case '^':
                if (peek(t) == '=') {
                    tok.kind = TOKEN_OP_CARET_EQUAL;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }
                tok.kind = TOKEN_OP_CARET;
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;

            case '<': {
                char peeked = peek(t);
                if (peeked == '<') {
                    if (peek2(t) == '=') {
                        tok.kind = TOKEN_OP_LSHFT_EQUAL;
                        lit.cstr = &t->src.cstr[offset];
                        lit.len = 3;
                        tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                        TokenList_append(&t->tokens, tok);
                        advance(t);
                        advance(t);
                        break;
                    }
                    tok.kind = TOKEN_OP_LSHFT;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                } else if (peeked == '=') {
                    tok.kind = TOKEN_OP_LTE;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }
                tok.kind = TOKEN_OP_LT;
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;
            }

            case '>': {
                char peeked = peek(t);
                if (peeked == '>') {
                    if (peek2(t) == '=') {
                        tok.kind = TOKEN_OP_RSHFT_EQUAL;
                        lit.cstr = &t->src.cstr[offset];
                        lit.len = 3;
                        tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                        TokenList_append(&t->tokens, tok);
                        advance(t);
                        advance(t);
                        break;
                    }
                    tok.kind = TOKEN_OP_RSHFT;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                } else if (peeked == '=') {
                    tok.kind = TOKEN_OP_GTE;
                    lit.cstr = &t->src.cstr[offset];
                    lit.len = 2;
                    tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                    TokenList_append(&t->tokens, tok);
                    advance(t);
                    break;
                }
                tok.kind = TOKEN_OP_GT;
                lit.cstr = &t->src.cstr[offset];
                lit.len = 1;
                tok.text = (String *)StrInterner_intern(&cd->str_table, lit);
                TokenList_append(&t->tokens, tok);
                break;
            }

            case -1:    // EOF
                tok.kind = TOKEN_EOF;
                tok.text = (String *)StrInterner_intern(&cd->str_table, token_literals[TOKEN_EOF]);
                TokenList_append(&t->tokens, tok);
                break;

            default: {
                // Found an unknown character; need to err
                err_stray_char(&cd->errors, t->src_path, tok.pos, t->ch);
                break;
            }
            }
            advance(t);
        }

    }
}

