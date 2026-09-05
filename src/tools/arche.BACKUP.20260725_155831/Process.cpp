#include "Process.h"
#include <cstdlib>
#include <cstdio>
#include <array>
#include <iostream>
#include <memory>

Process::Result Process::run(const std::string& cmd) {
    Result r;
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) { r.exit_code = -1; return r; }
    std::string out;
    std::array<char, 4096> buf;
    while (fgets(buf.data(), buf.size(), pipe))
        out += buf.data();
    r.exit_code = pclose(pipe);
    r.output = std::move(out);
    return r;
}

bool Process::system(const std::string& cmd) {
    return std::system(cmd.c_str()) == 0;
}

bool Process::available(const std::string& cmd) {
    return std::system((cmd + " >/dev/null 2>&1").c_str()) == 0;
}
