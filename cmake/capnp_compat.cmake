# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# CMake target definitions for backwards compatibility with Ubuntu bionic
# capnproto 0.6.1 package (https://packages.ubuntu.com/bionic/libcapnp-dev)

include(CheckIncludeFileCXX)
include(CMakePushCheckState)

if (NOT DEFINED capnp_PREFIX AND DEFINED CAPNP_INCLUDE_DIRS)
  get_filename_component(capnp_PREFIX "${CAPNP_INCLUDE_DIRS}" DIRECTORY)
endif()

if (NOT DEFINED CAPNPC_OUTPUT_DIR)
  set(CAPNPC_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif()

if (NOT DEFINED CAPNP_LIB_CAPNPC AND DEFINED CAPNP_LIB_CAPNP-RPC)
  string(REPLACE "-rpc" "c" CAPNP_LIB_CAPNPC "${CAPNP_LIB_CAPNP-RPC}")
endif()

if (NOT TARGET CapnProto::capnp AND DEFINED CAPNP_LIB_CAPNP)
  add_library(CapnProto::capnp SHARED IMPORTED)
  set_target_properties(CapnProto::capnp PROPERTIES IMPORTED_LOCATION "${CAPNP_LIB_CAPNP}")
endif()

if (NOT TARGET CapnProto::capnpc AND DEFINED CAPNP_LIB_CAPNPC)
  add_library(CapnProto::capnpc SHARED IMPORTED)
  set_target_properties(CapnProto::capnpc PROPERTIES IMPORTED_LOCATION "${CAPNP_LIB_CAPNPC}")
endif()

if (NOT TARGET CapnProto::capnp-rpc AND DEFINED CAPNP_LIB_CAPNP-RPC)
  add_library(CapnProto::capnp-rpc SHARED IMPORTED)
  set_target_properties(CapnProto::capnp-rpc PROPERTIES IMPORTED_LOCATION "${CAPNP_LIB_CAPNP-RPC}")
endif()

if (NOT TARGET CapnProto::kj AND DEFINED CAPNP_LIB_KJ)
  add_library(CapnProto::kj SHARED IMPORTED)
  set_target_properties(CapnProto::kj PROPERTIES IMPORTED_LOCATION "${CAPNP_LIB_KJ}")
endif()

if (NOT TARGET CapnProto::kj-async AND DEFINED CAPNP_LIB_KJ-ASYNC)
  add_library(CapnProto::kj-async SHARED IMPORTED)
  set_target_properties(CapnProto::kj-async PROPERTIES IMPORTED_LOCATION "${CAPNP_LIB_KJ-ASYNC}")
endif()

cmake_push_check_state()
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${capnp_INCLUDE_DIRS})
check_include_file_cxx("kj/filesystem.h" HAVE_KJ_FILESYSTEM)
cmake_pop_check_state()