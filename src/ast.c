#include <stdlib.h>
#include <stdio.h>

#include "dd_string.h"

#include "ast.h"

// How many spaces to include per indent level. Used in AstNode_print
#define SPACES_PER_INDENT 4

AstNode *AstNode_create() {
    AstNode *n = malloc(sizeof(AstNode));

    return n;
}

void AstNode_destroy(AstNode *node) {
    if (node == NULL) return;

    switch (node->kind) {
    case ASTNODE_INVALID:
        free(node);
        break;
    case ASTNODE_PROGRAM:
        AstNode_destroy(node->node.program.function);
        free(node);
        break;
    case ASTNODE_FUNCTION:
        String_free(&node->node.function.name);
        AstNode_destroy(node->node.function.statement);
        free(node);
        break;
    case ASTNODE_RETURN:
        AstNode_destroy(node->node.ret.constant);
        free(node);
        break;
    case ASTNODE_CONSTANT:
        free(node);
        break;
    case ASTNODE_UNARY:
        AstNode_destroy(node->node.unary.exp);
        free(node);
        break;
    }
}

void AstNode_print(AstNode *node, int indent_lvl) {
    int spaces = indent_lvl * SPACES_PER_INDENT;

    //printf("indent level is: %d; spaces is: %d\n", indent_lvl, spaces);

    switch (node->kind) {
    case ASTNODE_INVALID:
        break;
    case ASTNODE_PROGRAM:
        printf("%*s\n", spaces, "Program(");
        AstNode_print(node->node.program.function, indent_lvl+1);
        printf("%2$*1$c\n", spaces, ')');
        break;
    case ASTNODE_FUNCTION:
        printf("%2$*1$s\n", spaces+9, "Function(");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s\"%3$s\"\n%5$*4$s",
            spaces+5, "name=", node->node.function.name.cstr, spaces+5, "body=");
        AstNode_print(node->node.function.statement, indent_lvl);
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    case ASTNODE_RETURN:
        printf("Return(\n");
        AstNode_print(node->node.ret.constant, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    case ASTNODE_CONSTANT:
        printf("%2$*1$s%3$d)\n", spaces+9, "Constant(", node->node.constant.c);
        break;
    case ASTNODE_UNARY:
        printf("%2$*1$s\n", spaces+6, "Unary(");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$s%3$s\n%5$*4$s\n", spaces+3, "op=", (node->node.unary.op == UNARY_NEGATE) ? "Negate" :
            ((node->node.unary.op == UNARY_COMPLEMENT) ? "Complement" : "Invald"), spaces + 5, "exp=(");
        AstNode_print(node->node.unary.exp, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
    }
}
