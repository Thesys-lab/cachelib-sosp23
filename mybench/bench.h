
#include "request.h"
#include "cache.h"

struct bench_data {
  struct reader *reader;
  Cache *cache;
  PoolId pool;

  int cache_size_in_mb;
  int64_t n_get;
  int64_t n_set;
  int64_t n_del;
  int64_t n_get_miss;

  struct timeval start_time;
  struct timeval end_time;
  int32_t trace_time;
  int32_t report_interval;
};


void *trace_replay_run(struct bench_data *bench_data);

void benchmark_destroy(struct bench_data *bench_data); 

void report_bench_result(struct bench_data *bench_data);
