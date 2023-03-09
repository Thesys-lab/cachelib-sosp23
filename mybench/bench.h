#pragma once

#include "cache.h"
#include "reader.h"
#include "request.h"

struct bench_data {
  Cache *cache;
  PoolId pool;

  int64_t n_get;
  int64_t n_set;
  int64_t n_del;
  int64_t n_get_miss;

  struct timeval start_time;
  struct timeval end_time;
  int64_t trace_time;
};

typedef struct {
  int64_t cache_size_in_mb;
  int32_t report_interval;

  int n_thread;

  int hashpower;

  char trace_path[MAX_TRACE_PATH_LEN];
  enum trace_type trace_type;
} bench_opts_t;

static inline bench_opts_t create_default_bench_opts() {
  bench_opts_t opts;
  opts.cache_size_in_mb = 200;
  opts.hashpower = 26;
  opts.n_thread = 1;
  opts.report_interval = 86400;
  opts.trace_type = oracleGeneral;
  return opts;
}

static inline int cache_go(Cache *cache, PoolId pool, struct request *req,
                          int64_t *n_get, int64_t *n_set, int64_t *n_del,
                          int64_t *n_get_miss) {
  int status = 0;

  switch (req->op) {
    case op_get:
      (*n_get)++;
      status = cache_get(cache, pool, req);
      if (status == 1) {
        (*n_get_miss)++;
        (*n_set)++;
        status = cache_set(cache, pool, req);
      }
      break;
    case op_set:
      (*n_set)++;
      status = cache_set(cache, pool, req);
      break;
    case op_del:
      status = cache_del(cache, pool, req);
      (*n_del)++;
      break;
    case op_ignore:
      break;
    default:;
      printf("op not supported %d\n", req->op);
      assert(false);
  }

  return status;
}

void trace_replay_run(struct bench_data *bench_data, bench_opts_t *opts);

void report_bench_result(struct bench_data *bench_data, bench_opts_t *opts);

void trace_replay_run_mt(struct bench_data *bench_data, bench_opts_t *opts);
