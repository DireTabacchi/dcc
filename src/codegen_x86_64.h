#ifndef CODEGEN_X86_64_H
#define CODEGEN_X86_64_H

#include "codegen.h"

typedef enum reg_ {
    REG_EAX,
    REG_AL,
    REG_CL,
    REG_EDX,
    REG_DL,
    REG_R10D,
    REG_R10B,
    REG_R11D,
    REG_R11B,
    X64_REGISTERS_LENGTH
} X64Register;

typedef enum asmOpSize_ {
    AOS_BYTE,   // 1 byte
    AOS_DWORD    // 4 bytes
} AsmOpSize;

static char *cond_code_table[7] = {
    (char *)"INVALID",
    (char *)"e",
    (char *)"ne",
    (char *)"l",
    (char *)"le",
    (char *)"g",
    (char *)"ge"
};

void emit_program(CodegenDriver *cgd);

#endif
