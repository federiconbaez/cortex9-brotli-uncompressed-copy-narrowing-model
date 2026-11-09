#include "sweep.h"
#include "cli_parse.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void row(FILE *o, const char *id, cx9_version v, const cx9_result *r) {
  fprintf(o, "%s,%s,%s,%" PRIu64 ",%" PRId64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%ld,%d\n",
          id, cx9_version_name(v), cx9_label_name(r->label), r->remaining,
          r->nbytes, r->copy_len, r->room, r->copied, r->steps, r->truncated);
}

static int results(const char *cases, const char *out) {
  char path[512], line[512];
  FILE *in = fopen(cases, "r"), *o;
  int v, w, l;
  if (!in) return 1;
  snprintf(path, sizeof path, "%s/cases.csv", out);
  if (!(o = fopen(path, "w"))) { fclose(in); return 1; }
  fprintf(o, "id,version,label,remaining,nbytes,copy_len,room,copied,steps,truncated\n");
  while (fgets(line, sizeof line, in)) {
    char id[64], f[6][32], *fp[6];
    cx9_input ci;
    int k;
    if (line[0] == '#' || line[0] == '\n') continue;
    if (sscanf(line, "%63s %31s %31s %31s %31s %31s %31s", id, f[0], f[1],
               f[2], f[3], f[4], f[5]) != 7) {
      fprintf(stderr, "malformed line\n"); fclose(in); fclose(o); return 1;
    }
    for (k = 0; k < 6; ++k) fp[k] = f[k];
    if (cx9_parse_fields(fp, &ci)) { fprintf(stderr, "bad case %s\n", id); fclose(in); fclose(o); return 1; }
    for (v = 0; v < 3; ++v) {
      cx9_result r = cx9_run((cx9_version)v, &ci);
      row(o, id, (cx9_version)v, &r);
    }
  }
  fclose(in);
  fclose(o);
  snprintf(path, sizeof path, "%s/sweep.csv", out);
  if (!(o = fopen(path, "w"))) return 1;
  fprintf(o, "version,size_t_bits,cases");
  /* BAD_INPUT is never produced by the sweep; its column is not printed */
  for (l = 0; l <= CX9_MODEL_LIMIT; ++l) fprintf(o, ",%s", cx9_label_name((cx9_label)l));
  fprintf(o, ",truncated,int_overflow,min_overrun_remaining,max_copy_len_overrun\n");
  for (v = 0; v < 3; ++v)
    for (w = 64; w >= 32; w -= 32) {
      cx9_sweep s;
      cx9_run_sweep((cx9_version)v, w, &s);
      fprintf(o, "%s,%d,%ld", cx9_version_name((cx9_version)v), w, s.cases);
      for (l = 0; l <= CX9_MODEL_LIMIT; ++l) fprintf(o, ",%ld", s.count[l]);
      fprintf(o, ",%ld,%ld,%" PRIu64 ",%" PRIu64 "\n", s.truncated, s.int_overflow,
              s.min_overrun_remaining, s.max_copy_len_overrun);
    }
  fclose(o);
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 9 && strcmp(argv[1], "case") == 0) {
    cx9_input in;
    int vi = cx9_parse_version(argv[2]);
    cx9_version v;
    cx9_result r;
    if (vi < 0 || cx9_parse_fields(argv + 3, &in)) { fprintf(stderr, "bad arguments\n"); return 2; }
    v = (cx9_version)vi;
    r = cx9_run(v, &in);
    printf("id,version,label,remaining,nbytes,copy_len,room,copied,steps,truncated\n");
    row(stdout, "cli", v, &r);
    return 0;
  }
  if (argc == 4 && strcmp(argv[1], "results") == 0) return results(argv[2], argv[3]);
  fprintf(stderr,
          "usage: brcopy case pre|fix|size_t_min AVAIL_IN ACC_BYTES META_LEN WBITS POS SIZE_T_BITS\n"
          "       brcopy results fixtures/cases.tsv OUTDIR\n"
          "Arithmetic only: no input is read and nothing is decompressed.\n");
  return 2;
}
