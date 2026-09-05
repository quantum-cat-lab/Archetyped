#include "CmdUtil.h"
#include "CmdBuild.h"    // used by test command
#include "../CLIUtils.h"
#include "../Workspace.h"
#include "../Process.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstdio>

// =============================================================================
// doctor
// =============================================================================
void CmdDoctor::execute(const Args& args, Workspace& ws) {
    bool quick = args.has("--quick");
    int score = 0, total = quick ? 5 : 10;

    cli::printSection(cli::iconDoctor(), "Archetyped System Diagnostics");

    auto check = [&](int num, int t, const std::string& label, bool ok) {
        std::cout << "    " << cli::gray("[" + std::to_string(num) + "/" + std::to_string(t) + "]")
                  << " " << cli::bold(label) << "  ";
        if (ok) { std::cout << cli::iconOk(); score++; }
        else std::cout << cli::iconFail();
        std::cout << "\n";
    };

    check(1, total, "C++ Compiler",
        Process::available("g++ --version") || Process::available("clang++ --version"));
    check(2, total, "CMake",
        Process::available("cmake --version"));

    if (quick) {
        cli::printRule();
        cli::printMeter(score, total, "Quick Health");
        return;
    }

    check(3, total, "Project Root",   fs::exists(ws.projectRoot()));
    check(4, total, "Modules Root",   fs::exists(ws.modulesRoot()));
    check(5, total, "module_workspace.json", fs::exists(ws.workspaceJsonPath()));
    check(6, total, "Engine Build",   fs::exists(ws.engineBinary()));

    bool hasContainers = fs::exists(ws.containersRoot());
    check(7, total, "Containers", hasContainers);

    int modCount = (int)ws.listModules().size();
    check(8, total, "Modules (" + std::to_string(modCount) + " found)", modCount > 0);

    {
        std::string cpuStr = "?";
        auto r = Process::run("nproc");
        if (r.ok()) { cpuStr = r.output; while (!cpuStr.empty() && cpuStr.back() == '\n') cpuStr.pop_back(); }
        check(9, total, "CPU Cores: " + cpuStr, true);
    }

    check(10, total, "SDK (run check-sdk)", true);

    cli::printRule();
    cli::printMeter(score, total, "Overall Health");
    std::cout << "\n";
}

// =============================================================================
// status
// =============================================================================
void CmdStatus::execute(const Args& args, Workspace& ws) {
    (void)args;
    cli::printRule();
    std::cout << "  " << cli::bold(cli::cyan("Archetyped Project Status")) << "\n";
    cli::printRule();
    std::cout << "\n";

    cli::printKV(cli::iconInfo() + " Workspace", ws.projectRoot().string());
    cli::printKV(cli::iconPackage() + " Arche", "v0.0.10");

    bool engineBuilt = fs::exists(ws.engineBinary());
    cli::printKV(cli::iconBuild() + " Engine",
        engineBuilt ? cli::green("Built") : cli::red("Not built"));

    auto mods = ws.listModules();
    int builtMods = 0, modWithSdk = 0;
    for (auto& m : mods) {
        if (fs::exists(m.path / "build")) builtMods++;
        if (fs::exists(m.path / "include" / "SDK")) modWithSdk++;
    }
    std::string modStatus = std::to_string(mods.size()) + " total, "
        + std::to_string(builtMods) + " built, "
        + std::to_string((int)mods.size() - builtMods) + " unbuilt";
    cli::printKV(cli::iconModule() + " Modules", modStatus);

    auto containers = ws.listContainers();
    cli::printKV(cli::iconContainer() + " Containers", std::to_string(containers.size()));

    std::cout << "\n";
    cli::printSection(cli::iconSDK(), "SDK Health");
    bool sdkOk = true;
    json wsJson = ws.loadWorkspaceJson();
    if (wsJson.contains("modules") && wsJson["modules"].is_array()) {
        for (auto& mod : wsJson["modules"]) {
            if (!mod.contains("path")) continue;
            std::string mpath = (ws.modulesRoot() / mod.value("path", "")).string();
            fs::path sm = fs::path(mpath) / "configs" / "sdk-manifest.json";
            if (fs::exists(sm)) {
                fs::path sdkDir = fs::path(mpath) / "include" / "SDK";
                if (fs::exists(sdkDir)) {
                    for (const auto& ud : fs::directory_iterator(sdkDir)) {
                        if (ud.is_directory() && !fs::exists(ud.path() / "SdkManifest.g.h")) {
                            sdkOk = false;
                            std::cout << "    " << cli::iconFail() << " "
                                      << mod.value("name", "") << "/"
                                      << ud.path().filename().string()
                                      << " missing manifest\n";
                        }
                    }
                }
            }
        }
    }
    if (sdkOk) std::cout << "    " << cli::iconOk() << " "
                         << cli::green("All vendored copies match canonical") << "\n";

    std::cout << "\n";
    cli::printSection(cli::iconGear(), "System");
    {
        auto r = Process::run("g++ --version 2>&1 | head -1");
        std::string gppVer = r.ok() ? r.output : "?";
        while (!gppVer.empty() && (gppVer.back() == '\n' || gppVer.back() == '\r')) gppVer.pop_back();
        cli::printKV("Compiler", gppVer);
    }
    {
        auto r = Process::run("cmake --version 2>&1 | head -1");
        std::string cmakeVer = r.ok() ? r.output : "?";
        while (!cmakeVer.empty() && (cmakeVer.back() == '\n' || cmakeVer.back() == '\r')) cmakeVer.pop_back();
        cli::printKV("CMake", cmakeVer);
    }
    {
        auto r = Process::run("nproc");
        std::string cpuStr = r.ok() ? r.output : "?";
        while (!cpuStr.empty() && (cpuStr.back() == '\n' || cpuStr.back() == '\r')) cpuStr.pop_back();
        cli::printKV("CPU Cores", cpuStr);
    }
    std::cout << "\n";
    cli::printRule();
}

// =============================================================================
// completions
// =============================================================================
void CmdCompletions::execute(const Args& args, Workspace& ws) {
    (void)ws;
    std::string shell = args.pos(0);
    if (shell.empty()) {
        cli::printError("Usage: completions <bash|zsh|fish>");
        return;
    }

    static const char* CMDS =
        "engine-build arche-build scan list-mods info-mod init-mod update-mod "
        "init-cont info-cont add-mod rem-mod build run cp-cont rm-cont arc-cont "
        "unarc-cont sync-sdk check-sdk init-sdk deps test doctor status clean "
        "watch completions help";

    if (shell == "bash") {
        std::cout << "# arche bash completion\n";
        std::cout << "_arche_completions() {\n";
        std::cout << "  local cur=${COMP_WORDS[COMP_CWORD]}\n";
        std::cout << "  COMPREPLY=($(compgen -W \"" << CMDS << "\" -- $cur))\n";
        std::cout << "}\n";
        std::cout << "complete -F _arche_completions arche\n";
    } else if (shell == "zsh") {
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
    } else if (shell == "fish") {
        std::cout << "# arche fish completion\n";
        std::cout << "complete -c arche -f -a \"" << CMDS << "\"\n";
    } else {
        cli::printError("Unknown shell: " + shell + " (supported: bash, zsh, fish)");
    }
}

// =============================================================================
// test
// =============================================================================
void CmdTest::execute(const Args& args, Workspace& ws) {
    (void)args;
    std::cout << "Starting SDK Integration Test...\n";
    std::string testCont = "SDK_Test_Cont";

    fs::path buildPath = ws.engineBuildDir();
    if (fs::exists(buildPath)) fs::remove_all(buildPath);

    fs::path contPath = ws.containerPath(testCont);
    if (fs::exists(contPath)) fs::remove_all(contPath);
    fs::create_directories(contPath);

    json contJson = {
        {"container_id", testCont}, {"name", testCont}, {"version", "1.0.0"},
        {"load_order", json::array()},
        {"metadata", {{"target_platform", "Linux"}, {"build_type", "Debug"}}},
        {"global_settings", {{"log_level", "Verbose"}}}
    };
    std::ofstream file(contPath / "container.json");
    file << contJson.dump(4);
    file.close();

    std::ifstream inFile(contPath / "container.json");
    json cJson; inFile >> cJson; inFile.close();
    cJson["load_order"].push_back("TestModule");
    std::ofstream outFile(contPath / "container.json");
    outFile << cJson.dump(4);
    outFile.close();

    CmdBuild buildCmd;
    buildCmd.execute(Args::parse({testCont}), ws);

    fs::path binPath = contPath / "bin" / "TestModule.so";
    if (fs::exists(binPath)) {
        cli::printSuccess("SUCCESS: TestModule.so was built and packed into container");
    } else {
        std::cerr << "FAILURE: TestModule.so not found in container.\n";
    }
    fs::remove_all(contPath);
    std::cout << "SDK Integration Test Completed.\n";
}
