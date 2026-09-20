#ifndef CODEGEN_X86_64_H
#define CODEGEN_X86_64_H

#include "comp_driver.h"
#include "codegen.h"

typedef enum reg_ {
    REG_AX,
    REG_CX,
    REG_DX,
    REG_DI,
    REG_SI,
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    X64REGISTERS_LENGTH
} X64Register;

typedef enum asmOpSize_ {
    AOS_BYTE,   // 1 byte
    AOS_DWORD,  // 4 bytes
    AOS_QWORD,  // 8 bytes
    AOS_LENGTH
} AsmOpSize;

static char *reg_table[X64REGISTERS_LENGTH][AOS_LENGTH] = {
    { (char *)"%al",   (char *)"%eax",    (char *)"%rax" },
    { (char *)"%cl",   (char *)"%ecx",    (char *)"%rcx" },
    { (char *)"%dl",   (char *)"%edx",    (char *)"%rdx" },
    { (char *)"%dil",  (char *)"%edi",    (char *)"%rdi" },
    { (char *)"%sil",  (char *)"%esi",    (char *)"%rsi" },
    { (char *)"%r8b",  (char *)"%r8d",    (char *)"%r8" },
    { (char *)"%r9b",  (char *)"%r9d",    (char *)"%r9" },
    { (char *)"%r10b", (char *)"%r10d",   (char *)"%r10" },
    { (char *)"%r11b", (char *)"%r11d",   (char *)"%r11" }
};

static char *cond_code_table[7] = {
    (char *)"INVALID",
    (char *)"e",
    (char *)"ne",
    (char *)"l",
    (char *)"le",
    (char *)"g",
    (char *)"ge"
};

void emit_program(CompDriver *cd);

#endif
