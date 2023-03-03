// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

#include "keyvaluestore.grpc.pb.h"

//------------------------------------------------------------------------------
// Client Class
//------------------------------------------------------------------------------

class KeyValueStoreClient {
 public:
  explicit KeyValueStoreClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(keyvaluestore::KeyValueStore::NewStub(channel)) {}
  KeyValueStoreClient(const KeyValueStoreClient&) = delete;
  KeyValueStoreClient(KeyValueStoreClient&&) = delete;
  KeyValueStoreClient& operator=(const KeyValueStoreClient&) = delete;
  KeyValueStoreClient&& operator=(KeyValueStoreClient&&) = delete;

  // GetValue gets a value for the requested key.
  grpc::Status GetValue(
      std::string key, std::string& value,
      std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(3000));

  // SetValue sets a value for the key. Updating (Setting a value for an
  // existing key) is not supported by the server.
  grpc::Status SetValue(std::string key, std::string value);

 private:
  std::unique_ptr<keyvaluestore::KeyValueStore::Stub> stub_;
  // cache for key/value
  std::unordered_map<std::string, std::string> kv_map;
};
