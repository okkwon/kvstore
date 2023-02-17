#ifndef KVS_H
#define KVS_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef enum {
  kvsStatusOK = 0,
  kvsStatusTimeOut = 1,
  kvsStatusInvalidArgument = 2,
  kvsStatusInternalError = 3,
  kvsStatusInProgress = 4,
  kvsStatusInvalidUsage = 5,
  kvsStatusSystemError = 6,
  kvsStatusServerError = 7,
  kvsStatusConnectionError = 8,
  kvsStatusNum = 9,
} kvsStatus_t;

typedef struct {
  long long connection_timeout_ms; /* timeout for connection */
  long long timeout_ms;            /* timeout for kvs_get and kvs_set */
} kvsConfig_t;

/* This is a C wrapper for the kvs class. */
typedef struct kvs_t kvs_t;

kvsStatus_t kvs_create(kvs_t** store, const char* addr, kvsConfig_t* config);

kvsStatus_t kvs_destroy(kvs_t** store);

kvsStatus_t kvs_get(kvs_t* store, const char* key);

kvsStatus_t kvs_set(kvs_t* store, const char* key, const char* val);

#ifdef __cplusplus
}  // end extern "C"
#endif  // __cplusplus

#endif  // KVS_H
