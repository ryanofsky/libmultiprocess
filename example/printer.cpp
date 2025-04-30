// Copyright (c) The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <printer.h>

#include <init.capnp.h>
#include <init.capnp.proxy.h> // NOLINT(misc-include-cleaner) // IWYU pragma: keep

#include <cstring> // IWYU pragma: keep
#include <fstream>
#include <iostream>
#include <kj/async.h>
#include <kj/common.h>
#include <kj/memory.h>
#include <memory>
#include <mp/proxy-io.h>
#include <mp/util.h>
#include <stdexcept>
#include <string>

class PrinterImpl : public Printer
{
public:
    void print(const std::string& message) override { std::cout << "mpprinter: " << message << std::endl; }
};

class InitImpl : public Init
{
public:
    std::unique_ptr<Printer> makePrinter() override { return std::make_unique<PrinterImpl>(); }
};

static void LogPrint(mp::LogMessage log_data)
{
    if (log_data.level == mp::Log::Raise) throw std::runtime_error(log_data.message);
    std::ofstream("debug.log", std::ios_base::app) << log_data.message << std::endl;
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cout << "Usage: mpprinter <fd>\n";
        return 1;
    }
<<<<<<< HEAD
<<<<<<< HEAD
    mp::SocketId socket{mp::StartSpawned(argv[1])};
||||||| parent of 94af41b (util, refactor: Add SocketId type alias and use it)
    int fd;
    if (std::from_chars(argv[1], argv[1] + strlen(argv[1]), fd).ec != std::errc{}) {
        std::cerr << argv[1] << " is not a number or is larger than an int\n";
        return 1;
    }
=======
    mp::SocketId fd;
    if (std::from_chars(argv[1], argv[1] + strlen(argv[1]), fd).ec != std::errc{}) {
        std::cerr << argv[1] << " is not a number or is larger than an int\n";
        return 1;
    }
>>>>>>> 94af41b (util, refactor: Add SocketId type alias and use it)
||||||| parent of beaa50a (util, refactor: Add ConnectInfo type alias and use it)
    mp::SocketId fd;
    if (std::from_chars(argv[1], argv[1] + strlen(argv[1]), fd).ec != std::errc{}) {
        std::cerr << argv[1] << " is not a number or is larger than an int\n";
        return 1;
    }
=======
    mp::SocketId socket{mp::StartSpawned(argv[1])};
>>>>>>> beaa50a (util, refactor: Add ConnectInfo type alias and use it)
    mp::EventLoop loop("mpprinter", LogPrint);
    std::unique_ptr<Init> init = std::make_unique<InitImpl>();
<<<<<<< HEAD
<<<<<<< HEAD
    mp::ServeStream<InitInterface>(loop, mp::MakeStream(loop, socket), *init);
||||||| parent of beaa50a (util, refactor: Add ConnectInfo type alias and use it)
    mp::ServeStream<InitInterface>(loop, fd, *init);
=======
    mp::ServeStream<InitInterface>(loop, socket, *init);
>>>>>>> beaa50a (util, refactor: Add ConnectInfo type alias and use it)
||||||| parent of 091f5e1 (proxy, refactor: Change ConnectStream and ServeStream to accept stream objects)
    mp::ServeStream<InitInterface>(loop, socket, *init);
=======
    mp::ServeStream<InitInterface>(loop, mp::MakeStream(loop.m_io_context, socket), *init);
>>>>>>> 091f5e1 (proxy, refactor: Change ConnectStream and ServeStream to accept stream objects)
    loop.loop();
    return 0;
}
