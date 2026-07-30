# Copyright (c) The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# This file is included when MP_EXTRAS is enabled. It contains build features
# that are useful but not essential — things callers could configure themselves
# or that their environment might already handle. The distinction from
# CMakeLists.txt is that everything here could in principle be provided
# externally; it lives here to spare developers and packagers from each having
# to reinvent and maintain their own variants.
#
# What belongs here:
#   - Static analysis integration (clang-tidy, IWYU) and the workarounds they need
#   - Compile database export and editor/IDE helpers
#
# What belongs in CMakeLists.txt instead:
#   - Anything required to build or install the library correctly
#   - Anything callers cannot cleanly replicate without access to internals

# Export a compile database for clangd and other tooling, and symlink it into
# the source directory so editors find it without needing a .clangd config
# pointing at the build directory. The symlink is created at configure time
# so it is ready before any source is compiled.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      "${CMAKE_BINARY_DIR}/compile_commands.json"
      "${CMAKE_SOURCE_DIR}/compile_commands.json"
    RESULT_VARIABLE _symlink_result
  )
  if(NOT _symlink_result EQUAL 0)
    message(STATUS "MP_EXTRAS: could not create compile_commands.json symlink in source directory.")
  endif()
endif()

option(MP_ENABLE_CLANG_TIDY "Run clang-tidy with the compiler." OFF)
if(MP_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
  if(NOT CLANG_TIDY_EXECUTABLE)
    message(FATAL_ERROR "MP_ENABLE_CLANG_TIDY is ON but clang-tidy is not found.")
  endif()
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}")
endif()

option(MP_ENABLE_IWYU "Run include-what-you-use with the compiler." OFF)
if(MP_ENABLE_IWYU)
  find_program(IWYU_EXECUTABLE NAMES include-what-you-use iwyu)
  if(NOT IWYU_EXECUTABLE)
    message(FATAL_ERROR "MP_ENABLE_IWYU is ON but include-what-you-use was not found.")
  endif()
  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU_EXECUTABLE};-Xiwyu;--error")
  if(DEFINED ENV{IWYU_MAPPING_FILE})
    list(APPEND CMAKE_CXX_INCLUDE_WHAT_YOU_USE "-Xiwyu" "--mapping_file=$ENV{IWYU_MAPPING_FILE}")
  endif()
endif()

if(MP_ENABLE_CLANG_TIDY OR MP_ENABLE_IWYU)
  # Workaround for nix from https://gitlab.kitware.com/cmake/cmake/-/issues/20912#note_793338
  # Nix injects header paths via $NIX_CFLAGS_COMPILE; CMake tags these as
  # CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES and omits them from the compile
  # database, so clang-tidy, which ignores $NIX_CFLAGS_COMPILE, can't find capnp
  # headers. Setting them as standard passes them to clang-tidy.
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()
