
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <sys/time.h>
#include <sysexits.h>
#include <unistd.h>

#include "bench.h"
#include "reader.h"
#include "request.h"

void trace_replay_run(struct bench_data *bdata, bench_opts_t *opts) {
  struct request *req = new_request();
  struct reader *reader = open_trace(opts->trace_path, opts->trace_type);

  gettimeofday(&bdata->start_time, NULL);

  int status;
  read_trace(reader, req);
  int32_t trace_start_ts = req->timestamp;
  int32_t next_report_trace_ts = opts->report_interval > 0
                                     ? trace_start_ts + opts->report_interval
                                     : INT32_MAX;

  int warmup_time = 86400 * -1;

  bdata->n_get = bdata->n_set = bdata->n_get_miss = 0;
  bdata->n_del = 0;

  while (req->timestamp < warmup_time) {
    cache_go(bdata->cache, bdata->pool, req, &bdata->n_get, &bdata->n_set,
             &bdata->n_del, &bdata->n_get_miss);
    read_trace(reader, req);
  }

  if (warmup_time > 0) printf("warmup finish trace %d sec\n", req->timestamp);
  gettimeofday(&bdata->start_time, NULL);

  while (read_trace(reader, req) == 0) {
    cache_go(bdata->cache, bdata->pool, req, &bdata->n_get, &bdata->n_set,
             &bdata->n_del, &bdata->n_get_miss);

    if (req->timestamp >= next_report_trace_ts) {
      next_report_trace_ts += opts->report_interval;
      bdata->trace_time = req->timestamp;
      report_bench_result(bdata, opts);
    }
  }

  bdata->trace_time = req->timestamp;
  gettimeofday(&bdata->end_time, nullptr);

  close_trace(reader);
  free_request(req);
}

void report_bench_result(struct bench_data *bdata, bench_opts_t *opts) {
  int64_t n_req = bdata->n_get + bdata->n_set + bdata->n_del;
  double write_ratio = (double)bdata->n_set / n_req;
  double miss_ratio = (double)bdata->n_get_miss / bdata->n_get;
  double del_ratio = (double)bdata->n_del / n_req;
  gettimeofday(&bdata->end_time, nullptr);
  double runtime =
      (bdata->end_time.tv_sec - bdata->start_time.tv_sec) * 1000000.0 +
      (bdata->end_time.tv_usec - bdata->start_time.tv_usec);
  double throughput = (double)n_req / runtime;
  printf(
      "cachelib %s %ld MiB, %s, "
      "%.2lf hour, runtime %.2lf sec, %ld requests, throughput "
      "%.2lf MQPS, miss ratio %.4lf\n",
      // "utilization %.4lf, "
      // "write ratio %.4lf, del ratio %.4lf\n",
      typeid(bdata->cache).name(), opts->cache_size_in_mb, opts->trace_path,
      (double)bdata->trace_time / 3600.0, runtime / 1.0e6, bdata->n_get,
      throughput, miss_ratio);
}
