# libmultiprocess (support branch)

This branch contains CI scripts, documentation, and examples supporting the
libmultiprocess library. The library source code lives on the `master` branch.

Contents:

- `ci/` — CI scripts, configs, and patches
- `doc/` — design, usage, and installation documentation
- `example/` — example C++ code
- `CMakeLists.txt` — CMake project for building example code
- `shell.nix` — Nix development environment

It is recommended to check out the `support` branch to subdirectory of the
`master` branch, because `CMakeLists.txt` looks for the library source code in
`..` by default. (This can be controlled with the `MP_SOURCE_DIR` option). For
example, the following command will check out the `support` branch to a
subdirectory called `.support`:

```bash
git worktree add .support support
```

See [ci/README.md](ci/README.md) for instructions on running CI jobs locally in
the checked-out branch.
