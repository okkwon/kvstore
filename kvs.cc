// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kvs.h"

#include <iostream>

#include "client.h"
#include "server.h"

static KeyValueStoreClient* CastToKeyValueStoreClient(
    kvs_client_t* kvs_client) {
  return (KeyValueStoreClient*)(kvs_client);
}

static kvs_client_t* CastToKVSClient(KeyValueStoreClient* client) {
  return (kvs_client_t*)(client);
}

static KeyValueStoreServer* CastToKeyValueStoreServer(
    kvs_server_t* kvs_server) {
  return (KeyValueStoreServer*)(kvs_server);
}

static kvs_server_t* CastToKVSServer(KeyValueStoreServer* server) {
  return (kvs_server_t*)(server);
}

extern "C" {

//------------------------------------------------------------------------------
// Client C API
//------------------------------------------------------------------------------

kvs_status_t kvs_client_create(kvs_client_t** kvs_client, const char* addr,
                               kvs_client_config_t* config) {
  *kvs_client = nullptr;

  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint. We indicate
  // that the channel isn't authenticated (use of InsecureChannelCredentials()).
  auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());

  KeyValueStoreClient* client = new KeyValueStoreClient(channel);
  if (!client) {
    return KVS_STATUS_INTERNAL_ERROR;
  }

  *kvs_client = CastToKVSClient(client);
  return KVS_STATUS_OK;
}

kvs_status_t kvs_client_destroy(kvs_client_t** kvs_client) {
  if (*kvs_client) {
    KeyValueStoreClient* client = CastToKeyValueStoreClient(*kvs_client);
    delete client;
    *kvs_client = nullptr;
  }
  return KVS_STATUS_OK;
}

kvs_status_t kvs_client_get(kvs_client_t* kvs_client, const char* key,
                            int key_len, char* value, int value_len) {
  std::string key_str(key, key_len), v;
  KeyValueStoreClient* client = CastToKeyValueStoreClient(kvs_client);
  grpc::Status status = client->GetValue(key_str, v);
  if (status.ok()) {
    memcpy(value, v.data(), value_len);
    return KVS_STATUS_OK;
  } else {
    printf("grpc::StatusCode = %d\n", status.error_code());
    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
      return KVS_STATUS_DEADLINE_EXCEEDED;
    } else {
      // TODO(okkwon): handle error in a more detailed way.
      return KVS_STATUS_INTERNAL_ERROR;
    }
  }
}

kvs_status_t kvs_client_set(kvs_client_t* kvs_client, const char* key,
                            int key_len, const char* value, int value_len) {
  KeyValueStoreClient* client = CastToKeyValueStoreClient(kvs_client);
  std::string key_str(key, key_len), value_str(value, value_len);
  grpc::Status status = client->SetValue(key_str, value_str);
  if (status.ok()) {
    return KVS_STATUS_OK;
  } else {
    if (status.error_code() == grpc::StatusCode::ALREADY_EXISTS) {
      return KVS_STATUS_INVALID_USAGE;
    } else {
      // TODO(okkwon): add more error types to give more useful info.
      return KVS_STATUS_INTERNAL_ERROR;
    }
  }
  return KVS_STATUS_OK;
}

//------------------------------------------------------------------------------
// Server C API
//------------------------------------------------------------------------------

kvs_status_t kvs_server_create(kvs_server_t** kvs_server, const char* addr,
                               kvs_server_config_t* config) {
  if (kvs_server == nullptr || addr == nullptr || config == nullptr) {
    return KVS_STATUS_INVALID_ARGUMENT;
  }

  *kvs_server = nullptr;
  KeyValueStoreServerOptions options = {
      .timeout_in_ms = std::chrono::milliseconds(config->timeout_ms)};
  KeyValueStoreServer* server = new KeyValueStoreServer(addr, options);
  if (!server) {
    return KVS_STATUS_INTERNAL_ERROR;
  }
  *kvs_server = CastToKVSServer(server);
  return KVS_STATUS_OK;
}

kvs_status_t kvs_server_wait(kvs_server_t* kvs_server) {
  KeyValueStoreServer* server = CastToKeyValueStoreServer(kvs_server);
  server->Wait();
  return KVS_STATUS_OK;
}

kvs_status_t kvs_server_destroy(kvs_server_t** kvs_server) {
  if (kvs_server == nullptr) {
    return KVS_STATUS_INVALID_ARGUMENT;
  }

  if (*kvs_server) {
    KeyValueStoreServer* server = CastToKeyValueStoreServer(*kvs_server);
    delete server;
    *kvs_server = nullptr;
  }
  return KVS_STATUS_OK;
}

}  // extern "C"
