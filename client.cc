// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>

#include "kvs.h"

int main(int argc, char** argv) {
  kvs_client_t* kvs_client;

  kvs_status_t result;
  kvs_client_config_t config = {3000, 3000};

  result = kvs_client_create(&kvs_client, "localhost:50051", &config);
  if (result != KVS_STATUS_OK) {
    std::cout << "Creation failed\n";
    exit(1);
  }

  char value[128];

  kvs_client_set(kvs_client, "key1", "mykey1");
  kvs_client_get(kvs_client, "key1", value, sizeof(value));
  printf("%s\n", value);
  kvs_client_get(kvs_client, "key1", value, sizeof(value));
  printf("%s\n", value);

  kvs_client_destroy(&kvs_client);
  return 0;
}
