/* Fixtures and sweep against separately derived expectations, random
 * invariants, the size_t counterfactual control, PRACTICE levels 1-8. */
#include "sweep.h"
#include "cli_parse.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures;
#define CHECK(c, what)                                                       \
  do {                                                                       \
    checks++;                                                                \
    if (!(c)) {                                                              \
      failures++;                                                            \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, what);                  \
    }                                                                        \
  } while (0)

static int version_of(const char *s) {
  int v;
  for (v = 0; v < 3; ++v)
    if (strcmp(s, cx9_version_name((cx9_version)v)) == 0) return v;
  return -1;
}

static cx9_input mk(uint64_t avail, unsigned acc, int64_t meta, int wbits,
                    int64_t pos, int width) {
  cx9_input in;
  in.avail_in = avail;
  in.acc_bytes = acc;
  in.meta_len = meta;
  in.window_bits = wbits;
  in.rb_size = (int64_t)1 << wbits;
  in.pos = pos;
  in.size_t_bits = width;
  return in;
}

static void test_fixtures(const char *dir) {
  char path[512], line[512];
  cx9_input cases[32];
  char ids[32][64];
  int n = 0, rows = 0, sweep_rows = 0, in_sweep = 0, i;
  FILE *f;
  snprintf(path, sizeof path, "%s/cases.tsv", dir);
  f = fopen(path, "r");
  CHECK(f != NULL, "open cases.tsv");
  if (!f) return;
  while (n < 32 && fgets(line, sizeof line, f)) {
    unsigned long long a;
    unsigned acc;
    long long meta, pos;
    int wb, w;
    if (line[0] == '#' || line[0] == '\n') continue;
    if (sscanf(line, "%63s %llu %u %lld %d %lld %d", ids[n], &a, &acc, &meta,
               &wb, &pos, &w) == 7) {
      cases[n] = mk(a, acc, meta, wb, pos, w);
      n++;
    }
  }
  fclose(f);
  snprintf(path, sizeof path, "%s/expected.tsv", dir);
  f = fopen(path, "r");
  CHECK(f != NULL, "open expected.tsv");
  if (!f) return;
  while (fgets(line, sizeof line, f)) {
    char id[64], ver[16], label[32];
    long long nb;
    unsigned long long len, copied;
    long steps, c[7];
    int v, w, k;
    if (strncmp(line, "#sweep", 6) == 0) { in_sweep = 1; continue; }
    if (in_sweep) {
      cx9_sweep s;
      if (sscanf(line, "%15s %d %ld %ld %ld %ld %ld %ld %ld", ver, &w, &c[0],
                 &c[1], &c[2], &c[3], &c[4], &c[5], &c[6]) != 9) continue;
      v = version_of(ver);
      CHECK(v >= 0, ver);
      if (v < 0) continue;
      cx9_run_sweep((cx9_version)v, w, &s);
      for (k = 0; k <= CX9_MODEL_LIMIT; ++k) CHECK(s.count[k] == c[k], "sweep count");
      CHECK(s.count[CX9_BAD_INPUT] == 0, "sweep never BAD_INPUT");
      CHECK(s.int_overflow == 0, "no int overflow in pos + nbytes");
      sweep_rows++;
      continue;
    }
    if (sscanf(line, "%63s %15s %31s %lld %llu %llu %ld", id, ver, label, &nb,
               &len, &copied, &steps) != 7) continue;
    v = version_of(ver);
    for (i = 0; i < n; ++i)
      if (strcmp(id, ids[i]) == 0 && v >= 0) {
        cx9_result r = cx9_run((cx9_version)v, &cases[i]);
        CHECK(strcmp(cx9_label_name(r.label), label) == 0, id);
        CHECK(r.nbytes == nb, id);
        CHECK(r.copy_len == len, id);
        CHECK(r.copied == copied, id);
        CHECK(r.steps == steps, id);
        rows++;
      }
  }
  fclose(f);
  CHECK(n == 11 && rows == 3 * n, "every fixture row checked");
  CHECK(sweep_rows == 6, "every sweep row checked");
}

static uint64_t lcg(uint64_t *s) {
  *s = *s * 6364136223846793005ull + 1442695040888963407ull;
  return *s;
}

static void test_invariants(void) {
  uint64_t seed = 6127;
  int i;
  for (i = 0; i < 200000; ++i) {
    int width = (i & 1) ? 64 : 32;
    int wbits = 10 + (int)(lcg(&seed) % 15);           /* 10..24 */
    int64_t rb = (int64_t)1 << wbits;
    uint64_t avail = lcg(&seed) >> (lcg(&seed) % 64);
    unsigned acc = (unsigned)(lcg(&seed) % 9);
    int64_t meta = 1 + (int64_t)(lcg(&seed) % (1u << 24));
    int64_t pos = (int64_t)(lcg(&seed) % (uint64_t)rb);
    cx9_input in;
    cx9_result pre, fix, ctl;
    if (width == 32) avail &= 0xFFFFFFF0u;
    in = mk(avail, acc, meta, wbits, pos, width);
    pre = cx9_run(CX9_PRE, &in);
    fix = cx9_run(CX9_FIX, &in);
    ctl = cx9_run(CX9_SIZE_T_MIN, &in);
    /* fix and control never overrun or over-read, and agree */
    CHECK(fix.label != CX9_DEST_OVERRUN && fix.label != CX9_SRC_OVERREAD,
          "fix safe");
    CHECK(ctl.label == fix.label && ctl.copied == fix.copied, "fix == control");
    CHECK(fix.label != CX9_STALLED_TRUNCATED, "fix never stalls");
    CHECK(fix.nbytes >= 0 && fix.nbytes <= meta && fix.nbytes <= rb - pos,
          "fix nbytes within clamps");
    /* pre overruns exactly when the first cast is negative */
    CHECK((pre.label == CX9_DEST_OVERRUN) ==
              (cx9_to_int32(cx9_remaining(CX9_PRE, avail, acc, width)) < 0),
          "pre overrun iff negative cast");
    /* pre agrees with the fix whenever remaining input is below 2^31 */
    if (avail + acc < 0x80000000u)
      CHECK(pre.label == fix.label && pre.copied == fix.copied,
            "pre == fix below 2 GiB");
    CHECK(!pre.int_overflow && !fix.int_overflow, "no int overflow");
  }
}

static void practice(void) {
  cx9_input in;
  cx9_result r;
  /* 1: a small stored block is copied whole */
  in = mk(100, 0, 100, 16, 0, 64);
  r = cx9_run(CX9_PRE, &in);
  CHECK(r.label == CX9_SUCCESS && r.copied == 100, "practice_level_1");
  /* 2: 2^31 - 1 still fits an int: clamped to meta_len */
  in = mk(2147483647u, 0, 4096, 16, 0, 64);
  r = cx9_run(CX9_PRE, &in);
  CHECK(r.nbytes == 4096 && r.label == CX9_SUCCESS, "practice_level_2");
  /* 3: 2^31 becomes INT_MIN */
  CHECK(cx9_to_int32(2147483648u) == -2147483647 - 1, "practice_level_3");
  /* 4: both clamps let a negative value through; the size_t is huge */
  in = mk(2147483648u, 0, 4096, 16, 0, 64);
  r = cx9_run(CX9_PRE, &in);
  CHECK(r.label == CX9_DEST_OVERRUN && r.copy_len == 18446744071562067968ull,
        "practice_level_4");
  /* 5: the accumulator bytes count too */
  in = mk(2147483640u, 8, 4096, 16, 0, 64);
  r = cx9_run(CX9_PRE, &in);
  CHECK(r.label == CX9_DEST_OVERRUN, "practice_level_5");
  /* 6: 2^32 + 5 truncates to 5: no overrun, but the copy stalls */
  in = mk(4294967301ull, 0, 4096, 16, 0, 64);
  r = cx9_run(CX9_PRE, &in);
  CHECK(r.label == CX9_STALLED_TRUNCATED && r.copied == 5, "practice_level_6");
  /* 7: the fix caps at 2^30: the same inputs succeed */
  in = mk(2147483648u, 0, 4096, 16, 0, 64);
  r = cx9_run(CX9_FIX, &in);
  CHECK(r.label == CX9_SUCCESS && r.remaining == (1u << 30),
        "practice_level_7");
  /* 8: control: clamping in size_t without the cast is also safe */
  r = cx9_run(CX9_SIZE_T_MIN, &in);
  CHECK(r.label == CX9_SUCCESS && r.copied == 4096, "practice_level_8");
}

/* Adversarial inputs: refused with BAD_INPUT before any shift, under
 * ASan + UBSan with -fno-sanitize-recover. */
static void expect_bad(cx9_input in, const char *what) {
  int v;
  CHECK(cx9_validate(&in) != 0, what);
  for (v = 0; v < 3; ++v) {
    cx9_result r = cx9_run((cx9_version)v, &in);
    CHECK(r.label == CX9_BAD_INPUT && r.steps == 0 && r.copied == 0 &&
              r.remaining == 0, what);
  }
}

static int parse_line(const char *a, const char *b, const char *c,
                      const char *d, const char *e, const char *g) {
  char f[6][32], *fp[6];
  cx9_input in;
  int k;
  snprintf(f[0], 32, "%s", a); snprintf(f[1], 32, "%s", b);
  snprintf(f[2], 32, "%s", c); snprintf(f[3], 32, "%s", d);
  snprintf(f[4], 32, "%s", e); snprintf(f[5], 32, "%s", g);
  for (k = 0; k < 6; ++k) fp[k] = f[k];
  return cx9_parse_fields(fp, &in);
}

static void test_adversarial(void) {
  static const int bad_wbits[] = {-1, 0, 9, 31, 63, 64, 1000, INT32_MIN, INT32_MAX};
  cx9_input ok = mk(100, 0, 100, 16, 0, 64), in;
  unsigned i;
  CHECK(cx9_validate(&ok) == 0, "baseline valid");
  CHECK(cx9_validate(NULL) != 0, "NULL refused");
  CHECK(cx9_run(CX9_PRE, NULL).label == CX9_BAD_INPUT, "run NULL");
  for (i = 0; i < sizeof bad_wbits / sizeof bad_wbits[0]; ++i) {
    in = ok;
    in.window_bits = bad_wbits[i];  /* rb_size left at 1 << 16 */
    expect_bad(in, "window_bits out of 10..30");
  }
  in = ok; in.pos = -1; expect_bad(in, "pos -1");
  in = ok; in.pos = INT64_MIN; expect_bad(in, "pos INT64_MIN");
  in = ok; in.pos = in.rb_size; expect_bad(in, "pos == rb_size");
  in = ok; in.pos = INT64_MAX; expect_bad(in, "pos INT64_MAX");
  in = ok; in.rb_size = 0; in.pos = 0; expect_bad(in, "rb_size 0");
  in = ok; in.rb_size = -5; in.pos = 0; expect_bad(in, "rb_size negative");
  in = ok; in.rb_size = ((int64_t)1 << 16) + 1; expect_bad(in, "rb_size > 2^wbits");
  in = ok; in.size_t_bits = 16; expect_bad(in, "size_t_bits 16");
  in = ok; in.size_t_bits = 0; expect_bad(in, "size_t_bits 0");
  in = ok; in.size_t_bits = 128; expect_bad(in, "size_t_bits 128");
  in = ok; in.acc_bytes = 9; expect_bad(in, "acc 9");
  in = ok; in.acc_bytes = UINT32_MAX; expect_bad(in, "acc UINT_MAX");
  in = ok; in.meta_len = 0; expect_bad(in, "meta 0");
  in = ok; in.meta_len = -1; expect_bad(in, "meta -1");
  in = ok; in.meta_len = CX9_MAX_META_LEN + 1; expect_bad(in, "meta 2^24+1");
  in = ok; in.size_t_bits = 32; in.avail_in = 0x100000000ull; expect_bad(in, "avail > 32-bit mask");
  in = ok; in.size_t_bits = 32; in.avail_in = 0xFFFFFFF8u; in.acc_bytes = 8;
  expect_bad(in, "32-bit avail + acc > 2^32-1");
  in = ok; in.size_t_bits = 32; in.avail_in = 0xFFFFFFF7u; in.acc_bytes = 8;
  CHECK(cx9_validate(&in) == 0, "32-bit avail + acc == 2^32-1 accepted");
  in = mk(UINT64_MAX, 1, 100, 16, 0, 64); expect_bad(in, "64-bit avail + acc wraps");
  /* borders that must stay accepted */
  in = mk(UINT64_MAX - 8, 8, CX9_MAX_META_LEN, 30, ((int64_t)1 << 30) - 1, 64);
  CHECK(cx9_validate(&in) == 0, "largest 64-bit input accepted");
  CHECK(cx9_run(CX9_PRE, &in).label != CX9_BAD_INPUT, "largest runs");
  in = mk(0, 0, 1, 10, 0, 32); in.rb_size = 1;
  CHECK(cx9_validate(&in) == 0, "rb_size 1 accepted");
  CHECK(cx9_run((cx9_version)3, &ok).label == CX9_BAD_INPUT, "version 3");
  CHECK(cx9_run((cx9_version)-1, &ok).label == CX9_BAD_INPUT, "version -1");
  CHECK(strcmp(cx9_label_name((cx9_label)99), "UNKNOWN") == 0, "label 99");
  CHECK(strcmp(cx9_label_name((cx9_label)-1), "UNKNOWN") == 0, "label -1");
  CHECK(strcmp(cx9_label_name(CX9_BAD_INPUT), "BAD_INPUT") == 0, "label BAD_INPUT");
  CHECK(strcmp(cx9_version_name((cx9_version)3), "UNKNOWN") == 0, "version 3 name");
  CHECK(strcmp(cx9_version_name((cx9_version)-7), "UNKNOWN") == 0, "version -7 name");
  /* parser */
  CHECK(parse_line("100", "0", "100", "16", "0", "64") == 0, "parse good");
  CHECK(parse_line(" 100", "0", "100", "16", "0", "64") != 0, "parse leading space");
  CHECK(parse_line("+100", "0", "100", "16", "0", "64") != 0, "parse plus");
  CHECK(parse_line("100x", "0", "100", "16", "0", "64") != 0, "parse junk");
  CHECK(parse_line("", "0", "100", "16", "0", "64") != 0, "parse empty");
  CHECK(parse_line("-1", "0", "100", "16", "0", "64") != 0, "parse negative avail");
  CHECK(parse_line("99999999999999999999", "0", "100", "16", "0", "64") != 0, "parse ERANGE");
  CHECK(parse_line("100", "9", "100", "16", "0", "64") != 0, "parse acc 9");
  CHECK(parse_line("100", "0", "0", "16", "0", "64") != 0, "parse meta 0");
  CHECK(parse_line("100", "0", "100", " 16", "0", "64") != 0, "parse wbits space");
  CHECK(parse_line("100", "0", "100", "16x", "0", "64") != 0, "parse wbits junk");
  CHECK(parse_line("100", "0", "100", "64", "0", "64") != 0, "parse wbits 64");
  CHECK(parse_line("100", "0", "100", "-1", "0", "64") != 0, "parse wbits -1");
  CHECK(parse_line("100", "0", "100", "16", "65536", "64") != 0, "parse pos == rb");
  CHECK(parse_line("100", "0", "100", "16", "-1", "64") != 0, "parse pos -1");
  CHECK(parse_line("100", "0", "100", "16", "0", "16") != 0, "parse width 16");
  CHECK(parse_line("100", "0", "100", "16", "0", "64 ") != 0, "parse width trailing");
  CHECK(parse_line("100", "0", "100", "16", "0", "+64") != 0, "parse width plus");
  CHECK(parse_line("4294967295", "1", "100", "16", "0", "32") != 0, "parse 32-bit sum");
  CHECK(cx9_parse_version("pre") == CX9_PRE && cx9_parse_version("fix ") < 0 &&
            cx9_parse_version("") < 0, "parse version");
}

int main(int argc, char **argv) {
  test_fixtures(argc > 1 ? argv[1] : "fixtures");
  test_invariants();
  practice();
  test_adversarial();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
