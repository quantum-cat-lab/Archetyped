#pragma once

#include <string>
#include <vector>

/// Safe subprocess wrappers — replaces bare std::system().
struct Process {
    struct Result {
        std::string output;
        int exit_code = -1;
        bool ok() const { return exit_code == 0; }
    };

    /// Run a command via popen, capturing stdout. Returns output + exit code.
    static Result run(const std::string& cmd);

    /// Run a command via std::system() — interactive output (cmake builds).
    static bool system(const std::string& cmd);

    /// Check if a program exists and runs successfully (silent).
    static bool available(const std::string& cmd);
};
