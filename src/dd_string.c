#include <stdlib.h>
#include <string.h>
#include "dd_string.h"
#include <stdio.h>

String String_init_cstr(const char *str) {
    String ret;
    ret.len = dd_strlen(str);
    ret.cstr = (char *)calloc(ret.len+1, sizeof(char));
    memcpy(ret.cstr, str, ret.len);
    return ret;
}

String String_init_length(size_t len) {
    String ret;
    ret.len = len;
    ret.cstr = (char *)calloc(ret.len+1, sizeof(char));
    return ret;
}

String String_copy(String src) {
    return String_init_cstr(src.cstr);
}

void String_free(String *str) {
    if (str == NULL) return;
    free(str->cstr);
    str->cstr = NULL;
    str->len = 0;
}

bool String_equal(const String str1, const String str2) {
    if (str1.len != str2.len) return false;
    if (!str1.cstr || !str2.cstr) return str1.cstr == str2.cstr;
    size_t n = str1.len;
    const char *s1 = str1.cstr;
    const char *s2 = str2.cstr;
    for ( ; n && *s1 == *s2; n--, s1++, s2++);
    return n == 0;
}

int String_cmp(const String str1, const String str2) {
    size_t min_len = (str1.len < str2.len) ? str1.len : str2.len;
    const unsigned char *s1 = (const unsigned char *)str1.cstr;
    const unsigned char *s2 = (const unsigned char *)str2.cstr;
    for (size_t i = 0; i < min_len; i++, s1++, s2++) {
        if (*s1 != *s2) return *s1 - *s2;
    }
    return (int)str1.len - (int)str2.len;
}

