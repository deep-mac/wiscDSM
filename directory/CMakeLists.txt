# Copyright 2018 gRPC authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Assumes protobuf and gRPC have been installed using cmake.
# See cmake_externalproject/CMakeLists.txt for all-in-one cmake build
# that automatically builds all the dependencies before building helloworld.

cmake_minimum_required(VERSION 3.8)

project(Directory C CXX)

include(common.cmake)

# Proto file
get_filename_component(dir_proto "directory.proto" ABSOLUTE)
get_filename_component(dir_proto_path "${dir_proto}" PATH)

# Generated sources
set(dir_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/directory.pb.cc")
set(dir_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/directory.pb.h")
set(dir_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/directory.grpc.pb.cc")
set(dir_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/directory.grpc.pb.h")

# Custom sources
set(client_hdrs "${CMAKE_CURRENT_BINARY_DIR}/../../client.hh")
set(master_hdrs "${CMAKE_CURRENT_BINARY_DIR}/../../master.hh")

add_custom_command(
      OUTPUT "${dir_proto_srcs}" "${dir_proto_hdrs}" "${dir_grpc_srcs}" "${dir_grpc_hdrs}" "${client_hdrs}" "${master_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
	-I "${dir_proto_path}" -I "${dir_proto_path}/../.."
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${dir_proto}"
      DEPENDS "${dir_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/../..")

# dir_grpc_proto
add_library(dir_grpc_proto
  ${dir_grpc_srcs}
  ${dir_grpc_hdrs}
  ${dir_proto_srcs}
  ${dir_proto_hdrs}
  ${client_hdrs}
  ${master_hdrs})
target_link_libraries(dir_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# Targets greeter_[async_](client|server)
foreach(_target
  server)
  add_executable(${_target} "${_target}.cc")
  target_link_libraries(${_target}
    dir_grpc_proto
    absl::flags
    absl::flags_parse
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()
