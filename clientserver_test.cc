// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include "kvs.h"

namespace kvs {
namespace {

class ClientServerTest : public ::testing::Test {
 public:
  void StartServer(const std::string& server_addr) {
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
  StartServer("127.0.0.1:50051");

  kvs_client_t* kvs_client;

  kvs_status_t result;
  kvs_client_config_t config = {.connection_timeout_ms = 3000};

  result = kvs_client_create(&kvs_client, "localhost:50051", &config);
  EXPECT_EQ(result, KVS_STATUS_OK);

  char value[128];
  const char key1[] = "key1";
  const char value1[] = "value1";
  const char key2[] = "key2";
  const char value2[] = "value2";

  EXPECT_EQ(
      kvs_client_set(kvs_client, key1, sizeof(key1), value1, sizeof(value1)),
      KVS_STATUS_OK);
  EXPECT_EQ(
      kvs_client_get(kvs_client, key1, sizeof(key1), value, sizeof(value)),
      KVS_STATUS_OK);
  EXPECT_STREQ(value, value1);

  EXPECT_EQ(
      kvs_client_set(kvs_client, key2, sizeof(key2), value2, sizeof(value2)),
      KVS_STATUS_OK);
  EXPECT_EQ(
      kvs_client_get(kvs_client, key2, sizeof(key2), value, sizeof(value)),
      KVS_STATUS_OK);
  EXPECT_STREQ(value, value2);

  EXPECT_EQ(
      kvs_client_set(kvs_client, key1, sizeof(key1), value2, sizeof(value2)),
      KVS_STATUS_INVALID_USAGE);

  kvs_client_destroy(&kvs_client);
}

TEST_F(ClientServerTest, TwoClients) {
  StartServer("127.0.0.1:50051");

  kvs_client_t* client1 = nullptr;
  kvs_client_t* client2 = nullptr;
  kvs_status_t result;
  kvs_client_config_t config = {.connection_timeout_ms = 3000};

  result = kvs_client_create(&client1, "localhost:50051", &config);
  EXPECT_EQ(result, KVS_STATUS_OK);

  result = kvs_client_create(&client2, "localhost:50051", &config);
  EXPECT_EQ(result, KVS_STATUS_OK);

  char value[128];
  const char key1[] = "key1";
  const char value1[] = "value1";
  const char key2[] = "key2";
  const char value2[] = "value2";

  EXPECT_EQ(kvs_client_set(client1, key1, sizeof(key1), value1, sizeof(value1)),
            KVS_STATUS_OK);
  EXPECT_EQ(kvs_client_get(client2, key1, sizeof(key1), value, sizeof(value)),
            KVS_STATUS_OK);
  EXPECT_STREQ(value, value1);
  EXPECT_EQ(kvs_client_get(client1, key1, sizeof(key1), value, sizeof(value)),
            KVS_STATUS_OK);
  EXPECT_STREQ(value, value1);

  EXPECT_EQ(kvs_client_set(client2, key2, sizeof(key2), value2, sizeof(value2)),
            KVS_STATUS_OK);
  EXPECT_EQ(kvs_client_get(client1, key2, sizeof(key2), value, sizeof(value)),
            KVS_STATUS_OK);
  EXPECT_STREQ(value, value2);

  kvs_client_destroy(&client1);
}

TEST_F(ClientServerTest, GetValueTimeOut) {
  StartServer("127.0.0.1:50051");

  kvs_client_t* kvs_client;

  kvs_status_t result;
  kvs_client_config_t config = {.connection_timeout_ms = 3000};

  result = kvs_client_create(&kvs_client, "localhost:50051", &config);
  EXPECT_EQ(result, KVS_STATUS_OK);

  char value[128];
  const char key1[] = "key1";
  const char value1[] = "value1";

  EXPECT_EQ(
      kvs_client_get(kvs_client, key1, sizeof(key1), value, sizeof(value)),
      KVS_STATUS_DEADLINE_EXCEEDED);

  kvs_client_destroy(&kvs_client);
}

}  // namespace
}  // namespace kvs
