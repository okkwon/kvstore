// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <memory>
#include <string>

class KeyValueStoreServiceImpl;

struct KeyValueStoreServerOptions {
  std::chrono::milliseconds timeout_in_ms = std::chrono::milliseconds(3000);
};

class KeyValueStoreServer {
 public:
  explicit KeyValueStoreServer(const std::string& addr,
                               const KeyValueStoreServerOptions& options);
  KeyValueStoreServer(const KeyValueStoreServer&) = delete;
  KeyValueStoreServer(KeyValueStoreServer&&) = delete;
  KeyValueStoreServer& operator=(const KeyValueStoreServer&) = delete;
  KeyValueStoreServer&& operator=(KeyValueStoreServer&&) = delete;

  ~KeyValueStoreServer();

  void Wait();

 private:
  // Need to keep this service during the server's lifetime.
  std::unique_ptr<KeyValueStoreServiceImpl> service_impl_;
  std::unique_ptr<::grpc::Server> server_;
};
