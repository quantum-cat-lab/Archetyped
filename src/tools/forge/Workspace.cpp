#include "Workspace.h"
#include "CLIUtils.h"
#include <fstream>
#include <iostream>
#include <set>

namespace {
    // Walk up from cwd looking for a directory that looks like the project
    // root (has CMakeLists.txt and container/), like git does with .git.
    fs::path findProjectRoot() {
        fs::path dir = fs::current_path();
        // parent_path() of "/" is "/" — guard on reaching root, not emptiness,
        // otherwise the loop spins forever stat'ing "/".
        for (; dir != dir.root_path(); dir = dir.parent_path()) {
            std::error_code ec;
            if (fs::exists(dir / "CMakeLists.txt", ec) && fs::is_directory(dir / "container", ec))
                return dir;
        }
        if (std::error_code ec; fs::exists(dir / "CMakeLists.txt", ec) && fs::is_directory(dir / "container", ec))
            return dir;
        return {}; // not found — caller must refuse to guess
    }
}

Workspace::Workspace()
    : projectRoot_(findProjectRoot())
    // Modules live as sibling directories of the project root (PROJECTS/).
    , modulesRoot_(projectRoot_.empty() ? fs::path() : projectRoot_.parent_path())
{}

ModuleInfo Workspace::loadModule(const fs::path& modDir) const {
    ModuleInfo mi;
    mi.path = modDir;
    fs::path jp = modDir / "module.json";
    if (!fs::exists(jp)) return mi;
    try {
        std::ifstream f(jp);
        json j; f >> j;
        mi.id          = j.value("id", "");
        mi.name        = j.value("name", "");
        mi.version     = j.value("version", "");
        mi.entry_point = j.value("entry_point", mi.id + ".so");
        if (j.contains("dependencies") && j["dependencies"].is_array())
            for (auto& d : j["dependencies"])
                mi.dependencies.push_back(d.get<std::string>());
    } catch (...) {}
    return mi;
}

std::vector<ModuleInfo> Workspace::listModules() const {
    std::vector<ModuleInfo> out;
    eachModuleDir([&](const fs::path& p, const std::string&) {
        ModuleInfo mi = loadModule(p);
        if (!mi.id.empty()) out.push_back(std::move(mi));
    });
    return out;
}

fs::path Workspace::resolveModule(const std::string& name) const {
    if (fs::exists(modulesRoot_ / name / "module.json"))
        return modulesRoot_ / name;
    for (const auto& entry : fs::directory_iterator(modulesRoot_)) {
        if (!entry.is_directory()) continue;
        fs::path jp = entry.path() / "module.json";
        if (!fs::exists(jp)) continue;
        try {
            std::ifstream f(jp);
            json j; f >> j;
            if (j.value("id", "") == name)
                return entry.path();
        } catch (...) {}
    }
    return {};
}

bool Workspace::moduleExists(const std::string& name) const {
    return !resolveModule(name).empty();
}

fs::path Workspace::workspaceJsonPath() const {
    return projectRoot_ / "configs" / "module_workspace.json";
}

json Workspace::loadWorkspaceJson() const {
    fs::path p = workspaceJsonPath();
    if (!fs::exists(p)) return json::object();
    std::ifstream f(p);
    json j; f >> j;
    return j;
}

void Workspace::saveWorkspaceJson(const json& ws) const {
    fs::path p = workspaceJsonPath();
    fs::create_directories(p.parent_path());
    std::ofstream f(p);
    f << ws.dump(2) << std::endl;
}

void Workspace::addModuleToWorkspace(const std::string& name) {
    fs::path p = workspaceJsonPath();
    if (!fs::exists(p)) return;
    json ws = loadWorkspaceJson();
    for (auto& mod : ws["modules"])
        if (mod.value("name", "") == name) return;
    ws["modules"].push_back({{"name", name}, {"path", name}});
    saveWorkspaceJson(ws);
}

fs::path Workspace::containersRoot() const {
    return projectRoot_ / "container";
}

fs::path Workspace::containerPath(const std::string& name) const {
    return containersRoot() / name;
}

json Workspace::loadContainerJson(const std::string& name) const {
    fs::path p = containerPath(name) / "container.json";
    if (!fs::exists(p)) return json::object();
    std::ifstream f(p);
    json j; f >> j;
    return j;
}

void Workspace::saveContainerJson(const std::string& name, const json& cj) const {
    fs::path p = containerPath(name) / "container.json";
    fs::create_directories(p.parent_path());
    std::ofstream f(p);
    f << cj.dump(4);
}

std::vector<ContainerInfo> Workspace::listContainers() const {
    std::vector<ContainerInfo> out;
    fs::path cd = containersRoot();
    if (!fs::exists(cd)) return out;
    for (const auto& entry : fs::directory_iterator(cd)) {
        if (!entry.is_directory()) continue;
        fs::path jp = entry.path() / "container.json";
        if (!fs::exists(jp)) continue;
        try {
            std::ifstream f(jp);
            json j; f >> j;
            ContainerInfo ci;
            ci.id   = j.value("container_id", "");
            ci.name = j.value("name", entry.path().filename().string());
            ci.version = j.value("version", "");
            ci.path = entry.path();
            out.push_back(std::move(ci));
        } catch (...) {}
    }
    return out;
}

json Workspace::loadSdkManifest(const std::string& modName) const {
    fs::path p = resolveModule(modName);
    if (p.empty()) return json::object();
    return loadSdkManifest(p);
}

json Workspace::loadSdkManifest(const fs::path& modPath) const {
    fs::path p = modPath / "configs" / "sdk-manifest.json";
    if (!fs::exists(p)) return json::object();
    std::ifstream f(p);
    json j; f >> j;
    return j;
}

fs::path Workspace::moduleTemplatePath() const {
    return projectRoot_ / "assets" / "templates" / "module";
}

fs::path Workspace::engineBuildDir() const {
    return projectRoot_ / "build";
}

fs::path Workspace::engineBinary() const {
    return engineBuildDir() / "archetyped";
}

void Workspace::eachModuleDir(
    std::function<void(const fs::path&, const std::string&)> fn) const
{
    if (!fs::exists(modulesRoot_)) return;
    for (const auto& entry : fs::directory_iterator(modulesRoot_)) {
        if (!entry.is_directory()) continue;
        fs::path jp = entry.path() / "module.json";
        if (!fs::exists(jp)) continue;
        fn(entry.path(), entry.path().filename().string());
    }
}
