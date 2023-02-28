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
