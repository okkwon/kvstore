// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "client.h"

#include <iostream>

// GetValue gets a value for the requested key.
grpc::Status KeyValueStoreClient::GetValue(
    std::string key, std::string& value, std::chrono::milliseconds timeout_ms) {
  std::cout << "GetValue(\"" << key << "\") -> ";

  grpc::Status status = grpc::Status::OK;
  value = "";

  // Check the cache first.
  if (kv_map.count(key)) {
    value = kv_map[key];
    std::cout << "\"" << value << "\" (cached)\n";
    return status;
  }

  // Context for the client. It could be used to convey extra information to
  // the server and/or tweak certain RPC behaviors.
  grpc::ClientContext context;

  context.set_fail_fast(false);
  // std::chrono::system_clock::time_point deadline =
  //     std::chrono::system_clock::now() + timeout_ms;
  // context.set_deadline(deadline);

  // Key we are sending to the server.
  keyvaluestore::GetValueRequest request;
  request.set_key(std::move(key));
  // timeout_ms = std::min(timeout_ms,
  //                       std::chrono::duration_cast<std::chrono::milliseconds>(
  //                           std::chrono::minutes(10)));
  // FIXME: add timeout_ms field to the message and use it to wait for the key
  // to be set by another client.

  keyvaluestore::GetValueResponse response;
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
grpc::Status KeyValueStoreClient::SetValue(std::string key, std::string value) {
  // Context for the client. It could be used to convey extra information to
  // the server and/or tweak certain RPC behaviors.
  grpc::ClientContext context;

  std::cout << "SetValue(\"" << key << "\", \"" << value << "\")\n";

  // Put the key and value in the cache.
  kv_map[key] = value;

  keyvaluestore::SetValueRequest request;
  request.set_key(std::move(key));
  request.set_value(std::move(value));

  keyvaluestore::SetValueResponse response;
  grpc::Status status = stub_->SetValue(&context, request, &response);

  if (!status.ok()) {
    std::cout << status.error_code() << ": " << status.error_message()
              << std::endl;
    std::cout << "RPC failed";
  }
  return status;
}
