# Limitations

- **One function, one state machine.** Only the NONE and WRITE states of
  `CopyUncompressedBlockToOutput` and `BrotliGetRemainingBytes` are
  modelled.
  - Not modelled: meta-block header parsing beyond the 2^24 length bound
    (`decode.c:257-294`), the outer `BrotliDecoderDecompressStream` loop,
    and how a `NEEDS_MORE_INPUT` with input left is handled there.
    `STALLED_TRUNCATED` therefore marks the return, not a final decoder
    result.
- **Output and ring buffer.**
  - Output is assumed always available, as with the one-shot API.
  - The ring buffer is full-size (`1 << window_bits`). The shrunken ring
    buffer used when the output size is known is not modelled.
- **The `int` cast.** C11 6.3.1.3p3 makes the out-of-range conversion
  implementation-defined. The model reduces modulo 2^32, as GCC and Clang
  do, and does so explicitly.
- **`DEST_OVERRUN` is a count.** It compares the copy length with the
  room left in the allocation. No memory is touched. The first faulting
  step ends the run.
- **32-bit `size_t`.** Rows assume `avail_in + accumulator` fits, since a
  32-bit address space cannot hold more.
- **The grid is a grid.** The sweep values are boundaries, and the random
  check is seeded. Neither is a proof over all inputs. The
  "if and only if" property is checked, not proved.
- **Same model, two implementations.** The Python and C implementations
  agree with each other, which checks the transcription. Neither runs
  Brotli.
- This is not a new finding.
