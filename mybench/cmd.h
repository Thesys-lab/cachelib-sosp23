#pragma once

#include <cassert>

#include "bench.h"
#include "reader.h"
#include "string.h"

// extern struct reader *reader;


// static void parse_ttl_dict_str(char *ttl_dict_str, bench_opts_t *opts) {
//   char *curr = ttl_dict_str;
//   char *new_pos;
//   int32_t ttl;
//   double perc;
//   int ttl_array_idx = 0;
//   while (curr != NULL) {
//     ttl = strtol(curr, &new_pos, 10);
//     curr = new_pos;
//     new_pos = strchr(curr, ':');
//     assert(new_pos != NULL);
//     curr = new_pos + 1;
//     perc = strtod(curr, &new_pos);
//     for (int i = 0; i < (int)(perc * 100); i++) {
//       opts->default_ttls[ttl_array_idx + i] = ttl;
//     }
//     ttl_array_idx += (int)(perc * 100);
//     printf("find TTL %" PRId32 ": perc %.4lf, ", ttl, perc);
//     curr = new_pos;
//     new_pos = strchr(curr, ',');
//     curr = new_pos == NULL ? NULL : new_pos + 1;
//   }
//   printf("\n");

//   if (ttl_array_idx != 100) {
//     assert(ttl_array_idx == 99);
//     opts->default_ttls[99] = opts->default_ttls[98];
//   }
// }

static bench_opts_t parse_cmd(int argc, char *argv[]) {
  bench_opts_t opts = create_default_bench_opts();

  if (argc < 3) {
    printf(
        "usage: %s trace_path cache_size_in_MB [hashpower] [n_thread]\n",
        argv[0]);
    exit(1);
  }

  strncpy(opts.trace_path, argv[1], MAX_TRACE_PATH_LEN);
  opts.cache_size_in_mb = atoll(argv[2]);
  if (argc >= 4) {
    opts.hashpower = atoll(argv[3]);
  }
  if (argc >= 5) {
    opts.n_thread = atoi(argv[4]);
  }

  return opts;
}
