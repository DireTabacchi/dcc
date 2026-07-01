#include <stdlib.h>
#include <stdio.h>

#include "dd_string.h"

#include "ast.h"

// How many spaces to include per indent level. Used in AstNode_print
#define SPACES_PER_INDENT 4

AstNode *AstNode_create() {
    AstNode *n = malloc(sizeof(AstNode));
    *n = (AstNode){0};
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
        AstNode_destroy(node->node.ret.expr);
        free(node);
        break;

    case ASTNODE_CONSTANT:
        free(node);
        break;

    case ASTNODE_UNARY:
        AstNode_destroy(node->node.unary.exp);
        free(node);
        break;

    case ASTNODE_BINARY:
        AstNode_destroy(node->node.binary.left);
        AstNode_destroy(node->node.binary.right);
        free(node);
    }
}

char *binary_op_names[13] = {
    (char *)"UNKNOWN",
    (char *)"Add",
    (char *)"Subtract",
    (char *)"Multiply",
    (char *)"Divide",
    (char *)"Remainder",

    (char *)"(B)AND",
    (char *)"(B)OR",
    (char *)"(B)XOR",
    (char *)"Lt",
    (char *)"Gt",
    (char *)"Left Shift",
    (char *)"Right Shift"
};

void AstNode_print(AstNode *node, int indent_lvl) {
    int spaces = indent_lvl * SPACES_PER_INDENT;

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
        AstNode_print(node->node.ret.expr, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    case ASTNODE_CONSTANT:
        printf("%2$*1$s%3$d)\n", spaces+9, "Constant(", node->node.constant);
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
        break;

    case ASTNODE_BINARY:
        printf("%2$*1$s\n", spaces+7, "Binary(");
        indent_lvl += 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        // TODO: turn the strings in the ternary into a table
        printf("%2$*1$s%3$s\n%5$*4$s\n",
            spaces+3, "op=",
            binary_op_names[node->node.binary.op],
            spaces+6,
            "left=(");
        AstNode_print(node->node.binary.left, indent_lvl+1);
        printf("%2$*1$c\n%4$*3$s\n", spaces+1, ')', spaces+7, "right=(");
        AstNode_print(node->node.binary.right, indent_lvl+1);
        printf("%2$*1$c\n", spaces+1, ')');
        indent_lvl -= 1;
        spaces = indent_lvl * SPACES_PER_INDENT;
        printf("%2$*1$c\n", spaces+1, ')');
        break;
    }
}
