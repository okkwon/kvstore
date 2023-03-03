// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "server.h"

#include <grpcpp/grpcpp.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "keyvaluestore.grpc.pb.h"

// Logic and data behind the server's behavior.
class KeyValueStoreServiceImpl final
    : public keyvaluestore::KeyValueStore::Service {
 public:
  explicit KeyValueStoreServiceImpl(const KeyValueStoreServerOptions& options)
      : options_(options) {}
  KeyValueStoreServiceImpl(const KeyValueStoreServiceImpl&) = delete;
  KeyValueStoreServiceImpl(KeyValueStoreServiceImpl&&) = delete;
  KeyValueStoreServiceImpl& operator=(const KeyValueStoreServiceImpl&) = delete;
  KeyValueStoreServiceImpl&& operator=(KeyValueStoreServiceImpl&&) = delete;

  grpc::Status GetValue(grpc::ServerContext* context,
                        const keyvaluestore::GetValueRequest* request,
                        keyvaluestore::GetValueResponse* response) override {
    std::unique_lock<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    // Wake up every 1ms to check a value is set by a client.
    // TODO(okkwon): make it an option.
    auto wakeup_resolution = std::chrono::milliseconds(1);
    auto deadline = now + options_.timeout_in_ms;
    while (1) {
      if (cv_.wait_for(lock, wakeup_resolution,
                       [&]() { return kv_map_.count(request->key()); })) {
        response->set_value(kv_map_[request->key()]);
        return grpc::Status::OK;
      } else {
        if (std::chrono::system_clock::now() >= deadline) {
          return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED,
                              "GetValue() exceeded time limit.");
        }
      }
    }
  }

  grpc::Status SetValue(grpc::ServerContext* context,
                        const keyvaluestore::SetValueRequest* request,
                        keyvaluestore::SetValueResponse* response) override {
    if (kv_map_.count(request->key())) {
      // We expect only one client sets a value with a key only once.
      return grpc::Status(grpc::StatusCode::ALREADY_EXISTS,
                          "Updating an existing value is not supported");
    }
    kv_map_[request->key()] = request->value();
    return grpc::Status::OK;
  }

 private:
  std::unordered_map<std::string, std::string> kv_map_;
  KeyValueStoreServerOptions options_;
  std::condition_variable cv_;
  std::mutex mutex_;
};

KeyValueStoreServer::KeyValueStoreServer(
    const std::string& addr, const KeyValueStoreServerOptions& options) {
  grpc::ServerBuilder builder;
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

KeyValueStoreServer::~KeyValueStoreServer() {
  server_->Shutdown();
  server_->Wait();
  // The service impl must be freed up first before the server.
  service_impl_ = nullptr;
  server_ = nullptr;
}

void KeyValueStoreServer::Wait() { server_->Wait(); }
