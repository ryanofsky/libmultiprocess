// Copyright (c) The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MP_TEST_FOO_TYPES_H
#define MP_TEST_FOO_TYPES_H

#include <mp/proxy.h>
#include <mp/proxy-types.h>

// IWYU pragma: begin_exports
#include <capnp/common.h>
#include <cstddef>
#include <mp/test/foo.capnp.h>
#include <mp/type-context.h>
#include <mp/type-decay.h>
#include <mp/type-function.h>
#include <mp/type-interface.h>
#include <mp/type-map.h>
#include <mp/type-message.h>
#include <mp/type-number.h>
#include <mp/type-set.h>
#include <mp/type-string.h>
#include <mp/type-struct.h>
#include <mp/type-threadmap.h>
#include <mp/type-vector.h>
#include <string>
#include <type_traits>
// IWYU pragma: end_exports

namespace mp {
namespace test {
namespace messages {
struct ExtendedCallback; // IWYU pragma: export
struct FooCallback; // IWYU pragma: export
struct FooFn; // IWYU pragma: export
struct FooInterface; // IWYU pragma: export
} // namespace messages

template <typename Output>
void CustomBuildField(TypeList<FooCustom>, Priority<1>, InvokeContext& invoke_context, const FooCustom& value, Output&& output)
{
    BuildField(TypeList<std::string>(), invoke_context, output, value.v1);
    output.setV2(value.v2);
}

template <typename Input, typename ReadDest>
decltype(auto) CustomReadField(TypeList<FooCustom>, Priority<1>, InvokeContext& invoke_context, Input&& input, ReadDest&& read_dest)
{
    messages::FooCustom::Reader custom = input.get();
    return read_dest.update([&](FooCustom& value) {
        value.v1 = ReadField(TypeList<std::string>(), invoke_context, mp::Make<mp::ValueField>(custom.getV1()), ReadDestTemp<std::string>());
        value.v2 = custom.getV2();
    });
}

} // namespace test

inline void CustomBuildMessage(InvokeContext& invoke_context,
                        const test::FooMessage& src,
                        test::messages::FooMessage::Builder&& builder)
{
    builder.setMessage(src.message + " build");
}

inline void CustomReadMessage(InvokeContext& invoke_context,
                       const test::messages::FooMessage::Reader& reader,
                       test::FooMessage& dest)
{
    dest.message = std::string{reader.getMessage()} + " read";
}

inline void CustomBuildMessage(InvokeContext& invoke_context,
                        const test::FooMutable& src,
                        test::messages::FooMutable::Builder&& builder)
{
    builder.setMessage(src.message + " build");
}

inline void CustomReadMessage(InvokeContext& invoke_context,
                       const test::messages::FooMutable::Reader& reader,
                       test::FooMutable& dest)
{
    dest.message = std::string{reader.getMessage()} + " read";
}

inline void CustomPassMessage(InvokeContext& invoke_context,
                       const test::messages::FooMutable::Reader& reader,
                       test::messages::FooMutable::Builder builder,
                       std::function<void(test::FooMutable&)>&& fn)
{
    test::FooMutable mut;
    mut.message = std::string{reader.getMessage()} + " pass";
    fn(mut);
    builder.setMessage(mut.message + " return");
}

//! CustomBuildField for TestArg parameter. TestArg doesn't do anything special
//! on the client side, so this does nothing. TestArg server-side behavior is
//! implemented in CustomPassField below.
template <typename Output>
requires (std::is_same_v<decltype(std::declval<Output>().get()), test::messages::TestArg::Builder>)
void CustomBuildField(TypeList<>,
    Priority<1>,
    ClientInvokeContext& invoke_context,
    Output&& output)
{
}

//! CustomPassField processing TestArg parameter by calling a start_hook()
//! function which returns a bool promise, and continuing to execute the IPC if
//! the promise value is true, aborting if it is false. It also calls an
//! end_hook() function after the IPC call finishes, if it wasn't aborted.
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
requires (std::is_same_v<decltype(Accessor::get(std::declval<ServerContext>().call_context.getParams())), test::messages::TestArg::Reader>)
auto CustomPassField(TypeList<>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    const auto& start_hook = server_context.proxy_server.m_impl->m_start_hook;
    return start_hook().then([old=server_context, call_context=kj::mv(server_context.call_context), fn, args...](bool invoke_fn) mutable {
        // If start hook returns false, skip calling IPC function.
        if (!invoke_fn) return kj::Promise<typename ServerContext::CallContext>(kj::mv(call_context));
        // If start hook returns true, continue calling IPC function and
        // processing parameters/return values and calling end hook.
        ServerContext server_context{old.proxy_server, call_context, old.req};
        return fn.invoke(server_context, args...).then([old=server_context](ServerContext::CallContext call_context) {
            ServerContext server_context{old.proxy_server, call_context, old.req};
            const auto& end_hook = server_context.proxy_server.m_impl->m_end_hook;
            return end_hook().then([call_context = kj::mv(call_context)] { return call_context; });
        });
    });
}
} // namespace mp

#endif // MP_TEST_FOO_TYPES_H
