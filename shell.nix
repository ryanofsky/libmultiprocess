{ pkgs ? import <nixpkgs> {} }:

let
  capnproto-patched = pkgs.capnproto.overrideAttrs (old: {
    patches = (old.patches or []) ++ [ ./ci/patches/capnp-tidy.patch ];
  });
in
pkgs.mkShell {
  buildInputs = with pkgs; [
    capnproto-patched
  ];
  nativeBuildInputs = with pkgs; [
    cmake
    include-what-you-use
    llvmPackages_20.clang
    llvmPackages_20.clang-tools
    ninja
  ];
}
