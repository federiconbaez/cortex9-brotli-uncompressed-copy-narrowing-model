#include "cli_parse.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Strict decimal parsers: the token must start with a digit (or '-' for
 * signed fields), so leading spaces and '+' are refused; trailing junk,
 * an empty token and errno (ERANGE) are refused too. */
static int parse_u64(const char *s, uint64_t *out) {
  char *e;
  unsigned long long v;
  if (!s || !(*s >= '0' && *s <= '9')) return -1;
  errno = 0;
  v = strtoull(s, &e, 10);
  if (errno || *e) return -1;
  *out = (uint64_t)v;
  return 0;
}

static int parse_i64(const char *s, int64_t lo, int64_t hi, int64_t *out) {
  char *e;
  long long v;
  if (!s || !(*s == '-' || (*s >= '0' && *s <= '9'))) return -1;
  errno = 0;
  v = strtoll(s, &e, 10);
  if (errno || *e || v < lo || v > hi) return -1;
  *out = (int64_t)v;
  return 0;
}

int cx9_parse_fields(char **f, cx9_input *in) {
  int64_t acc, wbits, width;
  if (parse_u64(f[0], &in->avail_in) ||
      parse_i64(f[1], 0, CX9_MAX_ACC_BYTES, &acc) ||
      parse_i64(f[2], 1, CX9_MAX_META_LEN, &in->meta_len) ||
      parse_i64(f[3], CX9_MIN_WBITS, CX9_LARGE_MAX_WBITS, &wbits) ||
      parse_i64(f[4], 0, INT64_MAX, &in->pos) ||
      parse_i64(f[5], 32, 64, &width))
    return -1;
  in->acc_bytes = (unsigned)acc;
  in->window_bits = (int)wbits;
  in->rb_size = (int64_t)1 << in->window_bits; /* wbits checked 10..30 */
  in->size_t_bits = (int)width;
  return cx9_validate(in); /* pos < rb_size, width 32|64, 32-bit sum */
}

int cx9_parse_version(const char *s) {
  if (strcmp(s, "pre") == 0) return CX9_PRE;
  if (strcmp(s, "fix") == 0) return CX9_FIX;
  if (strcmp(s, "size_t_min") == 0) return CX9_SIZE_T_MIN;
  return -1;
}

