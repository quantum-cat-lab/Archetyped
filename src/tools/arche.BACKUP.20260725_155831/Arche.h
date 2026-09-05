#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cctype>
#include <set>
#include <tuple>
#include <functional>
#include "json.hpp"
#include "CLIUtils.h"

#define ARCHE_VERSION "0.0.10"

namespace fs = std::filesystem;
using json = nlohmann::json;

class Arche {
private:
    fs::path projectRoot_;
    fs::path modulesRoot_;
    fs::path templatePath_;

    using CommandHandler = std::function<void(const std::vector<std::string>&)>;
    std::unordered_map<std::string, CommandHandler> commandRegistry_;

public:
    Arche();
    int run(int argc, char* argv[]);

    // Accessors for command implementations
    const fs::path& projectRoot() const { return projectRoot_; }
    const fs::path& modulesRoot() const { return modulesRoot_; }
    bool runCommand(const std::string& cmd);
    fs::path resolveModuleDir(const std::string& modId) const;
    void addModuleToWorkspace(const std::string& name);

private:
    void registerCommands();
    void printHelp(const std::string& command = "");
    void replaceInFile(const fs::path& filePath, const std::string& oldStr, const std::string& newStr);
    static std::string macroIdent(const std::string& name);

    // Transitive dependency resolver (used by sync-sdk)
    std::vector<std::tuple<std::string,std::string,std::string,std::string,std::vector<std::string>>>
    resolveTransitive(const std::string& modulePath,
                      const std::map<std::string, std::vector<std::tuple<std::string,std::string,std::string,std::vector<std::string>>>>& byName,
                      const std::vector<std::tuple<std::string,std::string,std::string,std::string,std::vector<std::string>>>& direct,
                      bool quiet);

    // Command implementations (defined across multiple .cpp files)
    void doEngineBuild(const std::vector<std::string>& args);
    void doArcheBuild(const std::vector<std::string>& args);
    void doListMods(const std::vector<std::string>& args);
    void doInfoMod(const std::vector<std::string>& args);
    void doScan(const std::vector<std::string>& args);
    void doInitMod(const std::vector<std::string>& args);
    void doUpdateMod(const std::vector<std::string>& args);
    void doInitCont(const std::vector<std::string>& args);
    void doInfoCont(const std::vector<std::string>& args);
    void doAddMod(const std::vector<std::string>& args);
    void doRemMod(const std::vector<std::string>& args);
    void doBuild(const std::vector<std::string>& args);
    void doRun(const std::vector<std::string>& args);
    void doCpCont(const std::vector<std::string>& args);
    void doRmCont(const std::vector<std::string>& args);
    void doArcCont(const std::vector<std::string>& args);
    void doUnarcCont(const std::vector<std::string>& args);
    void doTest(const std::vector<std::string>& args);
    void doSyncSdk(const std::vector<std::string>& args);
    void doCheckSdk(const std::vector<std::string>& args);
    void doInitSdk(const std::vector<std::string>& args);
    void doDeps(const std::vector<std::string>& args);
    void doDoctor(const std::vector<std::string>& args);
    void doCompletions(const std::vector<std::string>& args);
    void doStatus(const std::vector<std::string>& args);
    void doClean(const std::vector<std::string>& args);
    void doWatch(const std::vector<std::string>& args);
};
