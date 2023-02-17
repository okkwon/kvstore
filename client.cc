/*
 *
 * Copyright 2018 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
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
  void GetValue(std::string key, std::chrono::milliseconds timeout_ms =
                                     std::chrono::milliseconds(3000)) {
    std::cout << "GetValue(\"" << key << "\") -> ";

    // Check the cache first.
    if (kv_map.count(key)) {
      std::cout << "\"" << kv_map[key] << "\" (cached)\n";
      return;
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
    Status status = stub_->GetValue(&context, request, &response);

    std::cout << "\"" << response.value() << "\"\n";

    if (!status.ok()) {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "RPC failed";
    }
  }

  // SetValue sets a value for the key. Updating (Setting a value for an
  // existing key) is not supported by the server.
  void SetValue(std::string key, std::string value) {
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
  }

 private:
  std::unique_ptr<KeyValueStore::Stub> stub_;
  // cache for key/value
  std::unordered_map<std::string, std::string> kv_map;
};

int main(int argc, char** argv) {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  auto channel = grpc::CreateChannel("localhost:50051",
                                     grpc::InsecureChannelCredentials());
  KeyValueStoreClient client(channel);
  client.SetValue("key1", "mykey1");
  client.GetValue("key1");
  client.GetValue("key1");

  return 0;
}
