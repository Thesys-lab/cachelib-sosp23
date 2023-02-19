#pragma once

#include <cassert>

#ifdef __cplusplus
extern "C" {
#endif

#include "reader.h"
#include "string.h"

extern struct reader *reader;

typedef struct {
  int32_t default_ttls[100];
  int64_t cache_size_in_mb;
  int32_t report_interval;

  int hashpower;
  bool nottl;

  char trace_path[MAX_TRACE_PATH_LEN];
  enum trace_type trace_type;
} bench_opts_t;

bench_opts_t create_default_bench_opts() {
  bench_opts_t opts;
  opts.cache_size_in_mb = 200;
  opts.hashpower = 26;
  opts.report_interval = 3600;
  opts.nottl = true;
  opts.trace_type = oracleSysTwrNS;
  for (int i = 0; i < 100; i++) {
    opts.default_ttls[i] = 86400;
  }
  return opts;
}

static void parse_ttl_dict_str(char *ttl_dict_str, bench_opts_t *opts) {
  char *curr = ttl_dict_str;
  char *new_pos;
  int32_t ttl;
  double perc;
  int ttl_array_idx = 0;
  while (curr != NULL) {
    ttl = strtol(curr, &new_pos, 10);
    curr = new_pos;
    new_pos = strchr(curr, ':');
    assert(new_pos != NULL);
    curr = new_pos + 1;
    perc = strtod(curr, &new_pos);
    for (int i = 0; i < (int)(perc * 100); i++) {
      opts->default_ttls[ttl_array_idx + i] = ttl;
    }
    ttl_array_idx += (int)(perc * 100);
    printf("find TTL %" PRId32 ": perc %.4lf, ", ttl, perc);
    curr = new_pos;
    new_pos = strchr(curr, ',');
    curr = new_pos == NULL ? NULL : new_pos + 1;
  }
  printf("\n");

  if (ttl_array_idx != 100) {
    assert(ttl_array_idx == 99);
    opts->default_ttls[99] = opts->default_ttls[98];
  }
}

static bench_opts_t parse_cmd(int argc, char *argv[]) {
  bench_opts_t opts = create_default_bench_opts();

  if (argc < 6) {
    printf(
        "usage: %s trace_path trace_type cache_size_in_MB "
        "report_interval nottl\n",
        argv[0]);
    exit(1);
  }

  strncpy(opts.trace_path, argv[1], MAX_TRACE_PATH_LEN);
  opts.cache_size_in_mb = atoll(argv[3]);
  opts.report_interval = atoll(argv[4]);
  char *trace_type_str = argv[2];
  opts.nottl = atol(argv[5]);

  if (opts.nottl) {
    printf("no ttl\n");
  }
  // char *ttl_dict_str = argv[4];

  enum trace_type trace_type;
  if (strcmp(trace_type_str, "oracleSysTwrNS") == 0) {
    opts.trace_type = oracleSysTwrNS;
  } else if (strcmp(trace_type_str, "oracleGeneral") == 0) {
    opts.trace_type = oracleGeneral;
  } else {
    printf("unknown trace type %s\n", trace_type_str);
    abort();
  }

  // parse_ttl_dict_str(ttl_dict_str, &opts);

  return opts;
}

#ifdef __cplusplus
}
#endif