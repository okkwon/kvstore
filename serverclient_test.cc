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

// Logic and data behind the server's behavior.
class KeyValueStoreServiceImpl final : public KeyValueStore::Service {
  Status GetValue(ServerContext* context, const GetValueRequest* request,
                  GetValueResponse* response) override {
    response->set_value(get_value_from_map(request->key()));
    return Status::OK;
  }

  Status SetValue(ServerContext* context, const SetValueRequest* request,
                  SetValueResponse* response) override {
    if (kv_map.count(request->key())) {
      // We expect only one client sets a value with a key only once.
      return Status(grpc::StatusCode::ALREADY_EXISTS,
                    "Updating an existing value is not supported");
    }
    kv_map[request->key()] = request->value();
    return Status::OK;
  }

 private:
  std::string get_value_from_map(const std::string& key) {
    if (kv_map.count(key))
      return kv_map[key];
    else
      return "";
  }

  // key value
  std::unordered_map<std::string, std::string> kv_map;
};

class ClientServerTest : public ::testing::Test {
 public:
  void StartService(int num_nodes, const std::string server_addr) {
    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case, it corresponds to an *synchronous* service.
    builder.RegisterService(&service_);
    // Finally assemble the server.
    server_ = builder.BuildAndStart();
  }

  void Stop() {
    if (stop_is_already_called_) {
      return;
    }
    server_->Shutdown();
    stop_is_already_called_ = true;
  }

  void TearDown() override { Stop(); }

  std::unique_ptr<::grpc::Server> server_;

 private:
  KeyValueStoreServiceImpl service_;
  bool stop_is_already_called_ = false;
};

TEST_F(ClientServerTest, SingleClient) {
  int num_nodes = 1;
  StartService(num_nodes, "127.0.0.1:50051");

  kvs_t* store;

  kvsStatus_t result;
  kvsConfig_t config = {3000, 3000};

  result = kvs_create(&store, "localhost:50051", &config);
  EXPECT_EQ(result, kvsStatusOK);

  char value[128];

  EXPECT_EQ(kvs_set(store, "key1", "mykey1"), kvsStatusOK);
  EXPECT_EQ(kvs_get(store, "key1", value, sizeof(value)), kvsStatusOK);
  EXPECT_STREQ(value, "mykey1");
  
  EXPECT_EQ(kvs_set(store, "key2", "mykey2"), kvsStatusOK);
  EXPECT_EQ(kvs_get(store, "key2", value, sizeof(value)), kvsStatusOK);
  EXPECT_STREQ(value, "mykey2");

  EXPECT_EQ(kvs_set(store, "key1", "mynewkey1"), kvsStatusInvalidUsage);

  kvs_destroy(&store);
}

}  // namespace
}  // namespace iree
