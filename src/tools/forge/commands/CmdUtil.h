#pragma once

#include "../Command.h"

class CmdDoctor      : public ICommand {
public:
    std::string name() const override { return "doctor"; }
    std::string description() const override { return "Run system diagnostics"; }
    int execute(const Args& args, Workspace& ws) override;
};

class CmdStatus      : public ICommand {
public:
    std::string name() const override { return "status"; }
    std::string description() const override { return "Project status dashboard"; }
    int execute(const Args& args, Workspace& ws) override;
};

class CmdCompletions : public ICommand {
public:
    std::string name() const override { return "completions"; }
    std::string description() const override { return "Generate shell completion script"; }
    int execute(const Args& args, Workspace& ws) override;
};

class CmdTest        : public ICommand {
public:
    std::string name() const override { return "test"; }
    std::string description() const override { return "Run SDK integration test"; }
    int execute(const Args& args, Workspace& ws) override;
};
