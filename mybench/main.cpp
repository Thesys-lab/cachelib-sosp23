
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
  bench_data.reader = open_trace(opts.trace_path, opts.trace_type);
  bench_data.report_interval = opts.report_interval;

  bench_data.cache_size_in_mb = opts.cache_size_in_mb;
  mycache_init(opts.cache_size_in_mb, opts.hashpower, &bench_data.cache,
               &bench_data.pool);

  trace_replay_run(&bench_data);
  report_bench_result(&bench_data);
  benchmark_destroy(&bench_data);

  return 0;
}
