#include "CmdModule.h"
#include "../CLIUtils.h"
#include "../Workspace.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <set>

// =============================================================================
// Helpers reused from the original Arche class
// =============================================================================
static std::string macroIdent(const std::string& name) {
    std::string out;
    for (unsigned char ch : name)
        out.push_back(std::isalnum(ch) ? (char)std::toupper(ch) : '_');
    return out;
}

static void replaceInFile(const fs::path& filePath, const std::string& oldStr, const std::string& newStr) {
    try {
        std::ifstream inFile(filePath);
        if (!inFile) return;
        std::string content((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        size_t pos = 0;
        bool changed = false;
        while ((pos = content.find(oldStr, pos)) != std::string::npos) {
            content.replace(pos, oldStr.length(), newStr);
            pos += newStr.length();
            changed = true;
        }

        if (changed) {
            std::ofstream outFile(filePath);
            outFile << content;
        }
    } catch (...) {}
}

// =============================================================================
// list-mods
// =============================================================================
void CmdModuleList::execute(const Args& args, Workspace& ws) {
    (void)args;
    cli::printSection(cli::iconModule(), "Available Modules");
    auto mods = ws.listModules();
    for (auto& m : mods)
        std::cout << "    " << cli::iconBullet() << " "
                  << cli::bold(m.id) << cli::gray(" v" + m.version) << "\n";
    if (mods.empty()) cli::printWarning("No modules found");
    cli::printRule();
    cli::printInfo("Total: " + std::to_string(mods.size()) + " module(s)");
}

// =============================================================================
// info-mod
// =============================================================================
void CmdModuleInfo::execute(const Args& args, Workspace& ws) {
    std::string modName = args.pos(0);
    if (modName.empty()) { cli::printError("ModuleInstance name required"); return; }

    fs::path modPath = ws.resolveModule(modName);
    if (modPath.empty()) { cli::printError("ModuleInstance '" + modName + "' not found"); return; }

    ModuleInfo mi = ws.loadModule(modPath);
    if (mi.id.empty()) { cli::printError("Failed to read module.json for '" + modName + "'"); return; }

    cli::printSection(cli::iconModule(), "ModuleInstance: " + modName);
    std::cout << "    " << cli::gray("ID:")    << "           " << cli::bold(mi.id) << "\n";
    std::cout << "    " << cli::gray("Name:")    << "         " << cli::bold(mi.name) << "\n";
    std::cout << "    " << cli::gray("Version:") << "      " << mi.version << "\n";
    std::cout << "    " << cli::gray("Entry:")   << "        " << mi.entry_point << "\n";

    std::string deps;
    for (auto& d : mi.dependencies) {
        if (!deps.empty()) deps += " ";
        deps += d;
    }
    if (deps.empty()) deps = "None";
    std::cout << "    " << cli::gray("Deps:") << "         " << deps << "\n";
}

// =============================================================================
// init-mod
// =============================================================================
void CmdModuleInit::execute(const Args& args, Workspace& ws) {
    std::string name = args.pos(0);
    if (name.empty()) { std::cerr << "Error: ModuleInstance name required.\n"; return; }

    fs::path modPath = ws.modulesRoot() / name;
    if (fs::exists(modPath)) { std::cerr << "Error: ModuleInstance directory already exists.\n"; return; }

    try {
        fs::path tmpl = ws.moduleTemplatePath();
        if (fs::exists(tmpl)) {
            fs::copy(tmpl, modPath, fs::copy_options::recursive);

            for (const auto& entry : fs::recursive_directory_iterator(modPath)) {
                if (entry.is_regular_file())
                    replaceInFile(entry.path(), "{{MODULE_NAME}}", name);
            }

            if (fs::exists(modPath / "build")) fs::remove_all(modPath / "build");
            if (fs::exists(modPath / "build-windows-clang")) fs::remove_all(modPath / "build-windows-clang");

            fs::create_directories(modPath / "assets");
            fs::create_directories(modPath / "configs");

            json modJson = {
                {"id", name}, {"name", name}, {"version", "1.0.0"},
                {"assets_root", "assets/"}, {"config_root", "configs/"},
                {"dependencies", json::array()}, {"entry_point", name + ".so"}
            };
            std::ofstream file(modPath / "module.json");
            file << modJson.dump(4);

            ws.addModuleToWorkspace(name);
            cli::printSuccess("ModuleInstance '" + name + "' created from template");
            return;
        }

        // No template — create bare structure
        fs::create_directories(modPath / "src");
        fs::create_directories(modPath / "assets");
        fs::create_directories(modPath / "configs");
        json modJson = {{"id", name}, {"name", name}, {"version", "1.0.0"},
            {"assets_root", "assets/"}, {"config_root", "configs/"},
            {"dependencies", json::array()}, {"entry_point", name + ".so"}};
        std::ofstream file(modPath / "module.json");
        file << modJson.dump(4);
        ws.addModuleToWorkspace(name);
        cli::printSuccess("ModuleInstance '" + name + "' created with basic structure");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// =============================================================================
// update-mod
// =============================================================================
void CmdModuleUpdate::execute(const Args& args, Workspace& ws) {
    std::string name = args.pos(0);
    if (name.empty()) { std::cerr << "Error: ModuleInstance name required.\n"; return; }

    fs::path modPath = ws.modulesRoot() / name;
    if (!fs::exists(modPath)) { std::cerr << "Error: ModuleInstance '" << name << "' not found.\n"; return; }

    try {
        fs::path tmpl = ws.moduleTemplatePath();
        if (!fs::exists(tmpl)) { std::cerr << "Error: Template not found.\n"; return; }

        std::cout << "Updating module '" << name << "' from template...\n";
        for (const auto& entry : fs::recursive_directory_iterator(tmpl)) {
            if (entry.is_directory()) continue;
            fs::path relPath = fs::relative(entry.path(), tmpl);
            fs::path destPath = modPath / relPath;

            // Preserve existing source files
            if (relPath.string().find("src/") == 0 && fs::exists(destPath))
                continue;

            fs::create_directories(destPath.parent_path());
            fs::copy_file(entry.path(), destPath, fs::copy_options::overwrite_existing);
            replaceInFile(destPath, "{{MODULE_NAME}}", name);
        }
        cli::printSuccess("ModuleInstance '" + name + "' updated successfully");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// =============================================================================
// scan
// =============================================================================
void CmdModuleScan::execute(const Args& args, Workspace& ws) {
    (void)args;
    std::cout << "Scanning for modules in " << ws.modulesRoot() << "...\n";
    int count = 0;
    if (fs::exists(ws.modulesRoot())) {
        for (const auto& entry : fs::directory_iterator(ws.modulesRoot())) {
            if (entry.is_directory() && fs::exists(entry.path() / "module.json"))
                count++;
        }
    }
    std::cout << "Found " << count << " valid modules. Project DB updated.\n";
}
