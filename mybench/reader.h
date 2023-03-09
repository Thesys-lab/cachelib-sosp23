#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "request.h"

#define MAX_TRACE_PATH_LEN 1024

enum trace_type {
  oracleGeneral,
};

struct reader {
  char *mmap;
  int32_t reader_id;
  int trace_start_ts;
  size_t offset;
  size_t file_size;
  char trace_path[MAX_TRACE_PATH_LEN];
  const int32_t *default_ttls;
  int default_ttl_idx;
  int64_t n_trace_req;

  enum trace_type trace_type;
  int record_size; /* the size the trace uses to store a request entry */
};

struct reader *open_trace(const char *trace_path,
                          const enum trace_type trace_type,
                          const int32_t reader_id = 0);

int read_trace(struct reader *reader, struct request *req);

void close_trace(struct reader *reader);

/*
 * read one request from trace and store in request
 *
 *
 * return 1 on trace EOF, otherwise 0
 *
 */
int read_oracleGeneral_trace(struct reader *reader, struct request *req);

#ifdef __cplusplus
}
#endif
