
/**
 *  a reader for reading requests from trace
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

#include <string>

#include "reader.h"

#define DEBUG_MODE

char val_array[MAX_VAL_LEN];

/**
 * default ttl is an array of 100 elements, if single ttl then the array is
 * repeat of single element, if multiple TTLs with different weight, it is
 * reflected in the array
 *
 * @param trace_path
 * @param default_ttls
 * @return
 */
struct reader *open_trace(const char *trace_path,
                          const enum trace_type trace_type,
                          const int reader_id) {
  int fd;
  struct stat st;
  struct reader *reader =
      reinterpret_cast<struct reader *>(malloc(sizeof(struct reader)));
  memset(reader, 0, sizeof(struct reader));
  reader->reader_id = reader_id;

  /* init reader module */
  for (int i = 0; i < MAX_VAL_LEN; i++) val_array[i] = (char)('A' + i % 26);

  // reader->default_ttls = default_ttls;
  reader->default_ttl_idx = 0;
  strncpy(reader->trace_path, trace_path, MAX_TRACE_PATH_LEN);

  /* get trace file info */
  if ((fd = open(trace_path, O_RDONLY)) < 0) {
    printf("Unable to open '%s', %s\n", trace_path, strerror(errno));
    exit(1);
  }

  if ((fstat(fd, &st)) < 0) {
    close(fd);
    printf("Unable to fstat '%s', %s\n", trace_path, strerror(errno));
    exit(1);
  }
  reader->file_size = st.st_size;

  /* set up mmap region */
  reader->mmap = (char *)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

  if ((reader->mmap) == MAP_FAILED) {
    close(fd);
    printf("Unable to allocate %zu bytes of memory, %s\n", st.st_size,
           strerror(errno));
    exit(1);
  }

  reader->trace_start_ts = -1;
  reader->trace_type = trace_type;
  if (trace_type == oracleGeneral) {
    reader->record_size = 24;
  } else {
    throw "unknown trace type " + std::to_string(trace_type);
  }

  if (reader->file_size % reader->record_size != 0) {
    printf("trace file size %zu is not multiple of item size %u\n",
           reader->file_size, reader->record_size);
  }

  reader->n_trace_req = reader->file_size / reader->record_size;
  // printf("trace request item size %u - %ld requests\n", reader->record_size,
  //        reader->n_trace_req);

  close(fd);
  return reader;
}

int read_trace(struct reader *reader, struct request *req) {
  if (reader->offset >= reader->file_size) {
    return 1;
  }

  static int n_read = 0;
  int status = read_oracleGeneral_trace(reader, req);

  if (req->ttl == 0) {
    req->ttl = 86400;
  }

  /* it is possible we have overflow here, but it should be rare */
  // unsigned long key = *(uint64_t *)req->key;
  // sprintf(req->key, "%.*lu", req->key_len, key);

  if (reader->trace_start_ts == -1) {
    reader->trace_start_ts = req->timestamp;
    // printf("reader %d first req ts %d, obj %ld, size %d\n", reader->reader_id,
    //        req->timestamp, *(uint64_t *)req->key, req->val_len);
  }

  req->timestamp = req->timestamp - reader->trace_start_ts + 1;

  // if (n_read++ == 0)
  //   print_req(req);
  reader->offset += reader->record_size;
  return status;
}

int read_oracleGeneral_trace(struct reader *reader, struct request *req) {
  char *record = reader->mmap + reader->offset;
  req->timestamp = *(uint32_t *)record + 1;

  uint64_t obj_id = *(uint64_t *)(record + 4);
  // used to make sure each reader has different keys
  obj_id = obj_id % (uint64_t)UINT32_MAX + reader->reader_id * 1000000000ULL;
  *(uint64_t *)req->key = obj_id;

  req->key_len = 8;
  req->val_len = *(uint64_t *)(record + 12);
  if (req->val_len > 1048500) req->val_len = 1048500;
  req->op = op_get;
  req->ttl = 2000000;

  return 0;
}

void close_trace(struct reader *reader) {
  munmap(reader->mmap, reader->file_size);

  free(reader);
}
