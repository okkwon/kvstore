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
