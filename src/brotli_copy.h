/* Arithmetic model of CopyUncompressedBlockToOutput (brotli decode.c) and
 * BrotliGetRemainingBytes (bit_reader.h) before and after 223d80c
 * (CVE-2020-8927). The ring buffer is an abstract region; no byte is read,
 * copied or decompressed. */
#ifndef CX9_BROTLI_COPY_H
#define CX9_BROTLI_COPY_H
#include <stdint.h>

#define CX9_SLACK 42           /* kRingBufferWriteAheadSlack, decode.c:46 */
#define CX9_LARGE_MAX_WBITS 30 /* BROTLI_LARGE_MAX_WBITS, constants.h:56 */
#define CX9_MAX_STEPS (1L << 20)

typedef enum { CX9_PRE = 0, CX9_FIX = 1, CX9_SIZE_T_MIN = 2 } cx9_version;

typedef enum {
  CX9_SUCCESS = 0,         /* meta_block_remaining_len reached 0 */
  CX9_NEEDS_MORE_INPUT,    /* returned with pos < 1 << window_bits */
  CX9_STALLED_TRUNCATED,   /* NEEDS_MORE_INPUT although input remains */
  CX9_DEST_OVERRUN,        /* copy length > room left in the allocation */
  CX9_SRC_OVERREAD,        /* copy length > bytes really remaining */
  CX9_FORMAT_ERROR,        /* WriteRingBuffer: meta_block_remaining_len < 0 */
  CX9_MODEL_LIMIT          /* step bound reached */
} cx9_label;

typedef struct {
  uint64_t avail_in;      /* br->avail_in */
  unsigned acc_bytes;     /* BrotliGetAvailableBits(br) >> 3, 0..8 */
  int64_t meta_len;       /* s->meta_block_remaining_len (int) */
  int window_bits;        /* s->window_bits, 10..24 (30 large window) */
  int64_t rb_size;        /* s->ringbuffer_size (int) */
  int64_t pos;            /* s->pos (int) */
  int size_t_bits;        /* 64 or 32 */
} cx9_input;

typedef struct {
  cx9_label label;
  uint64_t remaining;     /* BrotliGetRemainingBytes as size_t */
  int64_t nbytes;         /* int nbytes after the cast, first step */
  uint64_t copy_len;      /* (size_t)nbytes, first faulting or last step */
  uint64_t room;          /* allocation room at that step */
  uint64_t copied;        /* total bytes copied before the end */
  long steps;             /* NONE-state copies */
  int truncated;          /* cast changed the value, result non-negative */
  int int_overflow;       /* pos + nbytes left int range (UB in C) */
} cx9_result;

int64_t cx9_to_int32(uint64_t v); /* two's-complement narrowing, explicit */
uint64_t cx9_remaining(cx9_version v, uint64_t avail_in, unsigned acc_bytes,
                       int size_t_bits);
cx9_result cx9_run(cx9_version v, const cx9_input *in);
const char *cx9_label_name(cx9_label l);
const char *cx9_version_name(cx9_version v);
#endif
