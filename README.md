# cortex9-brotli-uncompressed-copy-narrowing-model

Arithmetic model of **CVE-2020-8927** in the Brotli decoder. Before commit
223d80c, `CopyUncompressedBlockToOutput` stored the remaining input size
(`size_t`) in an `int`. With 2 GiB or more of input, the value became
negative, passed both clamps, and turned back into a huge `size_t` copy
length.

The model is in C, educational, offline and deterministic. It **reads no
input**, decompresses nothing, links no Brotli and copies no bytes. The
ring buffer is an abstract region of `ringbuffer_size + 42` bytes.

*Versión en español: [README.es.md](README.es.md).*

## The bug, from the source (google/brotli)

Pre-fix code, parent 0c5603e, `c/dec/decode.c`:

```c
int nbytes = (int)BrotliGetRemainingBytes(&s->br);        /* 1366 */
if (nbytes > s->meta_block_remaining_len) nbytes = ...;    /* 1367 */
if (s->pos + nbytes > s->ringbuffer_size) nbytes = ...;    /* 1370 */
BrotliCopyBytes(&s->ringbuffer[s->pos], &s->br, (size_t)nbytes); /* 1373 */
```

`BrotliGetRemainingBytes` returns `avail_in + accumulator bytes`
(`bit_reader.h:97-99`). From 2^31 upwards the `int` is negative (GCC/Clang
reduce modulo 2^32), and a negative value passes both `>` clamps.

The fix, 223d80c "Update (#826)" ("IMPORTANT: decoder: fix potential
overflow when input chunk is >2GiB"), changes only `bit_reader.h:90-94`:
the function returns at most `1 << BROTLI_LARGE_MAX_WBITS` (2^30). The call
site in `decode.c` is unchanged.

**Versions.** NVD says "prior to 1.0.8". Tags v1.0.8 and v1.0.9 both
contain 223d80c (GitHub compare, checked 2026-11-06), and the CVE line is
in the v1.0.9 release notes. The two releases were published nine minutes
apart, so these statements do not contradict each other.

## What the model checks

`cx9_run` follows one call through the NONE and WRITE states. Output is
always available (one-shot API) and the ring buffer is full-size. The
call ends with one of these labels:

| Label | Meaning |
|---|---|
| `SUCCESS` | the meta-block length reached 0 |
| `NEEDS_MORE_INPUT` | the input ran out |
| `STALLED_TRUNCATED` | the cast truncated the value to a small positive one, so the call returned although input remained |
| `DEST_OVERRUN` | the copy length is larger than the room left in the allocation (abstract count) |
| `SRC_OVERREAD` | the copy length is larger than the bytes really remaining |
| `FORMAT_ERROR` | the meta-block length went negative |
| `MODEL_LIMIT` | the step bound was reached |

Three versions are compared:

- **pre:** the parent code;
- **fix:** 223d80c;
- **`size_t_min`:** a counterfactual control that clamps in `size_t`
  with no cast.

`results/sweep.csv` covers a boundary grid: 27 `avail_in` values ×
accumulator {0, 1, 8} × meta {1, 4096, 2^24} × window {10, 16, 24} × pos
{0, end}, for 64- and 32-bit `size_t`.

| version | size_t | cases | DEST_OVERRUN | STALLED_TRUNCATED |
|---|---|---|---|---|
| pre | 64 | 1458 | 360 | 222 |
| pre | 32 | 990 | 234 | 0 |
| fix | 64 / 32 | 1458 / 990 | 0 | 0 |
| size_t_min | 64 / 32 | 1458 / 990 | 0 | 0 |

- The smallest remaining size that overruns is exactly 2^31.
- In 200,000 random cases, the pre code overruns **if and only if** the
  cast is negative, and it matches the fix below 2 GiB.
- `scripts/derive_expected.py` is a separate implementation using Python
  integers. It reproduces every fixture and every sweep count, and CI
  checks this.

## Usage

```sh
sh scripts/build.sh test       # -Wall -Wextra -Werror, ASan/UBSan
sh scripts/build.sh results /tmp/r
./build/brcopy case pre 2147483648 0 4096 16 0 64
```

See [LIMITATIONS.md](LIMITATIONS.md), [SOURCES.md](SOURCES.md) and
[PRACTICE.md](PRACTICE.md) (Spanish). License: Apache-2.0.
