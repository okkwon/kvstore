// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kvs.h"

#include <grpcpp/grpcpp.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifdef BAZEL_BUILD
#include "examples/protos/keyvaluestore.grpc.pb.h"
#else
#include "keyvaluestore.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;
using keyvaluestore::GetValueRequest;
using keyvaluestore::GetValueResponse;
using keyvaluestore::KeyValueStore;
using keyvaluestore::SetValueRequest;
using keyvaluestore::SetValueResponse;

//------------------------------------------------------------------------------
// Client Class
//------------------------------------------------------------------------------

class KeyValueStoreClient {
 public:
  KeyValueStoreClient(std::shared_ptr<Channel> channel)
      : stub_(KeyValueStore::NewStub(channel)) {}

  // GetValue gets a value for the requested key.
  Status GetValue(
      std::string key, std::string& value,
      std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(3000)) {
    std::cout << "GetValue(\"" << key << "\") -> ";

    Status status = Status::OK;
    value = "";

    // Check the cache first.
    if (kv_map.count(key)) {
      value = kv_map[key];
      std::cout << "\"" << value << "\" (cached)\n";
      return status;
    }

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    context.set_fail_fast(false);
    std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + timeout_ms;
    context.set_deadline(deadline);

    // Key we are sending to the server.
    GetValueRequest request;
    request.set_key(std::move(key));
    timeout_ms = std::min(timeout_ms,
                          std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::minutes(10)));
    // FIXME: add timeout_ms field to the message and use it to wait for the key
    // to be set by another client.

    GetValueResponse response;
    status = stub_->GetValue(&context, request, &response);
    if (!status.ok()) {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "RPC failed";
    }

    value = response.value();
    std::cout << "\"" << value << "\"\n";

    return status;
  }

  // SetValue sets a value for the key. Updating (Setting a value for an
  // existing key) is not supported by the server.
  Status SetValue(std::string key, std::string value) {
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    std::cout << "SetValue(\"" << key << "\", \"" << value << "\")\n";

    // Put the key and value in the cache.
    kv_map[key] = value;

    SetValueRequest request;
    request.set_key(std::move(key));
    request.set_value(std::move(value));

    SetValueResponse response;
    Status status = stub_->SetValue(&context, request, &response);

    if (!status.ok()) {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "RPC failed";
    }
    return status;
  }

 private:
  std::unique_ptr<KeyValueStore::Stub> stub_;
  // cache for key/value
  std::unordered_map<std::string, std::string> kv_map;
};

static KeyValueStoreClient* CastToKeyValueStoreClient(
    kvs_client_t* kvs_client) {
  return (KeyValueStoreClient*)(kvs_client);
}

static kvs_client_t* CastToKVS(KeyValueStoreClient* client) {
  return (kvs_client_t*)(client);
}

//------------------------------------------------------------------------------
// Server Class
//------------------------------------------------------------------------------

// Logic and data behind the server's behavior.
class KeyValueStoreServiceImpl final : public KeyValueStore::Service {
 public:
  struct Options {
    std::chrono::milliseconds timeout_in_ms = std::chrono::milliseconds(3000);
  };
  Status GetValue(ServerContext* context, const GetValueRequest* request,
                  GetValueResponse* response) override {
    // TODO(okkwon): wait with a timeout.
    response->set_value(get_value_from_map(request->key()));
    return Status::OK;
  }

  Status SetValue(ServerContext* context, const SetValueRequest* request,
                  SetValueResponse* response) override {
    if (kv_map.count(request->key())) {
      // We expect only one client sets a value with a key only once.
      return Status(grpc::StatusCode::ALREADY_EXISTS,
                    "Updating an existing value is not supported");
    }
    kv_map[request->key()] = request->value();
    return Status::OK;
  }

 private:
  std::string get_value_from_map(const std::string& key) {
    if (kv_map.count(key))
      return kv_map[key];
    else
      return "";
  }

  // key value
  std::unordered_map<std::string, std::string> kv_map;
};

class KeyValueStoreServer {
 public:
  explicit KeyValueStoreServer(const std::string& addr) {
    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case, it corresponds to an *synchronous* service.
    builder.RegisterService(&service_);
    // Finally assemble the server.
    server_ = builder.BuildAndStart();
    std::cout << "Server listening on " << addr << std::endl;
  }

  ~KeyValueStoreServer() { server_->Shutdown(); }

  void Wait() { server_->Wait(); }

 private:
  // Need to keep this service during the server's lifetime.
  KeyValueStoreServiceImpl service_;
  std::unique_ptr<::grpc::Server> server_;
};

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
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());

  KeyValueStoreClient* client = new KeyValueStoreClient(channel);
  if (!client) {
    return KVS_STATUS_INTERNAL_ERROR;
  }

  *kvs_client = CastToKVS(client);
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
                            char* value, int n) {
  std::string v;
  KeyValueStoreClient* client = CastToKeyValueStoreClient(kvs_client);
  Status status = client->GetValue(key, v);
  if (status.ok()) {
    strncpy(value, v.c_str(), n);
    return KVS_STATUS_OK;
  } else {
    // TODO(okkwon): add more error types to give more useful info.
    return KVS_STATUS_INVALID_USAGE;
  }
}

kvs_status_t kvs_client_set(kvs_client_t* kvs_client, const char* key,
                            const char* val) {
  KeyValueStoreClient* client = CastToKeyValueStoreClient(kvs_client);
  Status status = client->SetValue(key, val);
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

  KeyValueStoreServer* server = new KeyValueStoreServer(addr);
  if (!server) {
    return KVS_STATUS_INTERNAL_ERROR;
  }
  *kvs_server = CastToKVSServer(server);
  return KVS_STATUS_OK;
}

void kvs_server_wait(kvs_server_t* kvs_server) {
  KeyValueStoreServer* server = CastToKeyValueStoreServer(kvs_server);
  server->Wait();
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
