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

#include <grpcpp/support/client_interceptor.h>

#include <map>

#ifdef BAZEL_BUILD
#include "examples/protos/keyvaluestore.grpc.pb.h"
#else
#include "keyvaluestore.grpc.pb.h"
#endif

// This is a naive implementation of a cache. A new cache is for each call. For
// each new key request, the key is first searched in the map and if found, the
// interceptor fills in the return value without making a request to the server.
// Only if the key is not found in the cache do we make a request.
class CachingInterceptor : public grpc::experimental::Interceptor {
 public:
  CachingInterceptor(grpc::experimental::ClientRpcInfo* info) {}

  void Intercept(
      ::grpc::experimental::InterceptorBatchMethods* methods) override {
    bool hijack = false;
    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::
                PRE_SEND_INITIAL_METADATA)) {
      // Hijack all calls
      hijack = true;
      // Create a stub on which this interceptor can make requests
      stub_ = keyvaluestore::KeyValueStore::NewStub(
          methods->GetInterceptedChannel());
    }
    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::PRE_SEND_MESSAGE)) {
      // We know that clients perform a Read and a Write in a loop, so we don't
      // need to maintain a list of the responses.
      std::string requested_key;
      const keyvaluestore::GetValueRequest* req_msg =
          static_cast<const keyvaluestore::GetValueRequest*>(
              methods->GetSendMessage());
      if (req_msg != nullptr) {
        requested_key = req_msg->key();
      } else {
        // The non-serialized form would not be available in certain scenarios,
        // so add a fallback
        keyvaluestore::GetValueRequest req_msg;
        auto* buffer = methods->GetSerializedSendMessage();
        auto copied_buffer = *buffer;
        GPR_ASSERT(grpc::SerializationTraits<keyvaluestore::GetValueRequest>::
                       Deserialize(&copied_buffer, &req_msg)
                           .ok());
        requested_key = req_msg.key();
      }

      // Check if the key is present in the map
      auto search = cached_map_.find(requested_key);
      if (search != cached_map_.end()) {
        std::cout << "Key " << requested_key << "found in map";
        response_ = search->second;
      } else {
        std::cout << "Key " << requested_key << " not found in cache";
        // Key was not found in the cache, so make a request
        keyvaluestore::GetValueRequest req;
        req.set_key(requested_key);
        keyvaluestore::GetValueResponse resp;
        stub_->GetValue(&context_, req, &resp);
        response_ = resp.value();
        // Insert the pair in the cache for future requests
        cached_map_.insert({requested_key, response_});
      }
    }
    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::PRE_RECV_MESSAGE)) {
      keyvaluestore::GetValueResponse* resp =
          static_cast<keyvaluestore::GetValueResponse*>(
              methods->GetRecvMessage());
      resp->set_value(response_);
    }
    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::PRE_RECV_STATUS)) {
      auto* status = methods->GetRecvStatus();
      *status = grpc::Status::OK;
    }
    // One of Hijack or Proceed always needs to be called to make progress.
    if (hijack) {
      // Hijack is called only once when PRE_SEND_INITIAL_METADATA is present in
      // the hook points
      methods->Hijack();
    } else {
      // Proceed is an indicator that the interceptor is done intercepting the
      // batch.
      methods->Proceed();
    }
  }

 private:
  grpc::ClientContext context_;
  std::unique_ptr<keyvaluestore::KeyValueStore::Stub> stub_;
  std::map<std::string, std::string> cached_map_;
  std::string response_;
};

class CachingInterceptorFactory
    : public grpc::experimental::ClientInterceptorFactoryInterface {
 public:
  grpc::experimental::Interceptor* CreateClientInterceptor(
      grpc::experimental::ClientRpcInfo* info) override {
    return new CachingInterceptor(info);
  }
};
