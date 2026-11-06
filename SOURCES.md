# Sources

Local copies under `security-research-45/sources/next3/brotli/` (not in
this repo). The `decode.c`, `bit_reader.h` and `constants.h` copies at
both commits match `raw.githubusercontent.com/google/brotli/<sha>/c/...`
(checked 2026-11-06 by read-only GET).

| Source | Used for | SHA-256 of copy |
|---|---|---|
| google/brotli 223d80cfbec8fd346e32906c732c8ede21f0cea6 "Update (#826)", 2020-08-26, parent 0c5603e07bed1d5fbb45e38f9bdf0e4560fde3f0 | causal change, listed in the message | (commit metadata via GitHub API) |
| `c/dec/decode.c` at 0c5603e | 46 slack, 257-294 MLEN, 1333 allocation, 1355-1400 copy, WriteRingBuffer 1266-1310 | 112c764f568fb2b914fd6d3c10f03ba10ec84065e69fb96ea33b52e93ba69e80 |
| `c/dec/decode.c` at 223d80c | call site unchanged | 3c6209b73b0b41526448d2315d3a104bf58a3e46d3096574158ba93ef10480c8 |
| `c/dec/bit_reader.h` at 0c5603e | 97-99 remaining bytes, 339-350 BrotliCopyBytes | eca1b4c4516f5cc63e05511eb30c8b2d895115d0723814984317e3e2cc97c214 |
| `c/dec/bit_reader.h` at 223d80c | 90-94 the cap | 10003d311994ac246399b51d0694de442b21d50e71a45d27c18da65cd01eff68 |
| `c/common/constants.h` pre / post | `BROTLI_LARGE_MAX_WBITS` 30 | 6888b69e0b62b1ad2acc908890a2f519d31eea493a8520c96c7d6db8bf243688 / a5ffdd6c1e800e61723fd0f0e8e1f6f6989d470e560bac0f1d0aa03dcabd43f6 |
| v1.0.9 release notes | "decoder: fix integer overflow when input chunk is larger than 2GiB (CVE-2020-8927)" | 0e7a14b123321890d0ed5eb7291a8cfe3a16813ff0d902e0b3c29aa56aaf1609 |
| NVD CVE-2020-8927 JSON | description, "prior to 1.0.8" | bbbedd62a0bf7a1cf74b0909e60124d0da03d01b1ba295b291196b3fea6d3a39 |

GitHub compare (2026-11-06): v1.0.8 is 4 commits ahead of 223d80c and
0 behind, and v1.0.9 is also ahead and 0 behind. Both tags contain the
fix.
