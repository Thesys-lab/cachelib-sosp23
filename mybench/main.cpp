
#include <glog/logging.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>

#include "bench.h"
#include "cache.h"
#include "cmd.h"
#include "reader.h"
#include "request.h"

int main(int argc, char *argv[]) {
  google::InitGoogleLogging("mybench");

  bench_opts_t opts = parse_cmd(argc, argv);
  struct bench_data bench_data;
  memset(&bench_data, 0, sizeof(bench_data));

  mycache_init(opts.cache_size_in_mb, opts.hashpower, &bench_data.cache,
               &bench_data.pool);

  if (opts.n_thread == 1) {
    trace_replay_run(&bench_data, &opts);
  } else {
    trace_replay_run_mt(&bench_data, &opts);
  }

  report_bench_result(&bench_data, &opts);

  return 0;
}
