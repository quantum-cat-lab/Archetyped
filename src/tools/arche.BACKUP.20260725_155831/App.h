#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "Workspace.h"
#include "Command.h"

/// Application shell — owns workspace, command registry, dispatch + REPL.
class App {
public:
    App();

    int run(int argc, char* argv[]);

    /// Register a command (called during construction).
    void addCommand(std::unique_ptr<ICommand> cmd);
    ICommand* findCommand(const std::string& name) const;

    /// Shortcut: look up and execute.
    void dispatch(const std::string& cmdName, const Args& args);

private:
    void printHelp(const std::string& cmd = "") const;

    Workspace ws_;
    std::unordered_map<std::string, std::unique_ptr<ICommand>> registry_;
};
