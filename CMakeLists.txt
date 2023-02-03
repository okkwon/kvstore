cmake_minimum_required(VERSION 3.20)

project(SimpleKVS C CXX)

if (NOT DEFINED CMAKE_CXX_STANDARD)
  set (CMAKE_CXX_STANDARD 17)
endif()

find_package(Threads REQUIRED)

set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}") 

set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)                                                
set(_REFLECTION gRPC::grpc++_reflection)                                                        
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)                                         

                                                                                                  
# Find gRPC installation                                                                        
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.                       
find_package(gRPC CONFIG REQUIRED)                                                              
message(STATUS "Using gRPC ${gRPC_VERSION}")                                                    
                                                                                                  
set(_GRPC_GRPCPP gRPC::grpc++)                                                                  
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)                         

# Proto file
get_filename_component(kvs_proto "keyvaluestore.proto" ABSOLUTE)
get_filename_component(kvs_proto_path "${kvs_proto}" PATH)

# Generated sources
set(kvs_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/keyvaluestore.pb.cc")
set(kvs_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/keyvaluestore.pb.h")
set(kvs_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/keyvaluestore.grpc.pb.cc")
set(kvs_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/keyvaluestore.grpc.pb.h")
add_custom_command(
      OUTPUT "${kvs_proto_srcs}" "${kvs_proto_hdrs}" "${kvs_grpc_srcs}" "${kvs_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${kvs_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${kvs_proto}"
      DEPENDS "${kvs_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# kvs_grpc_proto
add_library(kvs SHARED
  ${kvs_grpc_srcs}
  ${kvs_grpc_hdrs}
  ${kvs_proto_srcs}
  ${kvs_proto_hdrs})
target_link_libraries(kvs
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# client
add_executable(client "client.cc" "caching_interceptor.h")
target_link_libraries(client
  kvs
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# server
add_executable(server "server.cc")
target_link_libraries(server
  kvs
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})
