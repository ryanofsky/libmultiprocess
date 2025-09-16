// Copyright (c) The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <mp/test/foo.capnp.h>
#include <mp/test/foo.capnp.proxy.h>

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
    //! When to trigger disconnect.
    enum class When {
        //! Trigger disconnect before client makes an IPC call.
        BEFORE_CALL,
        //! Disconnect during a synchronous IPC call (IPC call with no
        //! mp.Context parameter that executes on the Event Loop thread)
        SYNC_BODY,
        //! Disconnect at the start of an asynchronous IPC call (IPC call with
        //! an mp.Context parameter that executes on a worker thread)
        ASYNC_START,
        //! Disconnect when the worker thread is started.
        ASYNC_THREAD_START,
        //! Disconnect while the worker thread is setting up the request_threads
        //! map, with Waiter::m_mutex held.
        ASYNC_THREAD_SETUP,
        //! Disconnect in the body of an asynchronous IPC call.
        ASYNC_THREAD_BODY,
        //! Disconnect while the worker thread is cleaning up the
        //! request_threads map, with Waiter::m_mutex held.
        ASYNC_THREAD_TEARDOWN,
        //! Disconect before exiting the worker thread.
        ASYNC_THREAD_END,
        //! Disconect at the end of an asynchronous call, in the event loop
        //! thread, right before sending the response.
        ASYNC_END,
    };

    //! After disconnecting, whether to wait for the disconnect to be processed
    //! on the other end before continuing, or to wait until the IPC call
    //! returns, or let the disconnect happen naturally. The first mode is
    //! useful for letting test deterministically test all possible states. The
    //! others are useful for checking to see if there are any race conditions,
    //! especially with ThreadSanitizer.
    enum class How { WAIT_DISCONNECTED, WAIT_DONE, YOLO };

    static void Run();

    static constexpr auto Whens() { return std::array{
        When::BEFORE_CALL,
        When::SYNC_BODY,
        When::ASYNC_START,
        When::ASYNC_THREAD_START,
        When::ASYNC_THREAD_SETUP,
        When::ASYNC_THREAD_BODY,
        When::ASYNC_THREAD_TEARDOWN,
        When::ASYNC_THREAD_END,
        When::ASYNC_END,
    }; }
    static constexpr auto Hows() { return std::array{How::WAIT_DISCONNECTED, How::WAIT_DONE, How::YOLO}; }
    static constexpr auto Sides() { return std::array{Side::SERVER, Side::CLIENT}; }
    static constexpr const char* Str(When when)
    {
        switch (when) {
        case When::BEFORE_CALL: return "BEFORE_CALL";
        case When::SYNC_BODY: return "SYNC_BODY";
        case When::ASYNC_START: return "ASYNC_START";
        case When::ASYNC_THREAD_START: return "ASYNC_THREAD_START";
        case When::ASYNC_THREAD_SETUP: return "ASYNC_THREAD_SETUP";
        case When::ASYNC_THREAD_BODY: return "ASYNC_THREAD_BODY";
        case When::ASYNC_THREAD_TEARDOWN: return "ASYNC_THREAD_TEARDOWN";
        case When::ASYNC_THREAD_END: return "ASYNC_THREAD_END";
        case When::ASYNC_END: return "ASYNC_END";
        }
        assert(0);
        return nullptr;
    }
    static constexpr const char* Str(How how)
    {
        switch (how) {
        case How::WAIT_DISCONNECTED: return "WAIT_DISCONNECTED";
        case How::WAIT_DONE: return "WAIT_DONE";
        case How::YOLO: return "YOLO";
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
};

void DisconnectTest::Run()
{
    for (auto when : Whens())
    for (auto how : Hows())
    for (auto side : Sides()) {
        std::cout << "DisconnectTest when=" << Str(when) << " how=" << Str(how) << " side=" << Str(side) << "\n";
        std::promise<void> signal;
        TestSetup setup{/*client_owns_connection=*/side == Side::SERVER && when == When::BEFORE_CALL};
        ProxyClient<messages::FooInterface>* foo = setup.client.get();
        KJ_EXPECT(foo->add(1, 2) == 3);

        // Condition variable, future and variables, for blocking until a
        // a disconnect has been handled or the IPC call is done.
        std::condition_variable cv;
        std::optional<kj::PromiseFulfillerPair<void>> future;
        bool disconnect_handled{false}
        bool ipc_done{false};

        // Helper to trigger disconnection and optionally wait for disconnection to be processed.
        auto do_disconnect{[&] {
            // Add hook to notify condition variable as the server connection
            // object is destroyed.
            {
                const Lock lock(loop->m_mutex);
                Connection* conn{side == Side::Server setup.server_connection.get() : setup.client_connection.get()}
                setup.s_connect->m_sync_cleanup_fns.emplace(setup.s_connect->m_sync_cleanup_fns.end(), [&] {
                    const Lock lock(loop->m_mutex);
                    cv.notify_all();
                });
            }

            // Sanity check thread disconnect hooks called at expected time.
            if (when == When::ASYNC_THREAD_SETUP || when == When::ASYNC_THREAD_TEARDOWN) {
                auto& tc = g_thread_context;
                auto it = tc.request_threads.find(setup.s_connect);
                assert(it != tc.request_threads.end());
            }

            if (side == Side::CLIENT) setup.client_disconnect();
            if (side == Side::SERVER) setup.server_disconnect();
            if (how == How::WAIT_DISCONNECTED) {
                if (when == When::ASYNC_THREAD_SETUP || when == When::ASYNC_THREAD_TEARDOWN) {
                    Lock lock{loop->m_mutex};
                    cv.wait(lock.m_lock, [&] {
                        std::cout << "@@@@ wait size " << tc.request_threads.size() << "\n";
                        return tc.request_threads.find(setup.s_connect) == tc.request_threads.end();
                    });
                }

            } elif (how == How::WAIT_DISCONNECTED) {
                signal.get_future().get();
            }
        }};

        bool call_async{false};
        if (when == When::BEFORE_CALL) {
            do_disconnect();
        } else if (when == When::SYNC_BODY) {
            // Set m_fn to initiate client disconnect when server is in the middle of
            // handling the callFn call to make sure this case is handled cleanly.
            do_disconnect();
        } else {
            call_async = true;
            foo->initThreadMap();
            setup.server->m_impl->m_fn = [&] {
                EventLoopRef loop{*setup.server->m_context.loop};
                if (when == ASYNC_THREAD_BODY) do_disconnect();
            };
            setup.server->m_impl->m_start_hook = []{
                if (when == When::ASYNC_START) do_disconnect();
                return kj::Promise<bool>{true};
            };
            setup.server->m_impl->m_end_hook = []{
                if (when == When::ASYNC_END) do_disconnect();
                return kj::READY_NOW;
            };
            assert(!setup.server->m_context.loop->m_signal);
            setup.server->m_context.loop->m_signal = [](const std::any&) {
                using namespace signals;
                if (when == When::ASYNC_THREAD_START && s.type() == typeid(CallStart)) do_disconnect();
                if (when == When::ASYNC_THREAD_SETUP && s.type() == typeid(Setup)) do_disconnect();
                if (when == When::ASYNC_THREAD_TEARDOWN && s.type() == typeid(CallTeardown)) do_disconnect();
                if (when == When::ASYNC_THREAD_END && s.type() == typeid(CallEnd)) do_disconnect();
            };
        }

        bool disconnected{false};
        try {
            if (call_async) foo->callFnAsync(); else foo->callFn();
        } catch (const std::runtime_error& e) {
            if (side == Side::CLIENT && when == When::BEFORE_CALL) {
                KJ_EXPECT(std::string_view{e.what()} == "IPC client method called after disconnect.");
            } else {
                KJ_EXPECT(std::string_view{e.what()} == "IPC client method call interrupted by disconnect.");
            }
            disconnected = true;
        }
        KJ_EXPECT(disconnected);

        // Now that the disconnect has been detected, set signal allowing the
        // callFn2() IPC call to return. Since signalling may not wake up the
        // thread right away, it is important for the signal variable to be declared
        // *before* the TestSetup variable so is not destroyed while
        // signal.get_future().get() is called.
        if (call_async) signal.set_value();
    }
}

KJ_TEST("Test disconnecting during IPC")
{
    DisconnectTest::Run();
}

} // namespace test
} // namespace mp
