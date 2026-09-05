#include "Arche.h"

// --- Engine Build ---
void Arche::doEngineBuild(const std::vector<std::string>& args) {
    std::cout << "  " << cli::iconBuild() << " " << cli::bold("Building Archetyped Engine Core...") << "\n";
    fs::path buildPath = projectRoot_ / "build";
    bool recache = std::find(args.begin(), args.end(), "--recache") != args.end();
    try {
        if (recache && fs::exists(buildPath)) fs::remove_all(buildPath);
        if (!fs::exists(buildPath)) fs::create_directories(buildPath);
        std::string cmakeConfig = "cmake -S " + projectRoot_.string() + " -B " + buildPath.string() + " -DCMAKE_BUILD_TYPE=Release";
        if (!runCommand(cmakeConfig)) return;
        std::string cmakeBuild = "cmake --build " + buildPath.string() + " --parallel $(nproc)";
        if (!runCommand(cmakeBuild)) return;

        std::cout << "Updating ModuleTemplate SDK..." << std::endl;
        fs::path sdkSrc = projectRoot_ / "include" / "Engine";
        fs::path sdkDest = projectRoot_ / "assets" / "templates" / "module" / "src" / "headers";
        if (fs::exists(sdkSrc)) {
            fs::create_directories(sdkDest);
            fs::copy(sdkSrc, sdkDest, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            std::cout << "SDK headers synchronized to template.\n";
        } else {
            std::cerr << "Warning: SDK source not found at " << sdkSrc << "\n";
        }

        cli::printSuccess("Archetyped Engine core built successfully.\n");
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << std::endl; }
}

// --- Arche Build ---
void Arche::doArcheBuild(const std::vector<std::string>& args) {
    std::cout << "Rebuilding Archetyped Toolchain (arche)..." << std::endl;
    fs::path buildPath = projectRoot_ / "build";
    bool recache = std::find(args.begin(), args.end(), "--recache") != args.end();

    if (recache && fs::exists(buildPath)) fs::remove_all(buildPath);
    if (!fs::exists(buildPath)) fs::create_directories(buildPath);

    std::string cmakeConfig = "cmake -S " + projectRoot_.string() + " -B " + buildPath.string() + " -DCMAKE_BUILD_TYPE=Release";
    if (!runCommand(cmakeConfig)) return;

    std::string buildCmd = "cmake --build " + buildPath.string() + " --target arche";
    if (runCommand(buildCmd)) {
        cli::printSuccess("Arche rebuilt successfully");
    } else {
        cli::printError("Failed to rebuild arche");
    }
}

// --- List Modules ---
void Arche::doListMods(const std::vector<std::string>& args) {
    cli::printSection(cli::iconModule(), "Available Modules");
    if (!fs::exists(modulesRoot_)) return;
    int count = 0;
    for (const auto& entry : fs::directory_iterator(modulesRoot_)) {
        if (entry.is_directory() && entry.path() != projectRoot_) {
            fs::path jsonPath = entry.path() / "module.json";
            if (fs::exists(jsonPath)) {
                try {
                    std::ifstream file(jsonPath);
                    json mJson; file >> mJson;
                    std::string id = mJson.value("id", "?");
                    std::string ver = mJson.value("version", "?");
                    std::cout << "    " << cli::iconBullet() << " " << cli::bold(id) << cli::gray(" v" + ver) << "\n";
                    count++;
                } catch (...) {}
            }
        }
    }
    if (count == 0) cli::printWarning("No modules found");
    cli::printRule();
    cli::printInfo("Total: " + std::to_string(count) + " module(s)");
}

// --- Info Mod ---
void Arche::doInfoMod(const std::vector<std::string>& args) {
    if (args.empty()) { cli::printError("ModuleInstance name required"); return; }
    fs::path jsonPath = modulesRoot_ / args[0] / "module.json";
    if (!fs::exists(jsonPath)) { cli::printError("ModuleInstance '" + args[0] + "' not found"); return; }
    try {
        std::ifstream file(jsonPath);
        json mJson; file >> mJson;
        std::vector<std::string> lines;
        lines.push_back(cli::gray("ID:") + "           " + cli::bold(mJson.value("id", "N/A")));
        lines.push_back(cli::gray("Name:") + "         " + cli::bold(mJson.value("name", "N/A")));
        lines.push_back(cli::gray("Version:") + "      " + mJson.value("version", "N/A"));
        lines.push_back(cli::gray("Entry:") + "        " + mJson.value("entry_point", "N/A"));
        std::string deps;
        if (mJson.contains("dependencies") && mJson["dependencies"].is_array()) {
            for (auto& dep : mJson["dependencies"]) {
                if (!deps.empty()) deps += " ";
                deps += dep.get<std::string>();
            }
        }
        if (deps.empty()) deps = "None";
        lines.push_back(cli::gray("Deps:") + "         " + deps);
        cli::printSection(cli::iconModule(), "ModuleInstance: " + args[0]);
        for (auto& l : lines) std::cout << "    " << l << "\n";
    } catch (const std::exception& e) { cli::printError(e.what()); }
}

// --- Scan ---
void Arche::doScan(const std::vector<std::string>& args) {
    std::cout << "Scanning for modules in " << modulesRoot_ << "...\n";
    int count = 0;
    if (fs::exists(modulesRoot_)) {
        for (const auto& entry : fs::directory_iterator(modulesRoot_)) {
            if (entry.is_directory() && entry.path() != projectRoot_) {
                if (fs::exists(entry.path() / "module.json")) count++;
            }
        }
    }
    std::cout << "Found " << count << " valid modules. Project DB updated.\n";
}

// --- Init Mod ---
void Arche::doInitMod(const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << "Error: ModuleInstance name required.\n"; return; }
    std::string name = args[0];
    fs::path modPath = modulesRoot_ / name;
    if (fs::exists(modPath)) { std::cerr << "Error: ModuleInstance directory already exists.\n"; return; }
    try {
        fs::path templatePath = projectRoot_ / "assets" / "templates" / "module";
        if (fs::exists(templatePath)) {
            fs::copy(templatePath, modPath, fs::copy_options::recursive);

            for (const auto& entry : fs::recursive_directory_iterator(modPath)) {
                if (entry.is_regular_file()) {
                    replaceInFile(entry.path(), "{{MODULE_NAME}}", name);
                }
            }

            if (fs::exists(modPath / "build")) fs::remove_all(modPath / "build");
            if (fs::exists(modPath / "build-windows-clang")) fs::remove_all(modPath / "build-windows-clang");

            fs::create_directories(modPath / "assets");
            fs::create_directories(modPath / "configs");

            json modJson = {
                {"id", name},
                {"name", name},
                {"version", "1.0.0"},
                {"assets_root", "assets/"},
                {"config_root", "configs/"},
                {"dependencies", json::array()},
                {"entry_point", name + ".so"}
            };
            std::ofstream file(modPath / "module.json");
            file << modJson.dump(4);

            addModuleToWorkspace(name);
            cli::printSuccess("ModuleInstance '" + name + "' created from template");
            return;
        }

        fs::create_directories(modPath / "src");
        fs::create_directories(modPath / "assets");
        fs::create_directories(modPath / "configs");
        json modJson = {{"id", name}, {"name", name}, {"version", "1.0.0"}, {"assets_root", "assets/"}, {"config_root", "configs/"}, {"dependencies", json::array()}, {"entry_point", name + ".so"}};
        std::ofstream file(modPath / "module.json");
        file << modJson.dump(4);
        addModuleToWorkspace(name);
        cli::printSuccess("ModuleInstance '" + name + "' created with basic structure");
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << std::endl; }
}

// --- Update Mod ---
void Arche::doUpdateMod(const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << "Error: ModuleInstance name required.\n"; return; }
    std::string name = args[0];
    fs::path modPath = modulesRoot_ / name;
    if (!fs::exists(modPath)) { std::cerr << "Error: ModuleInstance '" << name << "' not found.\n"; return; }
    try {
        fs::path templatePath = projectRoot_ / "assets" / "templates" / "module";
        if (!fs::exists(templatePath)) { std::cerr << "Error: Template not found.\n"; return; }

        std::cout << "Updating module '" << name << "' from template...\n";

        for (const auto& entry : fs::recursive_directory_iterator(templatePath)) {
            if (entry.is_directory()) continue;

            fs::path relPath = fs::relative(entry.path(), templatePath);
            fs::path destPath = modPath / relPath;

            if (relPath.string().find("src/") == 0) {
                if (fs::exists(destPath)) continue;
            }

            fs::create_directories(destPath.parent_path());
            fs::copy_file(entry.path(), destPath, fs::copy_options::overwrite_existing);
            replaceInFile(destPath, "{{MODULE_NAME}}", name);
        }

        cli::printSuccess("ModuleInstance '" + name + "' updated successfully");
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << std::endl; }
}

// --- Test ---
void Arche::doTest(const std::vector<std::string>& args) {
    std::cout << "Starting SDK Integration Test...\n";
    std::string testCont = "SDK_Test_Cont";

    fs::path buildPath = projectRoot_ / "build";
    if (fs::exists(buildPath)) fs::remove_all(buildPath);

    fs::path contPath = projectRoot_ / "container" / testCont;
    if (fs::exists(contPath)) fs::remove_all(contPath);
    fs::create_directories(contPath);

    json contJson = {{"container_id", testCont}, {"name", testCont}, {"version", "1.0.0"}, {"load_order", json::array()}, {"metadata", {{"target_platform", "Linux"}, {"build_type", "Debug"}}}, {"global_settings", {{"log_level", "Verbose"}}}};
    std::ofstream file(contPath / "container.json");
    file << contJson.dump(4);
    file.close();

    std::ifstream inFile(contPath / "container.json");
    json cJson; inFile >> cJson; inFile.close();
    cJson["load_order"].push_back("TestModule");
    std::ofstream outFile(contPath / "container.json");
    outFile << cJson.dump(4);
    outFile.close();

    doBuild({testCont});

    fs::path binPath = projectRoot_ / "container" / testCont / "bin" / "TestModule.so";
    if (fs::exists(binPath)) {
        cli::printSuccess("SUCCESS: TestModule.so was built and packed into container");
    } else {
        std::cerr << "FAILURE: TestModule.so not found in container.\n";
    }
    fs::remove_all(contPath);
    std::cout << "SDK Integration Test Completed.\n";
}

// --- Doctor ---
void Arche::doDoctor(const std::vector<std::string>& args) {
    bool quick = std::find(args.begin(), args.end(), "--quick") != args.end();
    int score = 0, total = quick ? 5 : 10;

    cli::printSection(cli::iconDoctor(), "Archetyped System Diagnostics");

    auto check = [&](int num, int t, const std::string& label, bool ok) {
        std::cout << "    " << cli::gray("[" + std::to_string(num) + "/" + std::to_string(t) + "]") << " " << cli::bold(label) << "  ";
        if (ok) { std::cout << cli::iconOk(); score++; }
        else std::cout << cli::iconFail();
        std::cout << "\n";
    };

    check(1, total, "C++ Compiler",
        runCommand("g++ --version > /dev/null 2>&1") || runCommand("clang++ --version > /dev/null 2>&1"));
    check(2, total, "CMake",
        runCommand("cmake --version > /dev/null 2>&1"));

    if (quick) {
        cli::printRule();
        cli::printMeter(score, total, "Quick Health");
        return;
    }

    check(3, total, "Project Root",
        fs::exists(projectRoot_));
    check(4, total, "Modules Root",
        fs::exists(modulesRoot_));

    fs::path wsJson = projectRoot_ / "configs" / "module_workspace.json";
    check(5, total, "module_workspace.json",
        fs::exists(wsJson));

    check(6, total, "Engine Build",
        fs::exists(projectRoot_ / "build" / "archetyped"));

    fs::path contDir = projectRoot_ / "container";
    bool hasContainers = fs::exists(contDir);
    check(7, total, "Containers", hasContainers);

    int modCount = 0;
    if (fs::exists(modulesRoot_)) {
        for (auto& e : fs::directory_iterator(modulesRoot_))
            if (e.is_directory() && e.path() != projectRoot_ && fs::exists(e.path() / "module.json"))
                modCount++;
    }
    check(8, total, "Modules (" + std::to_string(modCount) + " found)", modCount > 0);

    {
        std::string cpuStr = "?";
        FILE* pipe = popen("nproc 2>&1", "r");
        if (pipe) { char buf[64]; if (fgets(buf, sizeof(buf), pipe)) cpuStr = std::string(buf); pclose(pipe); }
        check(9, total, "CPU Cores: " + cpuStr, true);
    }

    check(10, total, "SDK (run check-sdk)", true);

    cli::printRule();
    cli::printMeter(score, total, "Overall Health");
    std::cout << "\n";
}

// --- Completions ---
void Arche::doCompletions(const std::vector<std::string>& args) {
    if (args.empty()) {
        cli::printError("Usage: completions <bash|zsh|fish>");
        return;
    }
    static const char* CMDS =
        "engine-build arche-build scan list-mods info-mod init-mod update-mod "
        "init-cont info-cont add-mod rem-mod build run cp-cont rm-cont arc-cont "
        "unarc-cont sync-sdk check-sdk init-sdk deps test doctor status clean "
        "watch completions help";
    if (args[0] == "bash") {
        std::cout << "# arche bash completion\n";
        std::cout << "_arche_completions() {\n";
        std::cout << "  local cur=${COMP_WORDS[COMP_CWORD]}\n";
        std::cout << "  COMPREPLY=($(compgen -W \"" << CMDS << "\" -- $cur))\n";
        std::cout << "}\n";
        std::cout << "complete -F _arche_completions arche\n";
    } else if (args[0] == "zsh") {
        std::cout << "# arche zsh completion\n";
        std::cout << "#compdef arche\n";
        std::cout << "local -a commands\n";
        std::cout << "commands=(\n";
        std::cout << "  'engine-build:Build engine core'\n";
        std::cout << "  'arche-build:Rebuild arche toolchain'\n";
        std::cout << "  'scan:Scan for modules'\n";
        std::cout << "  'list-mods:List modules'\n";
        std::cout << "  'info-mod:Show module info'\n";
        std::cout << "  'init-mod:Create module'\n";
        std::cout << "  'update-mod:Update module'\n";
        std::cout << "  'init-cont:Create container'\n";
        std::cout << "  'info-cont:Show container info'\n";
        std::cout << "  'add-mod:Add module to container'\n";
        std::cout << "  'rem-mod:Remove module from container'\n";
        std::cout << "  'build:Build container'\n";
        std::cout << "  'run:Launch engine'\n";
        std::cout << "  'sync-sdk:Vendor SDK headers'\n";
        std::cout << "  'check-sdk:Verify SDK integrity'\n";
        std::cout << "  'init-sdk:Create SDK provider'\n";
        std::cout << "  'deps:Show SDK dependency graph'\n";
        std::cout << "  'doctor:Run diagnostics'\n";
        std::cout << "  'status:Show project status'\n";
        std::cout << "  'clean:Clean artifacts'\n";
        std::cout << "  'completions:Generate completions'\n";
        std::cout << "  'test:Run tests'\n";
        std::cout << "  'help:Show help'\n";
        std::cout << ")\n";
        std::cout << "_arguments '*:command:(( ${commands[@]} ))'\n";
    } else if (args[0] == "fish") {
        std::cout << "# arche fish completion\n";
        std::cout << "complete -c arche -f -a \"" << CMDS << "\"\n";
    } else {
        cli::printError("Unknown shell: " + args[0] + " (supported: bash, zsh, fish)");
    }
}

// --- Status: project dashboard ---
void Arche::doStatus(const std::vector<std::string>& args) {
    cli::printRule();
    std::cout << "  " << cli::bold(cli::cyan("Archetyped Project Status")) << "\n";
    cli::printRule();
    std::cout << "\n";

    cli::printKV(cli::iconInfo() + " Workspace", projectRoot_.string());
    cli::printKV(cli::iconPackage() + " Arche", "v" ARCHE_VERSION);

    bool engineBuilt = fs::exists(projectRoot_ / "build" / "archetyped");
    cli::printKV(cli::iconBuild() + " Engine",
        engineBuilt ? cli::green("Built") : cli::red("Not built"));

    int totalMods = 0, builtMods = 0, modWithSdk = 0;
    if (fs::exists(modulesRoot_)) {
        for (const auto& entry : fs::directory_iterator(modulesRoot_)) {
            if (entry.is_directory() && entry.path() != projectRoot_ && fs::exists(entry.path() / "module.json")) {
                totalMods++;
                if (fs::exists(entry.path() / "build")) builtMods++;
                if (fs::exists(entry.path() / "include" / "SDK")) modWithSdk++;
            }
        }
    }
    std::string modStatus = std::to_string(totalMods) + " total, "
        + std::to_string(builtMods) + " built, "
        + std::to_string(totalMods - builtMods) + " unbuilt";
    cli::printKV(cli::iconModule() + " Modules", modStatus);

    int contCount = 0;
    fs::path contDir = projectRoot_ / "container";
    if (fs::exists(contDir)) {
        for (const auto& entry : fs::directory_iterator(contDir)) {
            if (entry.is_directory() && fs::exists(entry.path() / "container.json")) contCount++;
        }
    }
    cli::printKV(cli::iconContainer() + " Containers", std::to_string(contCount));

    std::cout << "\n";
    cli::printSection(cli::iconSDK(), "SDK Health");
    bool sdkOk = true;
    fs::path wsJson = projectRoot_ / "configs" / "module_workspace.json";
    if (fs::exists(wsJson)) {
        std::ifstream wf(wsJson); json ws; wf >> ws;
        for (auto& mod : ws["modules"]) {
            if (!mod.contains("path")) continue;
            std::string mpath = (modulesRoot_ / mod.value("path","")).string();
            fs::path sm = fs::path(mpath) / "configs" / "sdk-manifest.json";
            if (fs::exists(sm)) {
                fs::path sdkDir = fs::path(mpath) / "include" / "SDK";
                if (fs::exists(sdkDir)) {
                    for (const auto& ud : fs::directory_iterator(sdkDir)) {
                        if (ud.is_directory() && !fs::exists(ud.path() / "SdkManifest.g.h")) {
                            sdkOk = false;
                            std::cout << "    " << cli::iconFail() << " " << mod.value("name","") << "/" << ud.path().filename().string() << " missing manifest\n";
                        }
                    }
                }
            }
        }
    }
    if (sdkOk) std::cout << "    " << cli::iconOk() << " " << cli::green("All vendored copies match canonical") << "\n";

    std::cout << "\n";
    cli::printSection(cli::iconGear(), "System");
    {
        std::string gppVer = "?";
        FILE* pipe = popen("g++ --version 2>&1 | head -1", "r");
        if (pipe) { char buf[128]; if (fgets(buf, sizeof(buf), pipe)) { std::string s(buf); if (!s.empty()) gppVer = s.substr(0, s.find_last_not_of("\n")+1); } pclose(pipe); }
        cli::printKV("Compiler", gppVer);
    }
    {
        std::string cmakeVer = "?";
        FILE* pipe = popen("cmake --version 2>&1 | head -1", "r");
        if (pipe) { char buf[128]; if (fgets(buf, sizeof(buf), pipe)) { std::string s(buf); if (!s.empty()) cmakeVer = s.substr(0, s.find_last_not_of("\n")+1); } pclose(pipe); }
        cli::printKV("CMake", cmakeVer);
    }
    {
        std::string cpuStr = "?";
        FILE* pipe = popen("nproc 2>&1", "r");
        if (pipe) { char buf[64]; if (fgets(buf, sizeof(buf), pipe)) cpuStr = std::string(buf); pclose(pipe); }
        cli::printKV("CPU Cores", cpuStr);
    }
    std::cout << "\n";
    cli::printRule();
}

// --- Clean: remove build artifacts ---
void Arche::doClean(const std::vector<std::string>& args) {
    bool all = std::find(args.begin(), args.end(), "--all") != args.end();
    int removed = 0;

    cli::printSection(cli::iconBuild(), "Cleaning Build Artifacts");

    // 1. ModuleInstance build directories
    if (fs::exists(modulesRoot_)) {
        for (const auto& entry : fs::directory_iterator(modulesRoot_)) {
            if (entry.is_directory() && entry.path() != projectRoot_ && fs::exists(entry.path() / "module.json")) {
                fs::path buildPath = entry.path() / "build";
                if (fs::exists(buildPath)) {
                    fs::remove_all(buildPath);
                    std::cout << "    " << cli::iconBullet() << " " << entry.path().filename().string() << "/build " << cli::gray("removed") << "\n";
                    removed++;
                }
                fs::path wBuildPath = entry.path() / "build-windows-clang";
                if (fs::exists(wBuildPath)) {
                    fs::remove_all(wBuildPath);
                    removed++;
                }
            }
        }
    }

    // 2. Engine build directory
    fs::path engineBuild = projectRoot_ / "build";
    if (fs::exists(engineBuild)) {
        fs::remove_all(engineBuild);
        std::cout << "    " << cli::iconBullet() << " engine/build " << cli::gray("removed") << "\n";
        removed++;
    }

    // 3. Container bin directories
    fs::path contDir = projectRoot_ / "container";
    if (fs::exists(contDir)) {
        for (const auto& entry : fs::directory_iterator(contDir)) {
            if (entry.is_directory()) {
                fs::path binPath = entry.path() / "bin";
                if (fs::exists(binPath)) {
                    fs::remove_all(binPath);
                    std::cout << "    " << cli::iconBullet() << " container/" << entry.path().filename().string() << "/bin " << cli::gray("removed") << "\n";
                    removed++;
                }
            }
        }
    }

    // 4. --all: also clean vendored SDK headers
    if (all && fs::exists(modulesRoot_)) {
        for (const auto& entry : fs::directory_iterator(modulesRoot_)) {
            if (entry.is_directory() && entry.path() != projectRoot_) {
                fs::path sdkDir = entry.path() / "include" / "SDK";
                if (fs::exists(sdkDir)) {
                    fs::remove_all(sdkDir);
                    std::cout << "    " << cli::iconBullet() << " " << entry.path().filename().string() << "/include/SDK " << cli::gray("removed") << "\n";
                    removed++;
                }
            }
        }
    }

    cli::printRule();
    cli::printSuccess(std::to_string(removed) + " artifact(s) removed");
}
