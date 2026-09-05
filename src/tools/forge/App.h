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
    bool workspaceValid() const { return ws_.valid(); }

    /// Register a command (called during construction).
    void addCommand(std::unique_ptr<ICommand> cmd);
    ICommand* findCommand(const std::string& name) const;

    /// Shortcut: look up and execute. Returns the command's exit code.
    int dispatch(const std::string& cmdName, const Args& args);

private:
    void printHelp(const std::string& cmd = "") const;
    /// Resolve alias -> canonical name (single shared table).
    static std::string resolveAlias(const std::string& name);
    /// Closest registry command by edit distance (<= 2), empty if none.
    std::string suggestCommand(const std::string& name) const;

    Workspace ws_;
    std::unordered_map<std::string, std::unique_ptr<ICommand>> registry_;
};
