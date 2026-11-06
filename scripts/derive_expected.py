#!/usr/bin/env python3
"""Separate derivation of the expectations, from brotli decode.c and
bit_reader.h at 0c5603e (pre) and 223d80c (fix), using Python integers.

C rules applied explicitly:
- (int) of a size_t: reduce modulo 2**32 into [-2**31, 2**31), which is
  what GCC/Clang do with this implementation-defined conversion;
- (size_t) of a negative int: add 2**width.

Writes fixtures/expected.tsv and fixtures/expected_sweep.tsv to stdout,
separated by a '#sweep' line.
"""
SLACK, CAP = 42, 1 << 30
LABELS = ["SUCCESS", "NEEDS_MORE_INPUT", "STALLED_TRUNCATED", "DEST_OVERRUN",
          "SRC_OVERREAD", "FORMAT_ERROR", "MODEL_LIMIT"]


def as_int(x):
    x %= 1 << 32
    return x - (1 << 32) if x >= 1 << 31 else x


def decode_uncompressed(version, avail, acc, meta, wbits, pos, width):
    """One CopyUncompressedBlockToOutput call with unlimited output."""
    size_t = 1 << width
    rb = 1 << wbits
    first = None
    copied = steps = 0
    while True:
        steps += 1
        real = (avail + acc) % size_t
        if version == "fix" and avail > CAP:
            remaining = CAP
        else:
            remaining = real
        if version == "size_t_min":
            nbytes = min(remaining, meta, rb - pos)
        else:
            nbytes = as_int(remaining)
            if nbytes > meta:
                nbytes = meta
            if pos + nbytes > rb:
                nbytes = rb - pos
        if first is None:
            first = nbytes
        length = nbytes % size_t
        if length > rb + SLACK - pos:
            return "DEST_OVERRUN", first, length, copied, steps
        if length > real:
            return "SRC_OVERREAD", first, length, copied, steps
        take = min(length, acc)
        acc -= take
        avail -= length - take
        copied += length
        pos += nbytes
        meta -= nbytes
        if pos < rb:
            if meta == 0:
                return "SUCCESS", first, length, copied, steps
            if avail + acc > 0:
                return "STALLED_TRUNCATED", first, length, copied, steps
            return "NEEDS_MORE_INPUT", first, length, copied, steps
        if meta < 0:
            return "FORMAT_ERROR", first, length, copied, steps
        pos -= rb


AVAIL = [0, 1, 7, 8, 4095, 4096, 2**24 - 1, 2**24, 2**24 + 1,
         2**30 - 8, 2**30, 2**30 + 1, 2**31 - 9, 2**31 - 8, 2**31 - 1,
         2**31, 2**31 + 1, 2**32 - 9, 2**32 - 1, 2**32, 2**32 + 5,
         2**32 + 2**24, 2**32 + 2**31, 2**33 - 1, 2**33, 2**63, 2**64 - 9]


def main():
    for line in open("fixtures/cases.tsv"):
        if line.startswith("#") or not line.strip():
            continue
        cid, *nums = line.split()
        avail, acc, meta, wbits, pos, width = map(int, nums)
        for v in ("pre", "fix", "size_t_min"):
            label, nb, ln, copied, steps = decode_uncompressed(
                v, avail, acc, meta, wbits, pos, width)
            print("\t".join(map(str, (cid, v, label, nb, ln, copied, steps))))
    print("#sweep")
    for v in ("pre", "fix", "size_t_min"):
        for width in (64, 32):
            counts = dict.fromkeys(LABELS, 0)
            for avail in AVAIL:
                for acc in (0, 1, 8):
                    if avail + acc >= 1 << width:
                        continue
                    for meta in (1, 4096, 1 << 24):
                        for wbits in (10, 16, 24):
                            for pos in (0, (1 << wbits) - 1):
                                counts[decode_uncompressed(
                                    v, avail, acc, meta, wbits, pos,
                                    width)[0]] += 1
            print("\t".join([v, str(width)] +
                            [str(counts[k]) for k in LABELS]))


if __name__ == "__main__":
    main()
