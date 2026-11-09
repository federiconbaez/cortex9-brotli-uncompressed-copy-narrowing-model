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
  CX9_MODEL_LIMIT,         /* step bound reached */
  CX9_BAD_INPUT            /* outside cx9_validate's domain: nothing computed */
} cx9_label;

#define CX9_MIN_WBITS 10
#define CX9_MAX_META_LEN ((int64_t)1 << 24)
#define CX9_MAX_ACC_BYTES 8u

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
/* 0 when the input is inside the model's domain, -1 otherwise:
 * in != NULL; window_bits 10..30; rb_size 1..2^window_bits; pos
 * 0..rb_size-1; size_t_bits 32 or 64; acc_bytes 0..8; meta_len 1..2^24;
 * avail_in <= SIZE_MAX of that width and avail_in + acc_bytes <= 2^64-1;
 * with 32-bit size_t also
 * avail_in + acc_bytes <= 2^32-1. No shift happens before these checks. */
int cx9_validate(const cx9_input *in);
/* Returns label CX9_BAD_INPUT (all other fields 0) when cx9_validate fails
 * or v is not a known version. */
cx9_result cx9_run(cx9_version v, const cx9_input *in);
const char *cx9_label_name(cx9_label l);
const char *cx9_version_name(cx9_version v);
#endif
