#pragma once

#include "../Command.h"

class CmdModuleList    : public ICommand {
public:
    std::string name() const override { return "list-mods"; }
    std::string description() const override { return "List all available modules"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdModuleInfo    : public ICommand {
public:
    std::string name() const override { return "info-mod"; }
    std::string description() const override { return "Show detailed module info"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdModuleInit    : public ICommand {
public:
    std::string name() const override { return "init-mod"; }
    std::string description() const override { return "Create a module from template"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdModuleUpdate  : public ICommand {
public:
    std::string name() const override { return "update-mod"; }
    std::string description() const override { return "Update module from template"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdModuleScan    : public ICommand {
public:
    std::string name() const override { return "scan"; }
    std::string description() const override { return "Scan modules and update project DB"; }
    void execute(const Args& args, Workspace& ws) override;
};
