#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_LEN 24
#define MAX_KEY_LEN 256
#define MAX_VAL_LEN 8 * 1024 * 1024

extern char val_array[MAX_VAL_LEN];

typedef enum {
  op_invalid = 0,
  op_get,
  op_set,
  op_del,
  op_ignore,
} op_e;

static const char *op_names[5] = {"invalid", "get", "set", "delete", "ignore"};

struct request {
  int32_t timestamp;

  char key[MAX_KEY_LEN];
  char *val;
  int32_t key_len;
  int32_t val_len;
  int32_t ttl;
  op_e op;
};

static inline struct request *new_request() {
  struct request *request = (struct request *)malloc(sizeof(struct request));
  memset(request, 0, sizeof(struct request));
  memset(request->key, 0, MAX_KEY_LEN);
  request->val = val_array;

  return request;
}

static inline void print_req(struct request *req) {
  printf("%d %s %s vallen %d ttl %d\n", req->timestamp, op_names[req->op],
         req->key, req->val_len, req->ttl);
}

#ifdef __cplusplus
}
#endif
