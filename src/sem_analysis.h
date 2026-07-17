#ifndef SEM_ANALYSIS_H
#define SEM_ANALYSIS_H

#include <stddef.h>
#include "dd_string.h"

typedef struct compDriver_ CompDriver;

void sem_analyze(CompDriver *cd);

#endif // SEM_ANALYSIS_H
