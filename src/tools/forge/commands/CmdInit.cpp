#include "CmdInit.h"
#include "../CLIUtils.h"
#include "../Workspace.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Discoverable engine services (Tier-1 host/scheduler singletons). Only these
// are exposed as workspace-level "enabled systems" — Tier-2/3 (per-domain
// POJOs, factories) come up automatically with their Tier-1 host.
static const std::vector<std::string>& knownSystems() {
    static const std::vector<std::string> v = {
        "ECS", "Event", "DataBase", "VFS", "workScheduler",
        "SmartScheduler", "SystemExecution", "Clock", "ModuleLoader",
        "ContainerLoader", "SchemaRegistry",
        "ClrHost", "ClrScheduler", "ClrRegistry",
        "JvmHost", "JvmScheduler", "JvmRegistry",
        "KotlinRuntime", "CSharpRuntime",
        "BridgeRegistry", "MutexRegistry", "MemoryAlloc",
    };
    return v;
}

// Writes a JSON file with an empty object if the file does not exist.
// Returns true if a new file was created, false if it already existed.
static bool writeIfMissing(const fs::path& p, const std::string& content) {
    if (fs::exists(p)) return false;
    fs::create_directories(p.parent_path());
    std::ofstream f(p);
    f << content;
    return true;
}

int CmdInit::execute(const Args& args, Workspace& ws) {
    bool reset = args.has("--reset");
    bool quiet = args.has("--quiet") || args.has("-q");

    if (ws.projectRoot().empty()) {
        std::cerr << cli::red("✘") << " Not in an Archetyped project (no CMakeLists.txt + container/ above cwd).\n";
        std::cerr << "  " << cli::gray("cd into the project root and retry") << "\n";
        return 1;
    }

    auto root = ws.projectRoot();
    auto configsDir = root / "configs";
    fs::create_directories(configsDir);

    int created = 0, kept = 0;

    // 1. active_container.json — points at the default 'test' container.
    auto ac = configsDir / "active_container.json";
    {
        bool wasNew = !fs::exists(ac) || reset;
        if (wasNew) {
            std::ofstream f(ac);
            f << "{\n    \"active_container\": \"test\"\n}\n";
            created++;
            if (!quiet) std::cout << cli::iconBuild() << " " << cli::bold("active_container.json") << "  " << cli::green("created") << "\n";
        } else {
            kept++;
            if (!quiet) std::cout << cli::iconOk() << " " << cli::bold("active_container.json") << "  " << cli::gray("kept") << "\n";
        }
    }

    // 2. module_workspace.json — discover sibling module dirs and engine components.
    auto mw = configsDir / "module_workspace.json";
    {
        std::vector<std::pair<std::string, std::string>> modules; // {name, path}
        if (fs::exists(ws.modulesRoot())) {
            for (auto& e : fs::directory_iterator(ws.modulesRoot())) {
                if (!e.is_directory()) continue;
                if (!fs::exists(e.path() / "module.json")) continue;
                std::string name = e.path().filename().string();
                modules.emplace_back(name, name);
            }
        }
        std::sort(modules.begin(), modules.end());

        std::set<std::string> sysSet(knownSystems().begin(), knownSystems().end());
        std::vector<std::string> enabled;
        auto comps = root / "src" / "engine" / "components";
        if (fs::exists(comps)) {
            for (auto& e : fs::directory_iterator(comps)) {
                if (!e.is_directory()) continue;
                std::string name = e.path().filename().string();
                if (sysSet.count(name)) enabled.push_back(name);
            }
        }
        std::sort(enabled.begin(), enabled.end());

        std::ofstream f(mw);
        f << "{\n  \"modules\": [\n";
        for (size_t i = 0; i < modules.size(); ++i) {
            f << "    { \"name\": \"" << modules[i].first
              << "\", \"path\": \"" << modules[i].second << "\" }"
              << (i + 1 < modules.size() ? "," : "") << "\n";
        }
        f << "  ],\n  \"enabled_systems\": [\n";
        for (size_t i = 0; i < enabled.size(); ++i) {
            f << "    \"" << enabled[i] << "\"" << (i + 1 < enabled.size() ? "," : "") << "\n";
        }
        f << "  ]\n}\n";
        created++;
        if (!quiet) std::cout << cli::iconBuild() << " " << cli::bold("module_workspace.json")
                              << "  " << cli::green("created")
                              << "  " << cli::gray("(" + std::to_string(modules.size()) + " modules, "
                                                  + std::to_string(enabled.size()) + " systems)")
                              << "\n";
    }

    // 3. sdk-manifest.json — only create skeleton if missing.
    auto sdk = configsDir / "sdk-manifest.json";
    {
        bool wasNew = !fs::exists(sdk) || reset;
        if (wasNew) {
            std::ofstream f(sdk);
            f << "{\n  \"provides\": [],\n  \"requires\": []\n}\n";
            created++;
            if (!quiet) std::cout << cli::iconBuild() << " " << cli::bold("sdk-manifest.json") << "  " << cli::green("created") << "\n";
        } else {
            kept++;
            if (!quiet) std::cout << cli::iconOk() << " " << cli::bold("sdk-manifest.json") << "  " << cli::gray("kept") << "\n";
        }
    }

    if (!quiet) {
        std::cout << "\n";
        cli::printRule();
        std::cout << "  " << cli::iconOk() << " " << cli::bold("Workspace initialized")
                  << "  " << cli::gray("(" + std::to_string(created) + " created, "
                                       + std::to_string(kept) + " kept)")
                  << "\n";
        std::cout << "  " << cli::gray("Root: ") << root.string() << "\n";
        cli::printRule();
    }
    return 0;
}
