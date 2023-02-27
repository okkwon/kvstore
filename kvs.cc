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
using grpc::Status;
using keyvaluestore::GetValueRequest;
using keyvaluestore::GetValueResponse;
using keyvaluestore::KeyValueStore;
using keyvaluestore::SetValueRequest;
using keyvaluestore::SetValueResponse;

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

static KeyValueStoreClient* CastToKeyValueStoreClient(kvs_client_t* kvs) {
  return (KeyValueStoreClient*)(kvs);
}

static kvs_client_t* CastToKVS(KeyValueStoreClient* client) {
  return (kvs_client_t*)(client);
}

extern "C" {

kvsStatus_t kvs_client_create(kvs_client_t** kvs_client, const char* addr,
                              kvsConfig_t* config) {
  *kvs_client = nullptr;

  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  auto channel = grpc::CreateChannel("localhost:50051",
                                     grpc::InsecureChannelCredentials());

  KeyValueStoreClient* client = new KeyValueStoreClient(channel);
  if (!client) {
    return kvsStatusInternalError;
  }

  *kvs_client = CastToKVS(client);
  return kvsStatusOK;
}

kvsStatus_t kvs_client_destroy(kvs_client_t** kvs_client) {
  if (*kvs_client) {
    KeyValueStoreClient* client = CastToKeyValueStoreClient(*kvs_client);
    delete client;
    *kvs_client = nullptr;
  }
  return kvsStatusOK;
}

kvsStatus_t kvs_client_get(kvs_client_t* kvs_client, const char* key,
                           char* value, int n) {
  std::string v;
  KeyValueStoreClient* client = CastToKeyValueStoreClient(kvs_client);
  Status status = client->GetValue(key, v);
  if (status.ok()) {
    strncpy(value, v.c_str(), n);
    return kvsStatusOK;
  } else {
    // TODO(okkwon): add more error types to give more useful info.
    return kvsStatusInvalidUsage;
  }
}

kvsStatus_t kvs_client_set(kvs_client_t* kvs_client, const char* key,
                           const char* val) {
  KeyValueStoreClient* client = CastToKeyValueStoreClient(kvs_client);
  Status status = client->SetValue(key, val);
  if (status.ok()) {
    return kvsStatusOK;
  } else {
    if (status.error_code() == grpc::StatusCode::ALREADY_EXISTS) {
      return kvsStatusInvalidUsage;
    } else {
      // TODO(okkwon): add more error types to give more useful info.
      return kvsStatusInternalError;
    }
  }
  return kvsStatusOK;
}

}  // extern "C"
