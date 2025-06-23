#!/usr/bin/env bash
set -euxo pipefail

[ "${CI_CONFIG+x}" ] && source "$CI_CONFIG"

cmake -B build -G Ninja "${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"}"
