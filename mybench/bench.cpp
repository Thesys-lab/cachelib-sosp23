
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

void *trace_replay_run(struct bench_data *bench_data) {
  struct request *req = new_request();
  gettimeofday(&bench_data->start_time, NULL);

  int status;
  read_trace(bench_data->reader, req);
  int32_t trace_start_ts = req->timestamp;
  int32_t next_report_trace_ts =
      bench_data->report_interval > 0
          ? trace_start_ts + bench_data->report_interval
          : INT32_MAX;

  bool has_warmup = false;
  int warmup_time = 86400 * 0;

  while (read_trace(bench_data->reader, req) == 0) {
    if ((!has_warmup) && req->timestamp > warmup_time) {
      bench_data->n_get = 0;
      bench_data->n_set = 0;
      bench_data->n_get_miss = 0;
      bench_data->n_del = 0;
      has_warmup = true;
      gettimeofday(&bench_data->start_time, NULL);
      printf("warmup finish trace %d sec\n", req->timestamp);
    }

    switch (req->op) {
      case op_get:
        bench_data->n_get++;
        status = cache_get(bench_data->cache, bench_data->pool, req);
        // printf("%d  ", status);
        // print_req(req);
        if (status == 1) {
          bench_data->n_get_miss++;
          bench_data->n_set++;
          status = cache_set(bench_data->cache, bench_data->pool, req);
        }
        break;
      case op_set:
        bench_data->n_set++;
        status = cache_set(bench_data->cache, bench_data->pool, req);
        break;
      case op_del:
        status = cache_del(bench_data->cache, bench_data->pool, req);
        bench_data->n_del++;
        break;
      case op_ignore:
        break;
      default:;
        printf("op not supported %d\n", req->op);
        assert(false);
    }

    // print_req(req);
    if (req->timestamp >= next_report_trace_ts) {
      next_report_trace_ts += bench_data->report_interval;
      bench_data->trace_time = req->timestamp;
      report_bench_result(bench_data);
    }
  }

  bench_data->trace_time = req->timestamp;
  gettimeofday(&bench_data->end_time, nullptr);
  return bench_data;
}

void benchmark_destroy(struct bench_data *bench_data) {
  close_trace(bench_data->reader);
}

void report_bench_result(struct bench_data *bench_data) {
  int64_t n_req = bench_data->n_get + bench_data->n_set + bench_data->n_del;
  double write_ratio = (double)bench_data->n_set / n_req;
  double miss_ratio = (double)bench_data->n_get_miss / bench_data->n_get;
  double del_ratio = (double)bench_data->n_del / n_req;
  gettimeofday(&bench_data->end_time, nullptr);
  double runtime =
      (bench_data->end_time.tv_sec - bench_data->start_time.tv_sec) *
          1000000.0 +
      (bench_data->end_time.tv_usec - bench_data->start_time.tv_usec);
  double throughput = (double)n_req / runtime;
  printf(
      "cachelib %s %d MiB, %s, "
      "%.2lf hour, runtime %.2lf sec, %ld requests, throughput "
      "%.2lf MQPS, miss ratio %.4lf\n",
      // "utilization %.4lf, "
      // "write ratio %.4lf, del ratio %.4lf\n",
      typeid(bench_data->cache).name(), bench_data->cache_size_in_mb,
      bench_data->reader->trace_path, (double)bench_data->trace_time / 3600.0,
      runtime / 1.0e6, bench_data->n_get, throughput, miss_ratio);
}
