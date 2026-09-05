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

    /// True when a project root was found (marker-based discovery).
    bool valid() const { return !projectRoot_.empty(); }

    const fs::path& projectRoot() const { return projectRoot_; }
    const fs::path& modulesRoot() const { return modulesRoot_; }

    std::vector<ModuleInfo> listModules() const;
    ModuleInfo loadModule(const fs::path& modDir) const;
    fs::path resolveModule(const std::string& name) const;
    bool moduleExists(const std::string& name) const;

    json loadWorkspaceJson() const;
    void saveWorkspaceJson(const json& ws) const;
    fs::path workspaceJsonPath() const;
    void addModuleToWorkspace(const std::string& name);

    std::vector<ContainerInfo> listContainers() const;
    json loadContainerJson(const std::string& name) const;
    void saveContainerJson(const std::string& name, const json& cj) const;
    fs::path containerPath(const std::string& name) const;
    fs::path containersRoot() const;

    json loadSdkManifest(const std::string& modName) const;
    json loadSdkManifest(const fs::path& modPath) const;

    fs::path moduleTemplatePath() const;
    fs::path engineBuildDir() const;
    fs::path engineBinary() const;

    void eachModuleDir(std::function<void(const fs::path&, const std::string&)> fn) const;

private:
    fs::path projectRoot_;    // = cwd  (Archetyped/)
    fs::path modulesRoot_;    // parent of project root: sibling module dirs
};
