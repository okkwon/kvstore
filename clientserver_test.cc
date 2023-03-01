// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>
#include <gtest/gtest.h>

#include "keyvaluestore.grpc.pb.h"
#include "kvs.h"

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

namespace iree {
namespace {

class ClientServerTest : public ::testing::Test {
 public:
  void StartService(int num_nodes, const std::string& server_addr) {
    kvs_server_ = nullptr;
    kvs_server_config_t config = {.timeout_ms = 100};

    kvs_status_t status =
        kvs_server_create(&kvs_server_, "localhost:50051", &config);
    EXPECT_EQ(status, KVS_STATUS_OK);
  }

  void Stop() {
    if (stop_is_already_called_) {
      return;
    }
    kvs_server_destroy(&kvs_server_);
    stop_is_already_called_ = true;
  }

  void TearDown() override { Stop(); }

 private:
  kvs_server_t* kvs_server_;
  bool stop_is_already_called_ = false;
};

TEST_F(ClientServerTest, SingleClient) {
  int num_nodes = 1;
  StartService(num_nodes, "127.0.0.1:50051");

  kvs_client_t* kvs_client;

  kvs_status_t result;
  kvs_client_config_t config = {.connection_timeout_ms = 3000};

  result = kvs_client_create(&kvs_client, "localhost:50051", &config);
  EXPECT_EQ(result, KVS_STATUS_OK);

  char value[128];

  EXPECT_EQ(kvs_client_set(kvs_client, "key1", "mykey1"), KVS_STATUS_OK);
  EXPECT_EQ(kvs_client_get(kvs_client, "key1", value, sizeof(value)),
            KVS_STATUS_OK);
  EXPECT_STREQ(value, "mykey1");

  EXPECT_EQ(kvs_client_set(kvs_client, "key2", "mykey2"), KVS_STATUS_OK);
  EXPECT_EQ(kvs_client_get(kvs_client, "key2", value, sizeof(value)),
            KVS_STATUS_OK);
  EXPECT_STREQ(value, "mykey2");

  EXPECT_EQ(kvs_client_set(kvs_client, "key1", "mynewkey1"),
            KVS_STATUS_INVALID_USAGE);

  kvs_client_destroy(&kvs_client);
}

TEST_F(ClientServerTest, TwoClients) {
  int num_nodes = 2;
  StartService(num_nodes, "127.0.0.1:50051");

  kvs_client_t* client1 = nullptr;
  kvs_client_t* client2 = nullptr;
  kvs_status_t result;
  kvs_client_config_t config = {.connection_timeout_ms = 3000};

  result = kvs_client_create(&client1, "localhost:50051", &config);
  EXPECT_EQ(result, KVS_STATUS_OK);

  result = kvs_client_create(&client2, "localhost:50051", &config);
  EXPECT_EQ(result, KVS_STATUS_OK);

  char value[128];

  EXPECT_EQ(kvs_client_set(client1, "key1", "mykey1"), KVS_STATUS_OK);
  EXPECT_EQ(kvs_client_get(client2, "key1", value, sizeof(value)),
            KVS_STATUS_OK);
  EXPECT_STREQ(value, "mykey1");
  EXPECT_EQ(kvs_client_get(client1, "key1", value, sizeof(value)),
            KVS_STATUS_OK);
  EXPECT_STREQ(value, "mykey1");

  EXPECT_EQ(kvs_client_set(client2, "key2", "mykey2"), KVS_STATUS_OK);
  EXPECT_EQ(kvs_client_get(client1, "key2", value, sizeof(value)),
            KVS_STATUS_OK);
  EXPECT_STREQ(value, "mykey2");

  kvs_client_destroy(&client1);
}

TEST_F(ClientServerTest, GetValueTimeOut) {
  int num_nodes = 1;
  StartService(num_nodes, "127.0.0.1:50051");

  kvs_client_t* kvs_client;

  kvs_status_t result;
  kvs_client_config_t config = {.connection_timeout_ms = 3000};

  result = kvs_client_create(&kvs_client, "localhost:50051", &config);
  EXPECT_EQ(result, KVS_STATUS_OK);

  char value[128];

  EXPECT_EQ(kvs_client_get(kvs_client, "key1", value, sizeof(value)),
            KVS_STATUS_DEADLINE_EXCEEDED);

  kvs_client_destroy(&kvs_client);
}

}  // namespace
}  // namespace iree
