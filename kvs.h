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
typedef struct kvs_client_t kvs_client_t;

kvsStatus_t kvs_client_create(kvs_client_t** kvs_client, const char* addr,
                              kvsConfig_t* config);

kvsStatus_t kvs_client_destroy(kvs_client_t** kvs_client);

kvsStatus_t kvs_client_get(kvs_client_t* kvs_client, const char* key,
                           char* value, int n);

kvsStatus_t kvs_client_set(kvs_client_t* kvs_client, const char* key,
                           const char* val);

#ifdef __cplusplus
}  // end extern "C"
#endif  // __cplusplus

#endif  // KVS_H
