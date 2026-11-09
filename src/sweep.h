#ifndef CX9_SWEEP_H
#define CX9_SWEEP_H
#include "brotli_copy.h"
#define CX9_LABELS 8
typedef struct {
  long count[CX9_LABELS];
  long cases, truncated, int_overflow;
  uint64_t min_overrun_remaining; /* 0 = none */
  uint64_t max_copy_len_overrun;
} cx9_sweep;
/* Boundary grid; widths 64 and 32. Deterministic. */
void cx9_run_sweep(cx9_version v, int size_t_bits, cx9_sweep *out);
#endif
