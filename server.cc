// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kvs.h"

void RunServer() {
  kvs_server_t *kvs_server = nullptr;
  kvs_server_create(&kvs_server, "localhost:50051");
  kvs_server_wait(kvs_server);
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}
