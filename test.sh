#!/usr/bin/env bash

set -x
set -e

#git ls-files '**.h' '**.cpp' | xargs clang-format -i

#rm -rvf $HOME/work/mp/build/prefix/bin/mpgen
#(cd build/; /usr/bin/cmake -P cmake_install.cmake)
#exit
#make -C build install VERBOSE=1

# switch from gcc -> clang
#export CXX=clang++

if false; then
    cf="-O0 -ggdb"
    lf=""

    pushd ~/src/capnp
    git checkout origin/release-0.7.0
    git clean -dfx
    cd ~/src/capnp/c++
    autoreconf -i
    ./configure CXX=clang++ CXXFLAGS="$cf" LDFLAGS="$lf" --prefix="$PWD/prefix"
    make -j12
    make install
    popd
fi

if ! [ -e .envrc ]; then
    ln -sv ../meta/nix-libmultiprocess/.envrc .envrc
    ln -sv ../meta/nix-libmultiprocess/shell.nix shell.nix
    eval "$(direnv hook bash)"
fi

rm -rvf build
mkdir -p build
cd build

MP_CXX_FLAGS="-Werror -ftemplate-backtrace-limit=0 -fsanitize=address"
MP_CXX_FLAGS="-Werror -ftemplate-backtrace-limit=0"

CC=clang CXX=clang++ cmake -DCMAKE_INSTALL_PREFIX=$HOME/work/mp/build/prefix -DCapnProto_DEBUG=1 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="$MP_CXX_FLAGS" -DMULTIPROCESS_RUN_CLANG_TIDY=1 ..
#--debug-trycompile
make -j12 check install example mptests mpexamples
#make VERBOSE=1
#make -j12 mptest && gdb -ex run --args build/mptest
#make -j12 all mptest test install

#find $HOME/work/mp/build/prefix -type f
