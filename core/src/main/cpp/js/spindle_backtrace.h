#pragma once

#include <stdint.h>

#define SPINDLE_BT_MAX 16

#ifdef __cplusplus
extern "C" {
#endif

struct spindle_cpu_snap {
    uint64_t pc;
    uint64_t sp;
    uint64_t fp;
    uint64_t lr;
    uint64_t x[29];
};

/* snap == NULL walks the current thread. fuzzy != 0 scans the stack.
 * Returns the number of frames written to pcs. */
int spindle_backtrace(const struct spindle_cpu_snap* snap, int fuzzy, uint64_t* pcs, int max);

#ifdef __cplusplus
}
#endif
