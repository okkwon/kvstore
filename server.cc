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
#include <grpcpp/support/status.h>

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "keyvaluestore.grpc.pb.h"

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

// key value
std::unordered_map<std::string, std::string> kv_map;

std::string get_value_from_map(const std::string& key) {
  if (kv_map.count(key))
    return kv_map[key];
  else
    return "";
}

// Logic and data behind the server's behavior.
class KeyValueStoreServiceImpl final : public KeyValueStore::Service {
  Status GetValue(ServerContext* context, const GetValueRequest* request,
                  GetValueResponse* response) override {
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
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  KeyValueStoreServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case, it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}
