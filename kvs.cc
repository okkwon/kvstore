#include "kvs.h"

extern "C" {

kvsResult_t kvs_create(kvs_t** store, const char* addr, int port,
                       kvsConfig_t* config) {
  return kvsSuccess;
}

kvsResult_t kvs_destroy(kvs_t** store) { return kvsSuccess; }

kvsResult_t kvs_get(kvs_t* store, const char* key) { return kvsSuccess; }

kvsResult_t kvs_set(kvs_t* store, const char* key, const char* val) {
  return kvsSuccess;
}

}  // extern "C"
