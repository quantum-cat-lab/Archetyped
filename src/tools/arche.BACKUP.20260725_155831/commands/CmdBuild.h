#pragma once

#include "../Command.h"

class CmdEngineBuild : public ICommand {
public:
    std::string name() const override { return "engine-build"; }
    std::string description() const override { return "Build the Archetyped Engine core"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdArcheBuild  : public ICommand {
public:
    std::string name() const override { return "arche-build"; }
    std::string description() const override { return "Rebuild the arche toolchain"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdBuild       : public ICommand {
public:
    std::string name() const override { return "build"; }
    std::string description() const override { return "Build and pack modules into container"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdRun         : public ICommand {
public:
    std::string name() const override { return "run"; }
    std::string description() const override { return "Launch engine with container"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdWatch       : public ICommand {
public:
    std::string name() const override { return "watch"; }
    std::string description() const override { return "Hot-reload on file change"; }
    void execute(const Args& args, Workspace& ws) override;
};

class CmdClean       : public ICommand {
public:
    std::string name() const override { return "clean"; }
    std::string description() const override { return "Clean build artifacts"; }
    void execute(const Args& args, Workspace& ws) override;
};
