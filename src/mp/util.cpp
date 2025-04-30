// Copyright (c) The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <mp/config.h>
#include <mp/util.h>

#include <cerrno>
#include <cstdio>
<<<<<<< HEAD
<<<<<<< HEAD
#include <fcntl.h>
||||||| parent of 24c5e57 (util: Clear FD_CLOEXEC on child socket before exec)
#include <filesystem>
#include <iostream>
=======
#include <fcntl.h>
||||||| parent of 4f58c8c (util: Add Windows support)
#include <fcntl.h>
=======
>>>>>>> 4f58c8c (util: Add Windows support)
#include <filesystem>
#include <iostream>
>>>>>>> 24c5e57 (util: Clear FD_CLOEXEC on child socket before exec)
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/string-tree.h>
#include <pthread.h>
#include <sstream>
#include <string>
<<<<<<< HEAD
#include <sys/types.h>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
||||||| parent of 4f58c8c (util: Add Windows support)
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
=======
>>>>>>> 4f58c8c (util: Add Windows support)
#include <system_error>
#include <thread> // NOLINT(misc-include-cleaner) // IWYU pragma: keep
#include <unistd.h>
#include <utility>
#include <vector>

#ifdef WIN32
#include <atomic>
#include <windows.h>
#include <winsock2.h>
#else
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#ifdef __linux__
#include <sys/syscall.h>
#endif

#ifdef HAVE_PTHREAD_GETTHREADID_NP
#include <pthread_np.h>
#endif // HAVE_PTHREAD_GETTHREADID_NP

<<<<<<< HEAD
extern "C" char **environ; // NOLINT(readability-redundant-declaration)
||||||| parent of 4f58c8c (util: Add Windows support)
namespace fs = std::filesystem;
=======
#ifdef WIN32
// Forward-declare internal capnp function.
namespace kj { namespace _ { int win32Socketpair(SOCKET socks[2]); } }
#endif

namespace fs = std::filesystem;
>>>>>>> 4f58c8c (util: Add Windows support)

namespace mp {
namespace {

#ifndef WIN32
std::vector<char*> MakeArgv(const std::vector<std::string>& args)
{
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    return argv;
}

//! Return highest possible file descriptor.
size_t MaxFd()
{
    struct rlimit nofile;
    if (getrlimit(RLIMIT_NOFILE, &nofile) == 0) {
        return nofile.rlim_cur - 1;
    } else {
        return 1023;
    }
}
#endif

} // namespace

std::string ThreadName(const char* exe_name)
{
    char thread_name[16] = {0};
#ifdef HAVE_PTHREAD_GETNAME_NP
    pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
#endif // HAVE_PTHREAD_GETNAME_NP

    std::ostringstream buffer;
    buffer << (exe_name ? exe_name : "") << "-" << getpid() << "/";

    if (thread_name[0] != '\0') {
        buffer << thread_name << "-";
    }

    // Prefer platform specific thread ids over the standard C++11 ones because
    // the former are shorter and are the same as what gdb prints "LWP ...".
#ifdef __linux__
    buffer << syscall(SYS_gettid);
#elif defined(WIN32)
    buffer << GetCurrentThreadId();
#elif defined(HAVE_PTHREAD_THREADID_NP)
    uint64_t tid = 0;
    pthread_threadid_np(nullptr, &tid);
    buffer << tid;
#elif defined(HAVE_PTHREAD_GETTHREADID_NP)
    buffer << pthread_getthreadid_np();
#else
    buffer << std::this_thread::get_id();
#endif

    return std::move(buffer).str();
}

std::string LogEscape(const kj::StringTree& string, size_t max_size)
{
    std::string result;
    string.visit([&](const kj::ArrayPtr<const char>& piece) {
        if (result.size() > max_size) return;
        for (const char c : piece) {
            if (c == '\\') {
                result.append("\\\\");
            } else if (c < 0x20 || c > 0x7e) {
                char escape[4];
                snprintf(escape, sizeof(escape), "\\%02x", static_cast<unsigned char>(c));
                result.append(escape);
            } else {
                result.push_back(c);
            }
            if (result.size() > max_size) {
                result += "...";
                break;
            }
        }
    });
    return result;
}

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
std::tuple<ProcessId, SocketId> SpawnProcess(SpawnConnectInfoToArgsFn&& connect_info_to_args)
||||||| parent of 94af41b (util, refactor: Add SocketId type alias and use it)
int SpawnProcess(int& pid, FdToArgsFn&& fd_to_args)
=======
SocketId SpawnProcess(ProcessId& pid, FdToArgsFn&& fd_to_args)
>>>>>>> 94af41b (util, refactor: Add SocketId type alias and use it)
||||||| parent of beaa50a (util, refactor: Add ConnectInfo type alias and use it)
SocketId SpawnProcess(ProcessId& pid, FdToArgsFn&& fd_to_args)
=======
||||||| parent of 4f58c8c (util: Add Windows support)
=======
//! Generate command line that the executable being invoked will split up using
//! the CommandLineToArgvW function, which expects arguments with spaces to be
//! quoted, quote characters to be backslash-escaped, and backslashes to also be
//! backslash-escaped, but only if they precede a quote character.
std::string CommandLineFromArgv(const std::vector<std::string>& argv)
{
    std::string out;
    for (const auto& arg : argv) {
        if (!out.empty()) out += " ";
        if (!arg.empty() && arg.find_first_of(" \t\"") == std::string::npos) {
            // Argument has no quotes or spaces so escaping not necessary.
            out += arg;
        } else {
            out += '"'; // Start with a quote
            for (size_t i = 0; i < arg.size(); ++i) {
                if (arg[i] == '\\') {
                    // Count consecutive backslashes
                    size_t backslash_count = 0;
                    while (i < arg.size() && arg[i] == '\\') {
                        ++backslash_count;
                        ++i;
                    }
                    if (i < arg.size() && arg[i] == '"') {
                        // Backslashes before a quote need to be doubled
                        out.append(backslash_count * 2 + 1, '\\');
                        out.push_back('"');
                    } else {
                        // Otherwise, backslashes remain as-is
                        out.append(backslash_count, '\\');
                        --i; // Compensate for the outer loop's increment
                    }
                } else if (arg[i] == '"') {
                    // Escape double quotes with a backslash
                    out.push_back('\\');
                    out.push_back('"');
                } else {
                    out.push_back(arg[i]);
                }
            }
            out += '"'; // End with a quote
        }
    }
    return out;
}

>>>>>>> 4f58c8c (util: Add Windows support)
std::tuple<ProcessId, SocketId> SpawnProcess(ConnectInfoToArgsFn&& connect_info_to_args)
>>>>>>> beaa50a (util, refactor: Add ConnectInfo type alias and use it)
{
<<<<<<< HEAD
<<<<<<< HEAD
    auto fds{SocketPair()};
||||||| parent of 94af41b (util, refactor: Add SocketId type alias and use it)
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        throw std::system_error(errno, std::system_category(), "socketpair");
    }
=======
    SocketId fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        throw std::system_error(errno, std::system_category(), "socketpair");
    }
>>>>>>> 94af41b (util, refactor: Add SocketId type alias and use it)
||||||| parent of 022b29b (util, refactor: Add SocketPair() and use it in SpawnProcess)
    SocketId fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        throw std::system_error(errno, std::system_category(), "socketpair");
    }
=======
    auto fds{SocketPair()};
>>>>>>> 022b29b (util, refactor: Add SocketPair() and use it in SpawnProcess)

#ifndef WIN32
    // Evaluate the callback and build the argv array before forking.
    //
    // The parent process may be multi-threaded and holding internal library
    // locks at fork time. In that case, running code that allocates memory or
    // takes locks in the child between fork() and exec() can deadlock
    // indefinitely. Precomputing arguments in the parent avoids this.
    const std::vector<std::string> args{connect_info_to_args(std::to_string(fds[0]))};
    const std::vector<char*> argv{MakeArgv(args)};

<<<<<<< HEAD
    // Clear FD_CLOEXEC on fds[0] before forking so it survives exec in the child.
    int fds0_flags;
    KJ_SYSCALL(fds0_flags = fcntl(fds[0], F_GETFD));
    KJ_SYSCALL(fcntl(fds[0], F_SETFD, fds0_flags & ~FD_CLOEXEC));

    ProcessId pid = fork();
||||||| parent of beaa50a (util, refactor: Add ConnectInfo type alias and use it)
    pid = fork();
=======
    ProcessId pid = fork();
>>>>>>> beaa50a (util, refactor: Add ConnectInfo type alias and use it)
    if (pid == -1) {
        throw std::system_error(errno, std::system_category(), "fork");
    }
    // Parent process closes the descriptor for socket 0, child closes the
    // descriptor for socket 1. On failure, the parent throws, but the child
    // must _exit(126) (post-fork child must not throw).
    if (close(fds[pid ? 0 : 1]) != 0) {
        if (pid) {
            (void)close(fds[1]);
            throw std::system_error(errno, std::system_category(), "close");
        }
        static constexpr char msg[] = "SpawnProcess(child): close(fds[1]) failed\n";
        const ssize_t writeResult = ::write(STDERR_FILENO, msg, sizeof(msg) - 1);
        (void)writeResult;
        _exit(126);
    }

    if (!pid) {
        // Child process must close all potentially open descriptors, except
        // socket 0. Do not throw, allocate, or do non-fork-safe work here.
        const int maxFd = MaxFd();
        for (int fd = 3; fd < maxFd; ++fd) {
            if (fd != fds[0]) {
                close(fd);
            }
        }

        // Explicitly clear FD_CLOEXEC on the child's socket before calling
        // exec, so the fd survives into the spawned process regardless of how
        // the socket was created.
        int flags = fcntl(fds[0], F_GETFD);
        if (flags == -1) throw std::system_error(errno, std::system_category(), "fcntl F_GETFD");
        if (flags & FD_CLOEXEC) {
            flags &= ~FD_CLOEXEC;
            if (fcntl(fds[0], F_SETFD, flags) == -1) throw std::system_error(errno, std::system_category(), "fcntl F_SETFD");
        }

        execvp(argv[0], argv.data());
        // NOTE: perror() is not async-signal-safe; calling it here in a
        // post-fork child may deadlock in multithreaded parents.
        // TODO: Report errors to the parent via a pipe (e.g. write errno)
        // so callers can get diagnostics without relying on perror().
        perror("execvp failed");
        _exit(127);
    }
<<<<<<< HEAD
    return {pid, fds[1]};
||||||| parent of beaa50a (util, refactor: Add ConnectInfo type alias and use it)
    return fds[1];
=======
    return {pid, fds[1]};
#else
    // Create windows pipe to send socket over to child process.
    static std::atomic<int> counter{1};
    ConnectInfo pipe_path{"\\\\.\\pipe\\mp-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(counter.fetch_add(1))};
    HANDLE pipe{CreateNamedPipeA(pipe_path.c_str(), PIPE_ACCESS_OUTBOUND, PIPE_TYPE_MESSAGE | PIPE_WAIT, /*nMaxInstances=*/1, /*nOutBufferSize=*/0, /*nInBufferSize=*/0, /*nDefaultTimeOut=*/0, /*lpSecurityAttributes=*/nullptr)};
    KJ_WIN32(pipe != INVALID_HANDLE_VALUE, "CreateNamedPipe failed");

    // Start child process
    std::string cmd{CommandLineFromArgv(connect_info_to_args(pipe_path))};
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    KJ_WIN32(CreateProcessA(/*lpApplicationName=*/nullptr, const_cast<char*>(cmd.c_str()), /*lpProcessAttributes=*/nullptr, /*lpThreadAttributes=*/nullptr, /*bInheritHandles=*/TRUE, /*dwCreationFlags=*/0, /*lpEnvironment=*/nullptr, /*lpCurrentDirectory=*/nullptr, &si, &pi), "CreateProcess failed");
    KJ_WIN32(CloseHandle(pi.hThread), "CloseHandle(hThread)");

    // Duplicate socket for the child (now that we know its PID)
    WSAPROTOCOL_INFO info{};
    KJ_WINSOCK(WSADuplicateSocket(fds[0], pi.dwProcessId, &info), "WSADuplicateSocket failed");

    // Send socket to the child via the pipe
    KJ_WIN32(ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED, "ConnectNamedPipe failed");
    DWORD wr;
    KJ_WIN32(WriteFile(pipe, &info, sizeof(info), &wr, nullptr) && wr == sizeof(info), "WriteFile(pipe) failed");
    KJ_WIN32(CloseHandle(pipe), "CloseHandle(pipe)");

    return {reinterpret_cast<ProcessId>(pi.hProcess), fds[1]};
#endif
}

SocketId StartSpawned(const ConnectInfo& connect_info)
{
#ifndef WIN32
    return std::stoi(connect_info);
<<<<<<< HEAD
>>>>>>> beaa50a (util, refactor: Add ConnectInfo type alias and use it)
||||||| parent of 4f58c8c (util: Add Windows support)
=======
#else
    HANDLE pipe = CreateFileA(connect_info.c_str(), /*dwDesiredAccess=*/GENERIC_READ, /*dwShareMode=*/0, /*lpSecurityAttributes=*/nullptr, /*dwCreationDisposition=*/OPEN_EXISTING, /*dwFlagsAndAttributes=*/0, /*hTemplateFile=*/nullptr);
    KJ_WIN32(pipe != INVALID_HANDLE_VALUE, "CreateFile(pipe) failed");

    WSAPROTOCOL_INFO info{};
    DWORD rd;
    KJ_WIN32(ReadFile(pipe, &info, sizeof(info), &rd, nullptr) && rd == sizeof(info), "ReadFile(pipe) failed");
    KJ_WIN32(CloseHandle(pipe), "CloseHandle(pipe)");

    WSADATA dontcare;
    if (int wsaErr = WSAStartup(MAKEWORD(2, 2), &dontcare)) KJ_FAIL_WIN32("WSAStartup()", wsaErr);

    SOCKET socket{WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &info, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT)};
    KJ_WINSOCK(socket, "WSASocket(FROM_PROTOCOL_INFO) failed");
    return socket;
#endif
>>>>>>> 4f58c8c (util: Add Windows support)
}

<<<<<<< HEAD
<<<<<<< HEAD
SocketId StartSpawned(const SpawnConnectInfo& connect_info)
||||||| parent of b16f8c4 (util, refactor: Handle forking inside ExecProcess)
void ExecProcess(const std::vector<std::string>& args)
=======
||||||| parent of 022b29b (util, refactor: Add SocketPair() and use it in SpawnProcess)
=======
std::array<SocketId, 2> SocketPair()
{
#ifdef WIN32
    SOCKET pair[2];
    KJ_WINSOCK(kj::_::win32Socketpair(pair));
#else
    int pair[2];
    KJ_SYSCALL(socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
#endif
    return {pair[0], pair[1]};
}

>>>>>>> 022b29b (util, refactor: Add SocketPair() and use it in SpawnProcess)
ProcessId ExecProcess(const std::vector<std::string>& args)
>>>>>>> b16f8c4 (util, refactor: Handle forking inside ExecProcess)
{
<<<<<<< HEAD
<<<<<<< HEAD
    try {
        return std::stoi(connect_info);
    } catch (const std::exception&) {
        throw std::system_error(EINVAL, std::system_category(),
            std::string("StartSpawned: invalid connect_info '") + connect_info + "'");
||||||| parent of b16f8c4 (util, refactor: Handle forking inside ExecProcess)
    const std::vector<char*> argv{MakeArgv(args)};
    if (execvp(argv[0], argv.data()) != 0) {
        perror("execvp failed");
        if (errno == ENOENT && !args.empty()) {
            std::cerr << "Missing executable: " << fs::weakly_canonical(args.front()) << '\n';
        }
        _exit(1);
=======
||||||| parent of 4f58c8c (util: Add Windows support)
=======
#ifndef WIN32
>>>>>>> 4f58c8c (util: Add Windows support)
    const std::vector<char*> argv{MakeArgv(args)};
    ProcessId pid;
    KJ_SYSCALL(pid = fork());
    if (pid) return pid;
    if (execvp(argv[0], argv.data()) != 0) {
        perror("execvp failed");
        if (errno == ENOENT && !args.empty()) {
            std::cerr << "Missing executable: " << fs::weakly_canonical(args.front()) << '\n';
        }
        _exit(1);
>>>>>>> b16f8c4 (util, refactor: Handle forking inside ExecProcess)
    }
    KJ_UNREACHABLE;
#else
    std::string cmd{CommandLineFromArgv(args)};
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    KJ_WIN32(CreateProcessA(/*lpApplicationName=*/nullptr, const_cast<char*>(cmd.c_str()), /*lpProcessAttributes=*/nullptr, /*lpThreadAttributes=*/nullptr, /*bInheritHandles=*/FALSE, /*dwCreationFlags=*/0, /*lpEnvironment=*/nullptr, /*lpCurrentDirectory=*/nullptr, &si, &pi), "CreateProcess");
    KJ_WIN32(CloseHandle(pi.hThread), "CloseHandle(hThread)");
    return reinterpret_cast<ProcessId>(pi.hProcess);
#endif
}

<<<<<<< HEAD
std::array<SocketId, 2> SocketPair()
{
    int pair[2];
    KJ_SYSCALL(socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    KJ_SYSCALL(fcntl(pair[0], F_SETFD, FD_CLOEXEC));
    KJ_SYSCALL(fcntl(pair[1], F_SETFD, FD_CLOEXEC));
    return {pair[0], pair[1]};
}

ProcessId StartProcess(const std::vector<std::string>& args)
{
    const std::vector<char*> argv{MakeArgv(args)};
    ProcessId pid;
    if (int err = posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), ::environ)) {
        KJ_FAIL_SYSCALL("posix_spawnp", err, args.front());
    }
    return pid;
}

int WaitProcess(ProcessId pid)
||||||| parent of 36c91a0 (util, refactor: Add ProcessId type alias and use it)
int WaitProcess(int pid)
=======
int WaitProcess(ProcessId pid)
>>>>>>> 36c91a0 (util, refactor: Add ProcessId type alias and use it)
{
#ifndef WIN32
    int status;
    if (::waitpid(pid, &status, /*options=*/0) != pid) {
        throw std::system_error(errno, std::system_category(), "waitpid");
    }
    return status;
#else
    HANDLE handle{reinterpret_cast<HANDLE>(pid)};
    DWORD result{WaitForSingleObject(handle, /*dwMilliseconds=*/INFINITE)};
    if (result != WAIT_OBJECT_0) KJ_FAIL_WIN32("WaitForSingleObject(child)", GetLastError());
    KJ_WIN32(GetExitCodeProcess(handle, &result), "GetExitCodeProcess");
    KJ_WIN32(CloseHandle(handle), "CloseHandle(process)");
    return result;
#endif
}

} // namespace mp
