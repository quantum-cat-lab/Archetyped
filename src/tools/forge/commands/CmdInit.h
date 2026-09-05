#pragma once

#include "../Command.h"

/// `forge init` — initialize a workspace in the current project root.
/// Creates configs/{active_container,module_workspace,sdk-manifest}.json if missing,
/// scans sibling module directories, and populates module_workspace.json with
/// discovered modules + engine components.
class CmdInit : public ICommand {
public:
    std::string name() const override { return "init"; }
    std::string description() const override { return "Initialize workspace configs (idempotent)"; }
    int execute(const Args& args, Workspace& ws) override;
};
