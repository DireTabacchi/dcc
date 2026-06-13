#ifndef CODEGEN_X86_64_H
#define CODEGEN_X86_64_H

#include "codegen.h"

typedef enum reg_ {
    REG_EAX
} Register;

void emit_program(CodegenDriver *cgd);

#endif
