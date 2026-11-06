/* Fixtures and sweep against separately derived expectations, random
 * invariants, the size_t counterfactual control, PRACTICE levels 1-8. */
#include "sweep.h"
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
      for (k = 0; k < CX9_LABELS; ++k) CHECK(s.count[k] == c[k], "sweep count");
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

int main(int argc, char **argv) {
  test_fixtures(argc > 1 ? argv[1] : "fixtures");
  test_invariants();
  practice();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
