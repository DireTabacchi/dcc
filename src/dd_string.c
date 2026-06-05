#include <stdlib.h>
#include <string.h>
#include "dd_string.h"

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

void String_free(String *str) {
    free(str->cstr);
}
