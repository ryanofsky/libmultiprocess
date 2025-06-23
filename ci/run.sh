#!/usr/bin/env bash
set -euxo pipefail

[ "${CI_CONFIG+x}" ] && source "$CI_CONFIG"

for stage in configure build test; do
  nix-shell --pure --keep CI_CONFIG "${NIX_ARGS[@]+"${NIX_ARGS[@]}"}" --run ci/scripts/${stage}.sh shell.nix
done
