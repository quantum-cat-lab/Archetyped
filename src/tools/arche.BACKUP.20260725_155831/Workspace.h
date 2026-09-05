#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <functional>
#include "json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

/// ModuleInstance metadata from module.json
struct ModuleInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string entry_point;
    std::vector<std::string> dependencies;
    fs::path path;
};

/// Container metadata from container.json
struct ContainerInfo {
    std::string id;
    std::string name;
    std::string version;
    fs::path path;
};

/// Central workspace abstraction — owns path discovery and JSON manifest access.
class Workspace {
public:
    Workspace();

    const fs::path& projectRoot() const { return projectRoot_; }
    const fs::path& modulesRoot() const { return modulesRoot_; }

    // -- ModuleInstance queries -------------------------------------------------------
    std::vector<ModuleInfo> listModules() const;
    ModuleInfo loadModule(const fs::path& modDir) const;
    fs::path resolveModule(const std::string& name) const;
    bool moduleExists(const std::string& name) const;

    // -- ModuleInstance workspace JSON ------------------------------------------------
    json loadWorkspaceJson() const;
    void saveWorkspaceJson(const json& ws) const;
    fs::path workspaceJsonPath() const;
    void addModuleToWorkspace(const std::string& name);

    // -- Container queries ----------------------------------------------------
    std::vector<ContainerInfo> listContainers() const;
    json loadContainerJson(const std::string& name) const;
    void saveContainerJson(const std::string& name, const json& cj) const;
    fs::path containerPath(const std::string& name) const;
    fs::path containersRoot() const;

    // -- SDK queries ----------------------------------------------------------
    json loadSdkManifest(const std::string& modName) const;
    json loadSdkManifest(const fs::path& modPath) const;

    // -- Template / build paths -----------------------------------------------
    fs::path moduleTemplatePath() const;
    fs::path engineBuildDir() const;
    fs::path engineBinary() const;

    // -- Iterate all modules directory ----------------------------------------
    void eachModuleDir(std::function<void(const fs::path&, const std::string&)> fn) const;

private:
    fs::path projectRoot_;    // = cwd  (Archetyped/)
    fs::path modulesRoot_;    // = cwd's parent  (PROJECTS/)
};
