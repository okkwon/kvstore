#ifndef KVS_H
#define KVS_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef enum {
  kvsSuccess = 0,
  kvsTimeOut = 1,
  kvsInvalidArgument = 2,
  kvsInternalError = 3,
  kvsInProgress = 4,
  kvsInvalidUsage = 5,
  kvsSystemError = 6,
  kvsServerError = 7,
  kvsConnectionError = 8,
  kvsNumResults = 9,
} kvsResult_t;

typedef struct {
  long long connection_timeout_ms; /* timeout for connection */
  long long timeout_ms;            /* timeout for kvs_get and kvs_set */
} kvsConfig_t;

/* This is a C wrapper for the kvs class. */
typedef struct kvs_t kvs_t;

kvsResult_t kvs_create(kvs_t** store, const char* addr, int port,
                       kvsConfig_t* config);

kvsResult_t kvs_destroy(kvs_t** store);

kvsResult_t kvs_get(kvs_t* store, const char* key);

kvsResult_t kvs_set(kvs_t* store, const char* key, const char* val);

#ifdef __cplusplus
}  // end extern "C"
#endif  // __cplusplus

#endif  // KVS_H
