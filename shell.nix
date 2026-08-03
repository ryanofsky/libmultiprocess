{ pkgs ? import <nixpkgs> {}
, crossPkgs ? import <nixpkgs> {}
, enableLibcxx ? false # Whether to use libc++ toolchain and libraries instead of libstdc++
, minimal ? false # Whether to create minimal shell without extra tools (faster when cross compiling)
, capnprotoVersion ? null
, capnprotoSanitizers ? null # Optional sanitizers to build cap'n proto with
, cmakeVersion ? null
, libcxxSanitizers ? null # Optional LLVM_USE_SANITIZER value to use for libc++, see https://llvm.org/docs/CMake.html
}:

let
  lib  = pkgs.lib;
  llvmBase = crossPkgs.llvmPackages_21;
  llvm = llvmBase // lib.optionalAttrs (libcxxSanitizers != null) {
    libcxx = llvmBase.libcxx.override {
      devExtraCmakeFlags = [ "-DLLVM_USE_SANITIZER=${libcxxSanitizers}" ];
    };
  };
  capnprotoHashes = {
    "0.7.0" = "sha256-Y/7dUOQPDHjniuKNRw3j8dG1NI9f/aRWpf8V0WzV9k8=";
    "0.7.1" = "sha256-3cBpVmpvCXyqPUXDp12vCFCk32ZXWpkdOliNH37UwWE=";
    "0.8.0" = "sha256-rfiqN83begjJ9eYjtr21/tk1GJBjmeVfa3C3dZBJ93w=";
    "0.8.1" = "sha256-OZqNVYdyszro5rIe+w6YN00g6y8U/1b8dKYc214q/2o=";
    "0.9.0" = "sha256-yhbDcWUe6jp5PbIXzn5EoKabXiWN8lnS08hyfxUgEQ0=";
    "0.9.2" = "sha256-BspWOPZcP5nCTvmsDE62Zutox+aY5pw42d6hpH3v4cM=";
    "0.10.0" = "sha256-++F4l54OMTDnJ+FO3kV/Y/VLobKVRk461dopanuU3IQ=";
    "0.10.4" = "sha256-45sxnVyyYIw9i3sbFZ1naBMoUzkpP21WarzR5crg4X8=";
    "1.0.0" = "sha256-NLTFJdeOzqhk4ATvkc17Sh6g/junzqYBBEoXYGH/czo=";
    "1.0.2" = "sha256-LVdkqVBTeh8JZ1McdVNtRcnFVwEJRNjt0JV2l7RkuO8=";
    "1.1.0" = "sha256-gxkko7LFyJNlxpTS+CWOd/p9x/778/kNIXfpDGiKM2A=";
    "1.2.0" = "sha256-aDcn4bLZGq8915/NPPQsN5Jv8FRWd8cAspkG3078psc=";
  };
  capnprotoBase = if capnprotoVersion == null then crossPkgs.capnproto else crossPkgs.capnproto.overrideAttrs (old: {
    version = capnprotoVersion;
    src = crossPkgs.fetchFromGitHub {
      owner = "capnproto";
      repo  = "capnproto";
      rev   = "v${capnprotoVersion}";
      hash  = lib.attrByPath [capnprotoVersion] "" capnprotoHashes;
    };
    patches = lib.optionals (lib.versionAtLeast capnprotoVersion "0.9.0" && lib.versionOlder capnprotoVersion "0.10.4") [ ./ci/patches/spaceship.patch ];
    cmakeFlags = (old.cmakeFlags or []) ++ (lib.optionals (lib.versionAtLeast "1.1.0" capnprotoVersion) ["-DCMAKE_POLICY_VERSION_MINIMUM=3.5"]);
  } // (lib.optionalAttrs (lib.versionOlder capnprotoVersion "0.10") {
    env = { }; # Drop -std=c++20 flag forced by nixpkgs
  }));
  capnproto = (capnprotoBase.overrideAttrs (old: lib.optionalAttrs (capnprotoSanitizers != null) {
    env = (old.env or { }) // {
      CXXFLAGS =
        lib.concatStringsSep " " [
          (old.env.CXXFLAGS or "")
          "-fsanitize=${capnprotoSanitizers}"
          "-fno-omit-frame-pointer"
          "-g"
        ];
    };
  })).override (lib.optionalAttrs enableLibcxx { clangStdenv = llvm.libcxxStdenv; });
  clang = if enableLibcxx then llvm.libcxxClang else llvm.clang;
  clang-tools = llvm.clang-tools.override { inherit enableLibcxx; };
  # IWYU parses source files itself with an embedded clang frontend, finding
  # standard library headers through CPATH/CPLUS_INCLUDE_PATH variables set by
  # its nixpkgs wrapper script:
  # https://github.com/NixOS/nixpkgs/blob/master/pkgs/development/tools/analysis/include-what-you-use/wrapper
  # The wrapper bakes in the include paths of `llvmPackages.clang`, which on
  # Linux is the libstdc++-flavored clang wrapper, so by default IWYU analyzes
  # code against libstdc++ even when enableLibcxx is set — making the libc++
  # analysis silently identical to the libstdc++ one and leaving the
  # IWYU_MAPPING_FILE below unused (its keys are libc++-internal header paths
  # that never appear in a libstdc++ parse). Overriding `clang` to the
  # libcxxClang flavor makes the wrapper bake in libc++ include paths instead.
  # The libc++ used is the one from IWYU's own llvmPackages (kept matched to
  # the IWYU release by nixpkgs), not the pinned llvmBase toolchain above; the
  # two can differ by a major version, which is fine for include analysis and
  # avoids rebuilding IWYU against an LLVM its source may not support.
  include-what-you-use = pkgs.include-what-you-use.override (old:
    lib.optionalAttrs enableLibcxx {
      llvmPackages = old.llvmPackages // { clang = old.llvmPackages.libcxxClang; };
    });
  cmakeHashes = {
    "3.12.4" = "sha256-UlVYS/0EPrcXViz/iULUcvHA5GecSUHYS6raqbKOMZQ=";
  };
  cmakeBuild = if cmakeVersion == null then pkgs.cmake else (pkgs.cmake.overrideAttrs (old: {
    version = cmakeVersion;
    src = pkgs.fetchurl {
      url = "https://cmake.org/files/v${lib.versions.majorMinor cmakeVersion}/cmake-${cmakeVersion}.tar.gz";
      hash = lib.attrByPath [cmakeVersion] "" cmakeHashes;
    };
    patches = [];
  })).override { isMinimalBuild = true; };
in crossPkgs.mkShell {
  buildInputs = [
    capnproto
  ];
  nativeBuildInputs = with pkgs; [
    cmakeBuild
    include-what-you-use
    ninja
  ] ++ lib.optionals (!minimal) [
    clang
    clang-tools
  ];

  # Tell IWYU where its libc++ mapping file lives. This must be the mapping
  # file shipped with the same libc++ whose headers IWYU parses (see the
  # include-what-you-use override above), which is IWYU's own libc++, not
  # `llvm.libcxx` from the pinned toolchain.
  IWYU_MAPPING_FILE = if enableLibcxx then "${include-what-you-use.clang.libcxx.dev}/include/c++/v1/libcxx.imp" else null;
}
