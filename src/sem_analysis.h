#ifndef SEM_ANALYSIS_H
#define SEM_ANALYSIS_H

#include <stddef.h>
#include "dd_string.h"
#include "sym_table.h"

typedef struct semAnalyzer_ {
    size_t lbl_count;
    SymTable *lbl_table;
    SymTable *var_table;
} Sema;

typedef struct compDriver_ CompDriver;

void Sema_init(Sema *sa);
void Sema_deinit(Sema *sa);

void sem_analyze(CompDriver *cd);

#endif // SEM_ANALYSIS_H
