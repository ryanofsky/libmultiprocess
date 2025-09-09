CI_DESC="CI job using old Cap'n Proto and cmake versions"
CI_DIR=build-olddeps
# Pin olddeps to an older Nixpkgs channel, since compiling the old CMake
# requires an older GCC.
NIXPKGS_CHANNEL=nixos-25.05
export CXXFLAGS="-Werror -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-error=array-bounds"
<<<<<<< HEAD
NIX_ARGS=(--argstr capnprotoVersion "0.9.2" --argstr cmakeVersion "3.12.4" --argstr gccVersion "11")
||||||| parent of 1c2958b (cmake: Increase cmake policy version)
NIX_ARGS=(--argstr capnprotoVersion "0.7.1" --argstr cmakeVersion "3.12.4")
=======
# cmakeVersion here should match minimum version in CMakeLists.txt
NIX_ARGS=(--argstr capnprotoVersion "0.7.1" --argstr cmakeVersion "3.12.4")
>>>>>>> 1c2958b (cmake: Increase cmake policy version)
BUILD_ARGS=(-k)
