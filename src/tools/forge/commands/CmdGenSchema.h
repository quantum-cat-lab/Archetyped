#pragma once
#include "Command.h"
#include <string>

/// @brief `forge gen-schema <ModuleInstance> [--force]` — emit C# `StructLayout` from module.json `types`.
/// @details Reads `PROJECTS/<ModuleInstance>/module.json` `types` array, generates
///          `PROJECTS/<ModuleInstance>/src/main/csharp/Arche.<ModuleInstance>/Generated/Schema.cs` with
///          `[StructLayout(LayoutKind.Sequential, Pack=4)]` per type.
class CmdGenSchema : public ICommand {
public:
    std::string name() const override { return "gen-schema"; }
    std::string description() const override { return "Generate C# StructLayout schema from module.json types"; }
    int execute(const Args& args, Workspace& ws) override;
};
