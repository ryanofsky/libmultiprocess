{ pkgs ? import <nixpkgs> {}
, crossPkgs ? import <nixpkgs> {}
, enableLibcxx ? false # Whether to use libc++ toolchain and libraries instead of libstdc++
, minimal ? false # Whether to create minimal shell without extra tools (faster when cross compiling)
, capnprotoVersion ? null
}:

let
  lib  = pkgs.lib;
  llvm = crossPkgs.llvmPackages_20;
  capnprotoHashes = {
    "0.7.0" = "sha256-Y/7dUOQPDHjniuKNRw3j8dG1NI9f/aRWpf8V0WzV9k8=";
    "0.8.0" = "sha256-rfiqN83begjJ9eYjtr21/tk1GJBjmeVfa3C3dZBJ93w=";
  };
  capnprotoBase = if capnprotoVersion == null then crossPkgs.capnproto else crossPkgs.capnproto.overrideAttrs (old: {
    version = capnprotoVersion;
    src = crossPkgs.fetchFromGitHub {
      owner = "capnproto";
      repo  = "capnproto";
      rev   = "v${capnprotoVersion}";
      hash  = lib.attrByPath [capnprotoVersion] "" capnprotoHashes;
    };
    env = {};
    patches = [];
  });
  capnproto = capnprotoBase.override (lib.optionalAttrs enableLibcxx { clangStdenv = llvm.libcxxStdenv; });
  clang = if enableLibcxx then llvm.libcxxClang else llvm.clang;
  clang-tools = llvm.clang-tools.override { inherit enableLibcxx; };
in crossPkgs.mkShell {
  buildInputs = [
    capnproto
  ];
  nativeBuildInputs = with pkgs; [
    cmake
    include-what-you-use
    ninja
  ] ++ lib.optionals (!minimal) [
    clang
    clang-tools
  ];

  # Tell IWYU where its libc++ mapping lives
  IWYU_MAPPING_FILE = if enableLibcxx then "${llvm.libcxx.dev}/include/c++/v1/libcxx.imp" else null;
}
