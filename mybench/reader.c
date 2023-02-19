
/**
 *  a reader for reading requests from trace
 */

#include "reader.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

#define DEBUG_MODE

char val_array[MAX_VAL_LEN];

int read_generalNS_trace(struct reader* reader, struct request *req); 

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
                          const bool nottl) {
  int fd;
  struct stat st;
  struct reader *reader = malloc(sizeof(struct reader));
  memset(reader, 0, sizeof(struct reader));
  reader->nottl = nottl; 

  /* init reader module */
  for (int i = 0; i < MAX_VAL_LEN; i++)
    val_array[i] = (char)('A' + i % 26);

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

  switch (trace_type) {
  case oracleSysTwrNS:
    reader->record_size = 34;
    break;
  case oracleCF1:
    reader->record_size = 49;
    break;
  case oracleAkamai:
    reader->record_size = 100000000;
    break;
  case oracleGeneral:
    reader->record_size = 24;
    break;
  case generalNS:
    reader->record_size = 18;
    break;
  default:
    printf("unknown trace_type %d\n", trace_type);
    exit(1);
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

// __attribute__((unused)) static void process_ttl_jitter(struct reader *reader) {
//   if (reader->e->ttl > 500 && reader->e->ttl <= 3000) {
//     reader->e->ttl = 600;
//   } else if (reader->e->ttl > 3000 && reader->e->ttl <= 7200) {
//     reader->e->ttl = 3600;
//   } else if (reader->e->ttl > 7200 && reader->e->ttl <= 88000) {
//     reader->e->ttl = 86400;
//   } else if (reader->e->ttl > 88000 && reader->e->ttl <= 86400 * 15) {
//     reader->e->ttl = 86400 * 14;
//   } else {
//     reader->e->ttl = 86400 * 30;
//   }
// }

int read_trace(struct reader *reader, struct request *req) {
  if (reader->offset >= reader->file_size) {
    return 1;
  }

  static int n_read = 0; 
  int status;

  if (reader->trace_type == oracleSysTwrNS) {
    status = read_oracleSysTwrNS_trace(reader, req);
  } else if (reader->trace_type == oracleCF1) {
    status = read_oracleCF1_trace(reader, req); 
  } else if (reader->trace_type == oracleGeneral) {
    status = read_oracleGeneral_trace(reader, req); 
  }else if (reader->trace_type == generalNS) {
    status = read_generalNS_trace(reader, req); 
  } else {
    printf("unknown trace type when reading trace %d\n", reader->trace_type); 
    abort();
  }

  if (req->ttl == 0) {
    // req->ttl = reader->default_ttls[reader->default_ttl_idx];
    // reader->default_ttl_idx = (reader->default_ttl_idx + 1) % 100;
    req->ttl = 86400; 
  }

  /* it is possible we have overflow here, but it should be rare */
  // unsigned long key = *(uint64_t *)req->key;
  // sprintf(req->key, "%.*lu", req->key_len, key);

  if (reader->trace_start_ts == -1) {
    reader->trace_start_ts = req->timestamp; 
  }

  if (req->timestamp < reader->trace_start_ts) {
    if (reader->trace_type == oracleCF1 && req->timestamp > 700000 && req->timestamp < 800000) {
      reader->trace_start_ts = 0;     
    } else {
      printf("time err trace_start %d, current time %d\n", reader->trace_start_ts, req->timestamp); 
      abort();
    }
  }

  req->timestamp = req->timestamp - reader->trace_start_ts + 1;

  if (reader->nottl) {
    req->ttl = 20000000; 
  }

  // if (n_read++ == 0)
  //   print_req(req); 
  reader->offset += reader->record_size;
  return status;
}

int read_oracleSysTwrNS_trace(struct reader *reader, struct request *req) {
  char *record = reader->mmap + reader->offset;
  req->timestamp = *(uint32_t *)record + 1;
  *(uint64_t *)req->key = *(uint64_t *)(record + 4);
  req->key_len = *(uint16_t *)(record + 12);
  // if (req->key_len < 8) 
  //   req->key_len = 8; 
  req->val_len = *(uint32_t *)(record + 14);

  switch (*(uint16_t *)(record + 18)) {
  case 1:
  case 2:
  case 4:
  case 5:
  case 7:
  case 8:
  case 10:
  case 11:
    req->op = op_get;
    break;
  case 3:
  case 6:
    req->op = op_set;
    break;
  // case 7:
  // case 8:
  //   req->op = op_ignore; 
  //   break;
  case 9:
    req->op = op_del;
    break;
  default:
    printf("unsupported request op %d\n", (int)(*(uint16_t *)(record + 18))); 
    req->op = op_invalid;
    break;
  }
  // req->ns = *(uint16_t *) (record + 20);
  req->ttl = *(int32_t *)(record + 22);
  // req->next_access_vtime = *(int64_t *) (record + 26);
  // req->ttl = 2000000; 

  if (req->val_len > 1048000) {
    req->op = op_ignore; 
  } 

  return 0;
}

int read_oracleCF1_trace(struct reader* reader, struct request *req) {
  char *record = reader->mmap + reader->offset;
  req->timestamp = *(uint32_t *)record + 1;
  *(uint64_t *)req->key = *(uint64_t *)(record + 4);
  req->key_len = 8;
  req->val_len = *(uint64_t *)(record + 12);
  // scale down 
  req->val_len /= 10;
  req->op = op_get;
  req->ttl = *(int32_t *)(record + 20);


  if (req->val_len > 1048000 * 4) {
    req->val_len = 1048000 * 4;
    // req->op = op_ignore; 
  } 

    return 0; 
}

int read_oracleAkamai_trace(struct reader *reader, struct request *req) {

    return 0; 
}

int read_oracleGeneral_trace(struct reader *reader, struct request *req) {
  char *record = reader->mmap + reader->offset;
  req->timestamp = *(uint32_t *)record + 1;
  *(uint64_t *)req->key = *(uint64_t *)(record + 4);
  req->key_len = 8;
  req->val_len = *(uint64_t *)(record + 12);
  if (req->val_len > 1048500)
      req->val_len = 1048500; 
  req->op = op_get;
  req->ttl = 2000000;

  return 0; 
}

int read_generalNS_trace(struct reader* reader, struct request *req) {
  char *record = reader->mmap + reader->offset;
  req->timestamp = *(uint32_t *)record + 1;
  *(uint64_t *)req->key = *(uint64_t *)(record + 4);
  req->key_len = 8;
  req->val_len = *(uint64_t *)(record + 12);
  req->op = op_get;
  req->ttl = 2000000;
  // req->ns = *(uint16_t*)(record + 16);

  req->val_len /= 32; 

    return 0; 
}

void close_trace(struct reader *reader) {
  munmap(reader->mmap, reader->file_size);

  free(reader);
}

