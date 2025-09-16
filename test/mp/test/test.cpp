// Copyright (c) The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <mp/test/foo.capnp.h>
#include <mp/test/foo.capnp.proxy.h>
#include <mp/type-context.h>

#include <capnp/capability.h>
#include <capnp/rpc.h>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <kj/async.h>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/memory.h>
#include <kj/test.h>
#include <memory>
#include <mp/proxy.h>
#include <mp/proxy-io.h>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace mp {
namespace test {
using capnp::rpc::twoparty::Side;

/**
 * Test setup class creating a two way connection between a
 * ProxyServer<FooInterface> object and a ProxyClient<FooInterface>.
 *
 * Provides client_disconnect and server_disconnect lambdas that can be used to
 * trigger disconnects and test handling of broken and closed connections.
 *
 * Accepts a client_owns_connection option to test different ProxyClient
 * destroy_connection values and control whether destroying the ProxyClient
 * object destroys the client Connection object. Normally it makes sense for
 * this to be true to simplify shutdown and avoid needing to call
 * client_disconnect manually, but false allows testing more ProxyClient
 * behavior and the "IPC client method called after disconnect" code path.
 */
class TestSetup
{
public:
    std::function<void()> server_disconnect;
    std::function<void()> client_disconnect;
    std::unique_ptr<Connection> server_connection;
    std::unique_ptr<Connection> client_connection;
    std::promise<std::unique_ptr<ProxyClient<messages::FooInterface>>> client_promise;
    std::unique_ptr<ProxyClient<messages::FooInterface>> client;
    ProxyServer<messages::FooInterface>* server{nullptr};
    //! Thread variable should be after other struct members so the thread does
    //! not start until the other members are initialized.
    std::thread thread;

    TestSetup(bool client_owns_connection = true)
        : thread{[&] {
              EventLoop loop("mptest", [](bool raise, const std::string& log) {
                  std::cout << "LOG" << raise << ": " << log << "\n";
                  if (raise) throw std::runtime_error(log);
              });
              auto pipe = loop.m_io_context.provider->newTwoWayPipe();

              server_connection =
                  std::make_unique<Connection>(loop, kj::mv(pipe.ends[0]), [&](Connection& connection) {
                      auto server_proxy = kj::heap<ProxyServer<messages::FooInterface>>(
                          std::make_shared<FooImplementation>(), connection);
                      server = server_proxy;
                      return capnp::Capability::Client(kj::mv(server_proxy));
                  });
              server_disconnect = [&] { loop.sync([&] { server_connection.reset(); }); };
              // Set handler to destroy the server when the client disconnects. This
              // is ignored if server_disconnect() is called instead.
              server_connection->onDisconnect([&] { server_connection.reset(); });

              client_connection = std::make_unique<Connection>(loop, kj::mv(pipe.ends[1]));
              auto client_proxy = std::make_unique<ProxyClient<messages::FooInterface>>(
                  client_connection->m_rpc_system->bootstrap(ServerVatId().vat_id).castAs<messages::FooInterface>(),
                  client_connection.get(), /* destroy_connection= */ client_owns_connection);
              if (client_owns_connection) {
                  client_connection.release();
              } else {
                  client_disconnect = [&] { loop.sync([&] { client_connection.reset(); }); };
              }

              client_promise.set_value(std::move(client_proxy));
              loop.loop();
          }}
    {
        client = client_promise.get_future().get();
    }

    ~TestSetup()
    {
        // Test that client cleanup_fns are executed.
        bool destroyed = false;
        client->m_context.cleanup_fns.emplace_front([&destroyed] { destroyed = true; });
        client.reset();
        KJ_EXPECT(destroyed);

        thread.join();
    }
};

KJ_TEST("Call FooInterface methods")
{
    TestSetup setup;
    ProxyClient<messages::FooInterface>* foo = setup.client.get();

    KJ_EXPECT(foo->add(1, 2) == 3);

    FooStruct in;
    in.name = "name";
    in.setint.insert(2);
    in.setint.insert(1);
    in.vbool.push_back(false);
    in.vbool.push_back(true);
    in.vbool.push_back(false);
    FooStruct out = foo->pass(in);
    KJ_EXPECT(in.name == out.name);
    KJ_EXPECT(in.setint.size() == out.setint.size());
    for (auto init{in.setint.begin()}, outit{out.setint.begin()}; init != in.setint.end() && outit != out.setint.end(); ++init, ++outit) {
        KJ_EXPECT(*init == *outit);
    }
    KJ_EXPECT(in.vbool.size() == out.vbool.size());
    for (size_t i = 0; i < in.vbool.size(); ++i) {
        KJ_EXPECT(in.vbool[i] == out.vbool[i]);
    }

    FooStruct err;
    try {
        foo->raise(in);
    } catch (const FooStruct& e) {
        err = e;
    }
    KJ_EXPECT(in.name == err.name);

    class Callback : public ExtendedCallback
    {
    public:
        Callback(int expect, int ret) : m_expect(expect), m_ret(ret) {}
        int call(int arg) override
        {
            KJ_EXPECT(arg == m_expect);
            return m_ret;
        }
        int callExtended(int arg) override
        {
            KJ_EXPECT(arg == m_expect + 10);
            return m_ret + 10;
        }
        int m_expect, m_ret;
    };

    foo->initThreadMap();
    Callback callback(1, 2);
    KJ_EXPECT(foo->callback(callback, 1) == 2);
    KJ_EXPECT(foo->callbackUnique(std::make_unique<Callback>(3, 4), 3) == 4);
    KJ_EXPECT(foo->callbackShared(std::make_shared<Callback>(5, 6), 5) == 6);
    auto saved = std::make_shared<Callback>(7, 8);
    KJ_EXPECT(saved.use_count() == 1);
    foo->saveCallback(saved);
    KJ_EXPECT(saved.use_count() == 2);
    foo->callbackSaved(7);
    KJ_EXPECT(foo->callbackSaved(7) == 8);
    foo->saveCallback(nullptr);
    KJ_EXPECT(saved.use_count() == 1);
    KJ_EXPECT(foo->callbackExtended(callback, 11) == 12);

    FooCustom custom_in;
    custom_in.v1 = "v1";
    custom_in.v2 = 5;
    FooCustom custom_out = foo->passCustom(custom_in);
    KJ_EXPECT(custom_in.v1 == custom_out.v1);
    KJ_EXPECT(custom_in.v2 == custom_out.v2);

    foo->passEmpty(FooEmpty{});

    FooMessage message1;
    message1.message = "init";
    FooMessage message2{foo->passMessage(message1)};
    KJ_EXPECT(message2.message == "init build read call build read");

    FooMutable mut;
    mut.message = "init";
    foo->passMutable(mut);
    KJ_EXPECT(mut.message == "init build pass call return read");

    KJ_EXPECT(foo->passFn([]{ return 10; }) == 10);
}

// Test disconnect handling before and during IPC calls. This tests
// disconnecting from both the client and server sides, and tests both
// synchronous calls (with no mp.Context argument) that run on the event loop
// thread, and asynchronous calls (with an mp.Context argument) that run on a
// separate worker thread. It can trigger disconnects at different points during
// asynchronous call execution. It can also block after disconnecting, waiting
// for the disconnect to be processed, which is important because otherwise the
// IPC worker thread will usually return too fast and there will not be coverage
// for the server Connection object being destroyed before the ProxyServer
// object (due to the m_rpc_system.reset() call in the ~Connection destructor
// which frees any ProxyServer objects not in use).
struct DisconnectTest {
    //! Enum values indicating when to trigger disconnect and start waiting, and
    //! when to resume execution after waiting.
    enum class When {
        //! Trigger disconnect before client makes an IPC call.
        BEFORE_CALL,
        //! Disconnect at the start of an asynchronous IPC call (IPC call with
        //! an mp.Context parameter that executes on a worker thread)
        ASYNC_START,
        //! Disconnect when the worker thread is started.
        ASYNC_THREAD_START,
        //! Disconnect while the worker thread is setting up the request_threads
        //! map, with Waiter::m_mutex held.
        ASYNC_THREAD_SETUP,
        //! Disconnect in the body of the IPC call.
        INSIDE_CALL,
        //! Disconnect while the worker thread is cleaning up the
        //! request_threads map, with Waiter::m_mutex held.
        ASYNC_THREAD_TEARDOWN,
        //! Disconect before exiting the worker thread.
        ASYNC_THREAD_END,
        //! Disconnect at the end of an asynchronous call, in the event loop
        //! thread, right before sending the response.
        ASYNC_END,
        //! Enum values below are only used to indicate when to stop waiting
        //! after a disconnection. They can't be trigger the disconnection
        //! because they only happen after it is triggered.
        AFTER_CALL,
        CLIENT_DISCONNECT_START,
        CLIENT_DISCONNECT_END,
        SERVER_DISCONNECT_START,
        SERVER_DISCONNECT_END,
        MAX,
    };

    //! Whether to make synchronous IPC call (with no mp.Context parameter that
    //! executes on the Event Loop thread) or an asynchronous IPC call (with an
    //! mp.Context parameter that executes on a worker thread)
    enum class CallType { SYNC, ASYNC };

    template <typename E>
    static constexpr auto Ord(E e) noexcept {
        return static_cast<std::underlying_type_t<E>>(e);
    }

    static constexpr const char* Str(When when)
    {
        switch (when) {
        case When::BEFORE_CALL: return "BEFORE_CALL";
        case When::ASYNC_START: return "ASYNC_START";
        case When::ASYNC_THREAD_START: return "ASYNC_THREAD_START";
        case When::ASYNC_THREAD_SETUP: return "ASYNC_THREAD_SETUP";
        case When::INSIDE_CALL: return "INSIDE_CALL";
        case When::ASYNC_THREAD_TEARDOWN: return "ASYNC_THREAD_TEARDOWN";
        case When::ASYNC_THREAD_END: return "ASYNC_THREAD_END";
        case When::ASYNC_END: return "ASYNC_END";
        case When::AFTER_CALL: return "AFTER_CALL";
        case When::CLIENT_DISCONNECT_START: return "CLIENT_DISCONNECT_START";
        case When::CLIENT_DISCONNECT_END: return "CLIENT_DISCONNECT_END";
        case When::SERVER_DISCONNECT_START: return "SERVER_DISCONNECT_START";
        case When::SERVER_DISCONNECT_END: return "SERVER_DISCONNECT_END";
        case When::MAX: return "MAX";
        }
        assert(0);
        return nullptr;
    }

    static constexpr const char* Str(CallType call_type)
    {
        switch (call_type) {
        case CallType::SYNC: return "SYNC";
        case CallType::ASYNC: return "ASYNC";
        }
        assert(0);
        return nullptr;
    }

    static constexpr const char* Str(Side side)
    {
        switch (side) {
        case Side::SERVER: return "SERVER";
        case Side::CLIENT: return "CLIENT";
        }
        assert(0);
        return nullptr;
    }

    static void Test(When disconnect_trigger, When disconnect_wait, CallType call_type, Side disconnect_side);
    static void TestAll();
};

void DisconnectTest::Test(When disconnect_trigger, When disconnect_wait, CallType call_type, Side disconnect_side)
{
    std::cout << "DisconnectTest trigger=" << Str(disconnect_trigger) << " wait=" << Str(disconnect_wait) << " call_type=" << Str(call_type) << " side=" << Str(disconnect_side) << "\n";

    // Condition variable, future and variables for blocking until a a
    // disconnect has been handled and wait condition has been reached.
    std::condition_variable cv;
    std::optional<kj::PromiseFulfillerPair<void>> future;
    // These need to be declared before TestSetup since they can be accessed in
    // the TestSetup destructor.
    bool wait_done{false};

    // Helper called when a point is reached that could trigger a disconnect or
    // a resume notification after a disconnect. This is declared as a
    // std::function to allow a circular reference between do_when and
    // do_disconnect.
    std::function<void(When)> do_when;

    TestSetup setup{/*client_owns_connection=*/disconnect_side == Side::SERVER && disconnect_trigger == When::BEFORE_CALL};
    ProxyClient<messages::FooInterface>* foo = setup.client.get();
    KJ_EXPECT(foo->add(1, 2) == 3);

    auto do_resume{[&] {
        const Lock lock(setup.server->m_context.loop->m_mutex);
        if (future) future->fulfiller->fulfill();
        wait_done = true;
        cv.notify_all();
    }};

    // Helper to trigger disconnection and optionally wait for disconnection to be processed.
    auto do_disconnect{[&] {
        {
            // Add client/server connection callbacks here. Important to do it at last mem
            const Lock lock(setup.server->m_context.loop->m_mutex);
            setup.client_connection->m_sync_cleanup_fns.emplace(setup.client_connection->m_sync_cleanup_fns.begin(), [&] {
                do_when(When::CLIENT_DISCONNECT_START);
            });
            setup.client_connection->m_sync_cleanup_fns.emplace(setup.client_connection->m_sync_cleanup_fns.end(), [&] {
                do_when(When::CLIENT_DISCONNECT_END);
            });
            setup.server_connection->m_sync_cleanup_fns.emplace(setup.server_connection->m_sync_cleanup_fns.begin(), [&] {
                do_when(When::SERVER_DISCONNECT_START);
            });
            setup.server_connection->m_sync_cleanup_fns.emplace(setup.server_connection->m_sync_cleanup_fns.end(), [&] {
                do_when(When::SERVER_DISCONNECT_END);
            });
        }

        if (disconnect_side == Side::CLIENT) setup.client_disconnect();
        if (disconnect_side == Side::SERVER) setup.server_disconnect();
    }};

    do_when = [&](When when) {
        if (when == disconnect_trigger) {
            // If disconnecting from async thread with Waiter::m_mutex held,
            // deleting server connection would trigger a deadlock in
            // ~Connection ProxyClient<Thread> cleanup hook, so need to schedule
            // running it asynchronously.
            if (disconnect_side == Side::SERVER && (when == When::ASYNC_THREAD_SETUP || when == When::ASYNC_THREAD_TEARDOWN)) {
                setup.server->m_context.loop->m_task_set->add(kj::evalLater([&] { do_disconnect(); }));
            } else {
                do_disconnect();
            }

            if (when != disconnect_wait) {
                // If disconnecting from event loop thread, need to implement
                // wait by fulfilling a promise, not by blocking the thread.
                if (when == When::ASYNC_START || when == When::ASYNC_END) {
                    assert(!future);
                    future.emplace(kj::newPromiseAndFulfiller<void>());
                } else {
                    Lock lock{setup.server->m_context.loop->m_mutex};
                    cv.wait(lock.m_lock, [&] { return wait_done; });
                }
            }
        }
        if (when == disconnect_wait) {
            Lock lock{setup.server->m_context.loop->m_mutex};
            wait_done = true;
            cv.notify_all();
            if (future) future->fulfiller->fulfill();
        }
    };

    do_when(When::BEFORE_CALL);

    setup.server->m_impl->m_fn = [&] {
        EventLoopRef loop{*setup.server->m_context.loop};
        do_when(When::INSIDE_CALL);
    };

    assert(!setup.server->m_context.loop->m_signal);
    setup.server->m_context.loop->m_signal = [&](const std::any& s) {
        using namespace mp::signals;
        if (s.type() == typeid(CallStart))    do_when(When::ASYNC_THREAD_START);
        if (s.type() == typeid(CallSetup))    do_when(When::ASYNC_THREAD_SETUP);
        if (s.type() == typeid(CallTeardown)) do_when(When::ASYNC_THREAD_TEARDOWN);
        if (s.type() == typeid(CallEnd))      do_when(When::ASYNC_THREAD_END);
    };

    setup.server->m_impl->m_start_hook = [&]() -> kj::Promise<bool> {
        do_when(When::ASYNC_START);
        if (future) return future->promise.then([] { return true; });
        return true;
    };

    setup.server->m_impl->m_end_hook = [&]() -> kj::Promise<void> {
        do_when(When::ASYNC_END);
        if (future) return kj::mv(future->promise);
        return kj::READY_NOW;
    };

    if (call_type == CallType::ASYNC) foo->initThreadMap();

    bool disconnected{false};
    try {
        if (call_type == CallType::ASYNC) foo->callFnAsync(); else foo->callFn();
    } catch (const std::runtime_error& e) {
        if (disconnect_side == Side::CLIENT && disconnect_trigger == When::BEFORE_CALL) {
            KJ_EXPECT(std::string_view{e.what()} == "IPC client method called after disconnect.");
        } else {
            KJ_EXPECT(std::string_view{e.what()} == "IPC client method call interrupted by disconnect.");
        }
        disconnected = true;
    }
    KJ_EXPECT(disconnected);
    do_when(When::AFTER_CALL);
}

void DisconnectTest::TestAll()
{
    for (auto side : {Side::SERVER, Side::CLIENT}) {
        // FIXME add more cases
        Test(When::BEFORE_CALL,           When::BEFORE_CALL,           CallType::SYNC, side);
        Test(When::INSIDE_CALL,           When::INSIDE_CALL,           CallType::SYNC, side);
        Test(When::ASYNC_START,           When::ASYNC_START,           CallType::ASYNC, side);
        Test(When::ASYNC_THREAD_START,    When::ASYNC_THREAD_START,    CallType::ASYNC, side);
        Test(When::ASYNC_THREAD_SETUP,    When::ASYNC_THREAD_SETUP,    CallType::ASYNC, side);
        Test(When::INSIDE_CALL,           When::INSIDE_CALL,           CallType::ASYNC, side);
        Test(When::ASYNC_THREAD_TEARDOWN, When::ASYNC_THREAD_TEARDOWN, CallType::ASYNC, side);
        Test(When::ASYNC_THREAD_END,      When::ASYNC_THREAD_END,      CallType::ASYNC, side);
        Test(When::ASYNC_END,             When::ASYNC_END,             CallType::ASYNC, side);
    }
}

KJ_TEST("Test disconnecting during IPC")
{
    DisconnectTest::TestAll();
}
} // namespace test
} // namespace mp
