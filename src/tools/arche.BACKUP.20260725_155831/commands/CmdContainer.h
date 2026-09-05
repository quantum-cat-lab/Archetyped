#pragma once

#include "../Command.h"

class CmdContainerInit   : public ICommand {
public:
    std::string name() const override { return "init-cont"; }
    std::string description() const override { return "Create a new container"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdContainerInfo   : public ICommand {
public:
    std::string name() const override { return "info-cont"; }
    std::string description() const override { return "Show container info"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdContainerAdd    : public ICommand {
public:
    std::string name() const override { return "add-mod"; }
    std::string description() const override { return "Add module to container"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdContainerRemove : public ICommand {
public:
    std::string name() const override { return "rem-mod"; }
    std::string description() const override { return "Remove module from container"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdContainerCp     : public ICommand {
public:
    std::string name() const override { return "cp-cont"; }
    std::string description() const override { return "Clone a container"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdContainerRm     : public ICommand {
public:
    std::string name() const override { return "rm-cont"; }
    std::string description() const override { return "Delete a container"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdContainerArc    : public ICommand {
public:
    std::string name() const override { return "arc-cont"; }
    std::string description() const override { return "Archive container"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdContainerUnarc  : public ICommand {
public:
    std::string name() const override { return "unarc-cont"; }
    std::string description() const override { return "Unpack container archive"; }
    void execute(const Args& args, Workspace& ws) override;
};
