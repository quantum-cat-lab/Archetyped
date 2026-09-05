#include "Subprocess.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <signal.h>
#include <fcntl.h>

#include <iostream>
#include <algorithm>
#include <mutex>
#include <array>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool setNonBlocking(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    return fl >= 0 && fcntl(fd, F_SETFL, fl | O_NONBLOCK) == 0;
}

// Read all available data from an fd into a string buffer.
// Returns false on error (other than EAGAIN).
static bool drainFd(int fd, std::string& buf) {
    std::array<char, 65536> chunk;
    while (true) {
        ssize_t n = ::read(fd, chunk.data(), chunk.size());
        if (n > 0) {
            buf.append(chunk.data(), static_cast<size_t>(n));
        } else if (n == 0) {
            return true;               // EOF
        } else {
            if (errno == EAGAIN || errno == EINTR)
                return true;           // no more data right now
            return false;               // real error
        }
    }
}

// Build a C-compatible argv array from executable + args.
// storage owns the strings; argv is the final null-terminated array.
void Subprocess::buildArgv(
    const std::string& executable,
    const std::vector<std::string>& args,
    std::vector<std::string>& storage,
    std::vector<const char*>& argv,
    bool useShell)
{
    if (useShell) {
        storage = {"/bin/sh", "-c", executable};
    } else {
        storage.clear();
        storage.reserve(1 + args.size());
        storage.push_back(executable);
        storage.insert(storage.end(), args.begin(), args.end());
    }
    argv.clear();
    argv.reserve(storage.size() + 1);
    for (const auto& s : storage)
        argv.push_back(s.c_str());
    argv.push_back(nullptr);
}

// ---------------------------------------------------------------------------
// Internal implementation
// ---------------------------------------------------------------------------

SubprocessResult Subprocess::runImpl(
    const std::string& executable,
    const std::vector<std::string>& args,
    const SubprocessOptions& opts,
    std::function<void(std::string_view)> onStdout,
    std::function<void(std::string_view)> onStderr,
    bool useShell)
{
    SubprocessResult result;
    int outPipe[2] = {-1, -1};
    int errPipe[2] = {-1, -1};
    int inPipe[2]  = {-1, -1};

    if (::pipe(outPipe) < 0) return result;
    if (opts.combine_output) {
        errPipe[0] = -1; errPipe[1] = -1;
    } else {
        if (::pipe(errPipe) < 0) { ::close(outPipe[0]); ::close(outPipe[1]); return result; }
    }
    if (!opts.stdin_data.empty()) {
        if (::pipe(inPipe) < 0) {
            ::close(outPipe[0]); ::close(outPipe[1]);
            if (errPipe[0] >= 0) { ::close(errPipe[0]); ::close(errPipe[1]); }
            return result;
        }
    }

    // Set read ends to non-blocking for poll-based reading
    setNonBlocking(outPipe[0]);
    if (errPipe[0] >= 0) setNonBlocking(errPipe[0]);

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(outPipe[0]); ::close(outPipe[1]);
        if (errPipe[0] >= 0) { ::close(errPipe[0]); ::close(errPipe[1]); }
        if (inPipe[0] >= 0)  { ::close(inPipe[0]); ::close(inPipe[1]); }
        return result;
    }

    if (pid == 0) {
        if (!opts.stdin_data.empty()) {
            ::dup2(inPipe[0], STDIN_FILENO);
            ::close(inPipe[1]);
        } else {
            // /dev/null so the child never blocks on stdin
            int nullfd = ::open("/dev/null", O_RDONLY);
            if (nullfd >= 0) { ::dup2(nullfd, STDIN_FILENO); ::close(nullfd); }
        }

        ::dup2(outPipe[1], STDOUT_FILENO);
        ::close(outPipe[0]);
        ::close(outPipe[1]);

        if (opts.combine_output) {
            ::dup2(STDOUT_FILENO, STDERR_FILENO);
        } else {
            ::dup2(errPipe[1], STDERR_FILENO);
            ::close(errPipe[0]);
            ::close(errPipe[1]);
        }

        if (inPipe[0] >= 0) ::close(inPipe[0]);

        if (!opts.cwd.empty()) {
            if (::chdir(opts.cwd.c_str()) != 0) {
                // Can't change to requested directory — fail early
                ::_exit(126);
            }
        }

        // Restore default signal handlers so child isn't affected by parent's
        signal(SIGPIPE, SIG_DFL);

        if (useShell) {
            ::execl("/bin/sh", "/bin/sh", "-c", executable.c_str(), nullptr);
        } else {
            std::vector<std::string> storage;
            std::vector<const char*> argv;
            buildArgv(executable, args, storage, argv, false);
            ::execvp(executable.c_str(), const_cast<char* const*>(argv.data()));
        }
        // If we get here, exec failed
        ::_exit(127);
    }

    ::close(outPipe[1]);
    if (errPipe[1] >= 0) ::close(errPipe[1]);
    if (inPipe[0] >= 0) ::close(inPipe[0]);

    if (!opts.stdin_data.empty() && inPipe[1] >= 0) {
        const char* data = opts.stdin_data.data();
        size_t remain = opts.stdin_data.size();
        while (remain > 0) {
            ssize_t n = ::write(inPipe[1], data, remain);
            if (n > 0) { data += n; remain -= static_cast<size_t>(n); }
            else if (errno != EINTR) break;
        }
        ::close(inPipe[1]);
    }

    int outFd  = outPipe[0];
    int errFd  = errPipe[0];
    bool outEof = false, errEof = false;
    auto deadline = opts.timeout.count() > 0
        ? std::chrono::steady_clock::now() + opts.timeout
        : std::chrono::steady_clock::time_point::max();

    while (!outEof || (errFd >= 0 && !errEof)) {
        if (opts.timeout.count() > 0) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                ::kill(pid, SIGTERM);
                // Give it a moment, then SIGKILL
                auto killDeadline = now + std::chrono::milliseconds(100);
                while (std::chrono::steady_clock::now() < killDeadline) {
                    int wstatus = 0;
                    if (::waitpid(pid, &wstatus, WNOHANG) > 0) break;
                    ::usleep(1000);
                }
                ::kill(pid, SIGKILL);
                ::waitpid(pid, nullptr, 0);
                result.timed_out = true;
                goto cleanup;
            }
        }

        struct pollfd pfds[2];
        nfds_t nfds = 0;
        if (!outEof) {
            pfds[nfds].fd = outFd; pfds[nfds].events = POLLIN; nfds++;
        }
        if (errFd >= 0 && !errEof) {
            pfds[nfds].fd = errFd; pfds[nfds].events = POLLIN; nfds++;
        }
        if (nfds == 0) break;

        int pollTimeout = opts.timeout.count() > 0 ? 100 : 200; // ms
        int ret = ::poll(pfds, nfds, pollTimeout);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (nfds_t i = 0; i < nfds; ++i) {
            if (pfds[i].revents & (POLLIN | POLLHUP)) {
                int fd = pfds[i].fd;
                std::string buf;
                bool ok = drainFd(fd, buf);
                if (fd == outFd) {
                    if (!buf.empty()) {
                        result.out += buf;
                        if (onStdout) onStdout(buf);
                    }
                    if (!ok || (pfds[i].revents & POLLHUP)) outEof = true;
                } else if (fd == errFd) {
                    if (!buf.empty()) {
                        result.err += buf;
                        if (onStderr) onStderr(buf);
                    }
                    if (!ok || (pfds[i].revents & POLLHUP)) errEof = true;
                }
            }
        }
    }

    drainFd(outFd, result.out);
    if (errFd >= 0) drainFd(errFd, result.err);

    {
        int wstatus = 0;
        if (::waitpid(pid, &wstatus, 0) > 0) {
            if (WIFEXITED(wstatus)) {
                result.exit_code = WEXITSTATUS(wstatus);
            } else if (WIFSIGNALED(wstatus)) {
                result.term_signal = WTERMSIG(wstatus);
                result.exit_code  = -1;
            }
        }
    }

cleanup:
    ::close(outFd);
    if (errFd >= 0) ::close(errFd);

    return result;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

SubprocessResult Subprocess::run(
    const std::string& executable,
    const std::vector<std::string>& args,
    const SubprocessOptions& opts)
{
    return runImpl(executable, args, opts, nullptr, nullptr, false);
}

SubprocessResult Subprocess::run(
    const std::string& executable,
    const std::vector<std::string>& args,
    const SubprocessOptions& opts,
    std::function<void(std::string_view)> onStdout,
    std::function<void(std::string_view)> onStderr)
{
    return runImpl(executable, args, opts, std::move(onStdout), std::move(onStderr), false);
}

SubprocessResult Subprocess::shell(
    const std::string& cmdline,
    const SubprocessOptions& opts)
{
    return runImpl(cmdline, {}, opts, nullptr, nullptr, true);
}

SubprocessResult Subprocess::shell(
    const std::string& cmdline,
    const SubprocessOptions& opts,
    std::function<void(std::string_view)> onStdout,
    std::function<void(std::string_view)> onStderr)
{
    return runImpl(cmdline, {}, opts, std::move(onStdout), std::move(onStderr), true);
}

int Subprocess::stream(const std::string& executable, const std::vector<std::string>& args) {
    pid_t pid = ::fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        // Child: inherit parent's fds
        std::vector<std::string> storage;
        std::vector<const char*> argv;
        buildArgv(executable, args, storage, argv, false);
        ::execvp(executable.c_str(), const_cast<char* const*>(argv.data()));
        ::_exit(127);
    }

    int wstatus = 0;
    if (::waitpid(pid, &wstatus, 0) > 0) {
        if (WIFEXITED(wstatus)) return WEXITSTATUS(wstatus);
        if (WIFSIGNALED(wstatus)) return -WTERMSIG(wstatus);
    }
    return -1;
}

int Subprocess::streamShell(const std::string& cmdline) {
    pid_t pid = ::fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        ::execl("/bin/sh", "/bin/sh", "-c", cmdline.c_str(), nullptr);
        ::_exit(127);
    }

    int wstatus = 0;
    if (::waitpid(pid, &wstatus, 0) > 0) {
        if (WIFEXITED(wstatus)) return WEXITSTATUS(wstatus);
        if (WIFSIGNALED(wstatus)) return -WTERMSIG(wstatus);
    }
    return -1;
}

bool Subprocess::available(const std::string& executable) {
    auto r = shell("command -v " + executable + " >/dev/null 2>&1");
    return r.ok();
}
