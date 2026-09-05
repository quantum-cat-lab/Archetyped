#pragma once

#include "../Command.h"

class CmdSyncSdk   : public ICommand {
public:
    std::string name() const override { return "sync-sdk"; }
    std::string description() const override { return "Vendor SDK headers with transitive dep resolution"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdCheckSdk  : public ICommand {
public:
    std::string name() const override { return "check-sdk"; }
    std::string description() const override { return "Comprehensive SDK integrity check"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdInitSdk   : public ICommand {
public:
    std::string name() const override { return "init-sdk"; }
    std::string description() const override { return "Create SDK provider skeleton"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdDeps      : public ICommand {
public:
    std::string name() const override { return "deps"; }
    std::string description() const override { return "Show SDK dependency graph"; }
    void execute(const Args& args, Workspace& ws) override;
};
