#!/usr/bin/env bash
set -euxo pipefail

cmake --build build -t all tests mpexamples -- -k 0
