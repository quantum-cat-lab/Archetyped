#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

/// Full result of a subprocess execution.
struct SubprocessResult {
    int exit_code   = -1;        // -1 = failed to start
    std::string out;             // stdout data
    std::string err;             // stderr data
    bool timed_out  = false;
    int term_signal = 0;         // non-zero if killed by signal

    bool ok() const { return exit_code == 0 && !timed_out && term_signal == 0; }
    int code() const { return timed_out ? -1 : exit_code; }
};

/// Options controlling subprocess behaviour.
struct SubprocessOptions {
    std::string stdin_data;                        ///< piped to child's stdin
    fs::path cwd;                                  ///< empty = inherit cwd
    std::chrono::milliseconds timeout{0};           ///< 0 = infinite
    bool combine_output = false;                    ///< merge stderr into out

    SubprocessOptions& withStdin(std::string_view s)   { stdin_data = s; return *this; }
    SubprocessOptions& withCwd(const fs::path& p)       { cwd = p; return *this; }
    SubprocessOptions& withTimeout(std::chrono::milliseconds t) { timeout = t; return *this; }
    SubprocessOptions& combine(bool b = true)           { combine_output = b; return *this; }
};

/// RAII subprocess — fork/exec/pipe with full I/O capture.
///
/// Three execution modes:
///   Subprocess::run()       — spawn + args, capture stdout/stderr
///   Subprocess::shell()     — /bin/sh -c "…", capture stdout/stderr
///   Subprocess::stream()    — spawn + args, output goes to terminal (returns exit code)
///
/// All modes support timeout, cwd, and stdin injection.
///
class Subprocess {
public:
    /// Run an executable with arguments (no shell). PATH is searched via execvp().
    static SubprocessResult run(
        const std::string& executable,
        const std::vector<std::string>& args = {},
        const SubprocessOptions& opts = {}
    );

    /// Run with callbacks: stdout/stderr are streamed via callbacks AND captured.
    static SubprocessResult run(
        const std::string& executable,
        const std::vector<std::string>& args,
        const SubprocessOptions& opts,
        std::function<void(std::string_view)> onStdout,
        std::function<void(std::string_view)> onStderr = nullptr
    );

    /// Run via /bin/sh -c "cmdline" (supports pipes, redirects, globs).
    static SubprocessResult shell(
        const std::string& cmdline,
        const SubprocessOptions& opts = {}
    );

    /// Shell with callbacks.
    static SubprocessResult shell(
        const std::string& cmdline,
        const SubprocessOptions& opts,
        std::function<void(std::string_view)> onStdout,
        std::function<void(std::string_view)> onStderr = nullptr
    );

    /// Stream mode: child inherits terminal fds. Returns exit code.
    /// Equivalent to system() but uses fork/exec, not /bin/sh.
    static int stream(
        const std::string& executable,
        const std::vector<std::string>& args = {}
    );

    /// Stream via /bin/sh -c.
    static int streamShell(const std::string& cmdline);

    /// Silent check: true if the executable runs with exit code 0.
    static bool available(const std::string& executable);

private:
    static SubprocessResult runImpl(
        const std::string& executable,
        const std::vector<std::string>& argv,
        const SubprocessOptions& opts,
        std::function<void(std::string_view)> onStdout,
        std::function<void(std::string_view)> onStderr,
        bool useShell
    );

    static void buildArgv(
        const std::string& executable,
        const std::vector<std::string>& args,
        std::vector<std::string>& storage,
        std::vector<const char*>& argv,
        bool useShell
    );
};
