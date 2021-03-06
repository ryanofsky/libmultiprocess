# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

@0xe102a54b33a43a20;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("mp::test::messages");

using Proxy = import "/mp/proxy.capnp";

interface FooInterface $Proxy.wrap("mp::test::FooImplementation") {
    add @0 (a :Int32, b :Int32) -> (result :Int32);
    mapSize @1 (map :List(Pair(Text, Text))) -> (result :Int32);
    pass @2 (arg :FooStruct) -> (result :FooStruct);
}

struct FooStruct $Proxy.wrap("mp::test::FooStruct") {
    name @0 :Text;
}

struct Pair(Key, Value) {
    key @0 :Key;
    value @1 :Value;
}
