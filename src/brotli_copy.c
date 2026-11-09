#include "brotli_copy.h"

const char *cx9_label_name(cx9_label l) {
  static const char *n[] = {"SUCCESS",      "NEEDS_MORE_INPUT",
                            "STALLED_TRUNCATED", "DEST_OVERRUN",
                            "SRC_OVERREAD", "FORMAT_ERROR", "MODEL_LIMIT",
                            "BAD_INPUT"};
  if ((int)l < 0 || (int)l >= (int)(sizeof n / sizeof n[0])) return "UNKNOWN";
  return n[l];
}
const char *cx9_version_name(cx9_version v) {
  static const char *n[] = {"pre", "fix", "size_t_min"};
  if ((int)v < 0 || (int)v >= (int)(sizeof n / sizeof n[0])) return "UNKNOWN";
  return n[v];
}

static uint64_t size_t_mask(int bits) {
  return bits >= 64 ? UINT64_MAX : (((uint64_t)1 << bits) - 1);
}

/* (int)x for a 32-bit int: C11 6.3.1.3p3 leaves out-of-range values
 * implementation-defined; GCC and Clang reduce modulo 2^32. The model does
 * that explicitly so the model itself has no implementation-defined step. */
int64_t cx9_to_int32(uint64_t v) {
  uint64_t lo = v & 0xFFFFFFFFu;
  return lo >= 0x80000000u ? (int64_t)lo - ((int64_t)1 << 32) : (int64_t)lo;
}

uint64_t cx9_remaining(cx9_version v, uint64_t avail_in, unsigned acc_bytes,
                       int size_t_bits) {
  uint64_t mask = size_t_mask(size_t_bits);
  if (v == CX9_FIX) {
    /* post bit_reader.h:91-92 */
    uint64_t cap = (uint64_t)1 << CX9_LARGE_MAX_WBITS;
    if (avail_in > cap) return cap;
  }
  /* pre bit_reader.h:98: br->avail_in + (BrotliGetAvailableBits(br) >> 3) */
  return (avail_in + acc_bytes) & mask;
}

int cx9_validate(const cx9_input *in) {
  if (!in) return -1;
  if (in->window_bits < CX9_MIN_WBITS || in->window_bits > CX9_LARGE_MAX_WBITS)
    return -1;
  /* the shift below is safe: window_bits is now 10..30 */
  if (in->rb_size < 1 || in->rb_size > ((int64_t)1 << in->window_bits)) return -1;
  if (in->pos < 0 || in->pos >= in->rb_size) return -1;
  if (in->size_t_bits != 32 && in->size_t_bits != 64) return -1;
  if (in->acc_bytes > CX9_MAX_ACC_BYTES) return -1;
  if (in->meta_len < 1 || in->meta_len > CX9_MAX_META_LEN) return -1;
  if (in->avail_in > size_t_mask(in->size_t_bits)) return -1;
  /* also with 64-bit size_t the sum avail_in + acc_bytes must not wrap */
  if (in->avail_in > UINT64_MAX - in->acc_bytes) return -1;
  if (in->size_t_bits == 32 &&
      in->avail_in + in->acc_bytes > (uint64_t)0xFFFFFFFFu) /* no wrap: <= 2^32+7 */
    return -1;
  return 0;
}

cx9_result cx9_run(cx9_version v, const cx9_input *in) {
  cx9_result r = {0};
  uint64_t mask;
  if ((int)v < (int)CX9_PRE || (int)v > (int)CX9_SIZE_T_MIN || cx9_validate(in)) {
    r.label = CX9_BAD_INPUT;
    return r;
  }
  mask = size_t_mask(in->size_t_bits);
  uint64_t avail = in->avail_in;
  unsigned acc = in->acc_bytes;
  int64_t meta = in->meta_len, pos = in->pos;
  int64_t full = (int64_t)1 << in->window_bits;
  uint64_t alloc = (uint64_t)in->rb_size + CX9_SLACK; /* decode.c:1333 */

  for (;;) {
    uint64_t rem = cx9_remaining(v, avail, acc, in->size_t_bits);
    uint64_t truth = (avail + acc) & mask;
    int64_t nbytes, cast;
    uint64_t copy;
    if (r.steps == CX9_MAX_STEPS) {
      r.label = CX9_MODEL_LIMIT;
      return r;
    }
    r.steps++;
    cast = v == CX9_SIZE_T_MIN ? -1 : cx9_to_int32(rem);
    if (v == CX9_SIZE_T_MIN) {
      /* counterfactual control: clamp in size_t, no narrowing to int */
      uint64_t n = rem;
      if (n > (uint64_t)meta) n = (uint64_t)meta;
      if (n > (uint64_t)(in->rb_size - pos)) n = (uint64_t)(in->rb_size - pos);
      nbytes = (int64_t)n;
    } else {
      nbytes = cast;                                   /* decode.c:1366 */
      if (nbytes > meta) nbytes = meta;                /* 1367-1369 */
      if (pos + nbytes > INT32_MAX || pos + nbytes < INT32_MIN)
        r.int_overflow = 1;                            /* would be UB */
      if (pos + nbytes > in->rb_size) nbytes = in->rb_size - pos; /* 1370 */
    }
    if (r.steps == 1) {
      r.remaining = rem;
      r.nbytes = nbytes;
      r.truncated = cast >= 0 && (uint64_t)cast != rem;
    }
    copy = (uint64_t)nbytes & mask;                    /* (size_t)nbytes */
    r.copy_len = copy;
    r.room = alloc - (uint64_t)pos;
    if (copy > r.room) {                               /* 1373 */
      r.label = CX9_DEST_OVERRUN;
      return r;
    }
    if (copy > truth) {
      r.label = CX9_SRC_OVERREAD;
      return r;
    }
    /* BrotliCopyBytes: accumulator bytes first, then memcpy from next_in */
    if (copy <= acc) {
      acc -= (unsigned)copy;
    } else {
      avail -= copy - acc;
      acc = 0;
    }
    r.copied += copy;
    pos += nbytes;                                     /* 1374 */
    meta -= nbytes;                                    /* 1375 */
    if (pos < full) {                                  /* 1376-1381 */
      if (meta == 0)
        r.label = CX9_SUCCESS;
      else  /* the ring buffer is full-size here, so input remaining means
               the cast, not the input, stopped the copy */
        r.label = avail + acc > 0 ? CX9_STALLED_TRUNCATED
                                  : CX9_NEEDS_MORE_INPUT;
      return r;
    }
    /* WRITE state: output assumed always available (one-shot API) */
    if (meta < 0) {                                    /* WriteRingBuffer */
      r.label = CX9_FORMAT_ERROR;
      return r;
    }
    if (in->rb_size == full && pos >= in->rb_size) pos -= in->rb_size;
  }
}

#include <stddef.h>
#include "sweep.h"

static const uint64_t kAvail[] = {
    0, 1, 7, 8, 4095, 4096, 16777215, 16777216, 16777217,
    1073741816, 1073741824, 1073741825, 2147483639, 2147483640, 2147483647,
    2147483648u, 2147483649u, 4294967287u, 4294967295u,
    /* 64-bit size_t only: */
    4294967296ull, 4294967301ull, 4311744512ull, 6442450944ull,
    8589934591ull, 8589934592ull, 9223372036854775808ull,
    18446744073709551607ull};
static const unsigned kAcc[] = {0, 1, 8};
static const int64_t kMeta[] = {1, 4096, 16777216};
static const int kWbits[] = {10, 16, 24};

void cx9_run_sweep(cx9_version v, int size_t_bits, cx9_sweep *out) {
  size_t a, b, c, d;
  int p;
  *out = (cx9_sweep){{0}, 0, 0, 0, 0, 0};
  for (a = 0; a < sizeof kAvail / sizeof kAvail[0]; ++a) {
    if (size_t_bits == 32 && kAvail[a] > 0xFFFFFFFFu) continue;
    for (b = 0; b < 3; ++b) {
      /* precondition for 32-bit size_t: avail_in + accumulator bytes fit
       * (a 32-bit address space cannot hold more input) */
      if (size_t_bits == 32 && kAvail[a] + kAcc[b] > 0xFFFFFFFFu) continue;
      for (c = 0; c < 3; ++c)
        for (d = 0; d < 3; ++d)
          for (p = 0; p < 2; ++p) {
            cx9_input in;
            cx9_result r;
            in.avail_in = kAvail[a];
            in.acc_bytes = kAcc[b];
            in.meta_len = kMeta[c];
            in.window_bits = kWbits[d];
            in.rb_size = (int64_t)1 << kWbits[d];
            in.pos = p ? in.rb_size - 1 : 0;
            in.size_t_bits = size_t_bits;
            r = cx9_run(v, &in);
            out->cases++;
            out->count[r.label]++;
            out->truncated += r.truncated;
            out->int_overflow += r.int_overflow;
            if (r.label == CX9_DEST_OVERRUN) {
              if (!out->min_overrun_remaining ||
                  r.remaining < out->min_overrun_remaining)
                out->min_overrun_remaining = r.remaining;
              if (r.copy_len > out->max_copy_len_overrun)
                out->max_copy_len_overrun = r.copy_len;
            }
          }
    }
  }
}
