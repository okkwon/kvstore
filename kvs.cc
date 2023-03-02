// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kvs.h"

#include <grpcpp/grpcpp.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
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
    // std::chrono::system_clock::time_point deadline =
    //     std::chrono::system_clock::now() + timeout_ms;
    // context.set_deadline(deadline);

    // Key we are sending to the server.
    GetValueRequest request;
    request.set_key(std::move(key));
    // timeout_ms = std::min(timeout_ms,
    //                       std::chrono::duration_cast<std::chrono::milliseconds>(
    //                           std::chrono::minutes(10)));
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
  explicit KeyValueStoreServiceImpl(const Options& options)
      : options_(options) {}
  KeyValueStoreServiceImpl(const KeyValueStoreServiceImpl&) = delete;
  KeyValueStoreServiceImpl(KeyValueStoreServiceImpl&&) = delete;
  KeyValueStoreServiceImpl& operator=(const KeyValueStoreServiceImpl&) = delete;
  KeyValueStoreServiceImpl&& operator=(KeyValueStoreServiceImpl&&) = delete;

  Status GetValue(ServerContext* context, const GetValueRequest* request,
                  GetValueResponse* response) override {
    std::unique_lock<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    if (cv_.wait_until(lock, now + options_.timeout_in_ms,
                       [&]() { return kv_map_.count(request->key()); })) {
      response->set_value(get_value_from_map(request->key()));
      return Status::OK;
    } else {
      return Status(grpc::StatusCode::DEADLINE_EXCEEDED,
                    "GetValue() exceeded time limit.");
    }
  }

  Status SetValue(ServerContext* context, const SetValueRequest* request,
                  SetValueResponse* response) override {
    if (kv_map_.count(request->key())) {
      // We expect only one client sets a value with a key only once.
      return Status(grpc::StatusCode::ALREADY_EXISTS,
                    "Updating an existing value is not supported");
    }
    kv_map_[request->key()] = request->value();
    return Status::OK;
  }

 private:
  std::string get_value_from_map(const std::string& key) {
    if (kv_map_.count(key))
      return kv_map_[key];
    else
      return "";
  }

  // key value
  std::unordered_map<std::string, std::string> kv_map_;
  Options options_;
  std::condition_variable cv_;
  std::mutex mutex_;
};

class KeyValueStoreServer {
 public:
  explicit KeyValueStoreServer(
      const std::string& addr,
      const KeyValueStoreServiceImpl::Options& options) {
    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case, it corresponds to an *synchronous* service.
    service_impl_ = std::make_unique<KeyValueStoreServiceImpl>(options);
    builder.RegisterService(service_impl_.get());
    // Finally assemble the server.
    server_ = builder.BuildAndStart();
    std::cout << "Server listening on " << addr << std::endl;
  }

  ~KeyValueStoreServer() {
    server_->Shutdown();
    server_->Wait();
    // The service impl must be freed up first before the server.
    service_impl_ = nullptr;
    server_ = nullptr;
  }

  void Wait() { server_->Wait(); }

 private:
  // Need to keep this service during the server's lifetime.
  std::unique_ptr<KeyValueStoreServiceImpl> service_impl_;
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
                            int key_len, char* value, int value_len) {
  std::string key_str(key, key_len), v;
  KeyValueStoreClient* client = CastToKeyValueStoreClient(kvs_client);
  Status status = client->GetValue(key_str, v);
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
  Status status = client->SetValue(key_str, value_str);
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
  KeyValueStoreServiceImpl::Options options = {
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
