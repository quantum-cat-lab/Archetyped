#pragma once

#include "Subprocess.h"

// Legacy Process API — thin backward-compat wrapper over Subprocess.
// New code should use Subprocess directly.

struct Process {
    /// Captured run (stdout only, stderr merged). Uses Subprocess::shell internally.
    struct Result {
        std::string output;
        int exit_code = -1;
        bool ok() const { return exit_code == 0; }
    };

    /// Run a shell command and capture its output (stdout + stderr merged).
    static Result run(const std::string& cmd) {
        auto sr = Subprocess::shell(cmd, SubprocessOptions{}.combine(true));
        return {sr.out, sr.exit_code};
    }

    /// Run a shell command with output going directly to the terminal.
    static bool system(const std::string& cmd) {
        return Subprocess::streamShell(cmd) == 0;
    }

    /// Silent check: true if the program exists and exits with code 0.
    static bool available(const std::string& cmd) {
        return Subprocess::available(cmd);
    }
};
