#include "CmdBuild.h"
#include "CmdSDK.h"       // for sync-sdk call during build
#include "../CLIUtils.h"
#include "../Workspace.h"
#include "../Process.h"
#include "../SHA256.h"

#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>
#include <csignal>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <algorithm>

// =============================================================================
// Hash-based incremental check (moved from BuildCommands.cpp)
// =============================================================================
static std::string computeModuleSrcHash(const fs::path& modPath) {
    fs::path srcDir = modPath / "src";
    std::string combined;
    if (fs::exists(srcDir)) {
        std::vector<fs::path> files;
        for (auto& e : fs::recursive_directory_iterator(srcDir))
            if (e.is_regular_file()) {
                std::string ext = e.path().extension().string();
                if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp")
                    files.push_back(e.path());
            }
        std::sort(files.begin(), files.end());
        for (auto& f : files) {
            combined += f.string();
            combined += sha256_file(f);
        }
    }
    fs::path modJson = modPath / "module.json";
    if (fs::exists(modJson)) {
        combined += "module.json";
        combined += sha256_file(modJson);
    }
    fs::path cmakeFile = modPath / "CMakeLists.txt";
    if (fs::exists(cmakeFile)) {
        combined += "CMakeLists.txt";
        combined += sha256_file(cmakeFile);
    }
    return combined.empty() ? "" : sha256_hex(combined);
}

static std::string computeModuleCSharpHash(const fs::path& modPath) {
    std::string combined;
    for (auto& e : fs::recursive_directory_iterator(modPath)) {
        if (!e.is_regular_file()) continue;
        std::string ext = e.path().extension().string();
        if (ext == ".cs" || ext == ".csproj" || ext == ".sln") {
            combined += e.path().string();
            combined += sha256_file(e.path());
        }
    }
    return combined.empty() ? "" : sha256_hex(combined);
}

static fs::path findCSharpProj(const fs::path& modPath, const std::string& modId) {
    std::vector<fs::path> candidates = {
        modPath / (modId + ".csproj"),
        modPath / ("Arche." + modId + ".csproj"),
        modPath / "csharp" / (modId + ".csproj"),
        modPath / "csharp" / ("Arche." + modId + ".csproj"),
    };
    for (auto& p : candidates) if (fs::exists(p)) return p;
    for (auto& e : fs::directory_iterator(modPath))
        if (e.is_regular_file() && e.path().extension() == ".csproj") return e.path();
    fs::path csharpDir = modPath / "csharp";
    if (fs::exists(csharpDir))
        for (auto& e : fs::recursive_directory_iterator(csharpDir))
            if (e.is_regular_file() && e.path().extension() == ".csproj") return e.path();
    fs::path gradleDir = modPath / "src" / "main" / "csharp";
    if (fs::exists(gradleDir))
        for (auto& e : fs::recursive_directory_iterator(gradleDir))
            if (e.is_regular_file() && e.path().extension() == ".csproj") return e.path();
    return {};
}

// =============================================================================
// engine-build
// =============================================================================
int CmdEngineBuild::execute(const Args& args, Workspace& ws) {
    std::cout << "  " << cli::iconBuild() << " " << cli::bold("Building Archetyped Engine Core...") << "\n";
    fs::path buildPath = ws.engineBuildDir();
    bool recache = args.has("--recache");
    try {
        if (recache && fs::exists(buildPath)) fs::remove_all(buildPath);
        if (!fs::exists(buildPath)) fs::create_directories(buildPath);
        std::string cmakeConfig = "cmake -S " + ws.projectRoot().string() + " -B " + buildPath.string() + " -DCMAKE_BUILD_TYPE=Release";
        if (!Process::system(cmakeConfig)) return 1;
        int jobs = 2;
        try { int n = (int)std::thread::hardware_concurrency(); if (n > 0) jobs = std::min(n > 2 ? n - 2 : 2, 2); } catch(...) {}
        std::string cmakeBuild = "nice -n 10 cmake --build " + buildPath.string() + " --parallel " + std::to_string(jobs);
        if (!Process::system(cmakeBuild)) return 1;

        // Publish ManagedBootstrap (collectible ALC host) — unified with modules: src/main/csharp/
        {
            fs::path bsProj = ws.projectRoot() / "src" / "main" / "csharp" / "Bootstrap" / "Bootstrap.csproj";
            if (fs::exists(bsProj)) {
                std::cout << "  " << cli::iconBuild() << " Publishing ManagedBootstrap..." << "\n";
                std::string pub = "dotnet publish " + bsProj.string() + " -c Release -o " + buildPath.string();
                if (!Process::system(pub)) std::cerr << "Warning: dotnet publish Bootstrap failed\n";
                // normalize name: Bootstrap.dll -> ManagedBootstrap.dll for ClrHost compat
                try {
                    if (fs::exists(buildPath / "Bootstrap.dll") && !fs::exists(buildPath / "ManagedBootstrap.dll"))
                        fs::copy_file(buildPath / "Bootstrap.dll", buildPath / "ManagedBootstrap.dll", fs::copy_options::overwrite_existing);
                    if (fs::exists(buildPath / "Bootstrap.runtimeconfig.json") && !fs::exists(buildPath / "ManagedBootstrap.runtimeconfig.json"))
                        fs::copy_file(buildPath / "Bootstrap.runtimeconfig.json", buildPath / "ManagedBootstrap.runtimeconfig.json", fs::copy_options::overwrite_existing);
                } catch(...) {}
            }
        }

        std::cout << "Updating ModuleTemplate SDK..." << std::endl;
        fs::path sdkSrc = ws.projectRoot() / "include" / "Engine";
        fs::path sdkDest = ws.projectRoot() / "assets" / "templates" / "module" / "src" / "headers";
        if (fs::exists(sdkSrc)) {
            fs::create_directories(sdkDest);
            fs::copy(sdkSrc, sdkDest, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            std::cout << "SDK headers synchronized to template.\n";
        } else {
            std::cerr << "Warning: SDK source not found at " << sdkSrc << "\n";
        }
        cli::printSuccess("Archetyped Engine core built successfully.\n");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}

// =============================================================================
// forge-build
// =============================================================================
int CmdArcheBuild::execute(const Args& args, Workspace& ws) {
    std::cout << "Rebuilding forge toolchain..." << std::endl;
    fs::path buildPath = ws.engineBuildDir();
    bool recache = args.has("--recache");

    if (recache && fs::exists(buildPath)) fs::remove_all(buildPath);
    if (!fs::exists(buildPath)) fs::create_directories(buildPath);

    std::string cmakeConfig = "cmake -S " + ws.projectRoot().string() + " -B " + buildPath.string() + " -DCMAKE_BUILD_TYPE=Release";
    if (!Process::system(cmakeConfig)) return 1;

    if (Process::system("cmake --build " + buildPath.string() + " --target forge")) {
        cli::printSuccess("Arche rebuilt successfully");
    } else {
        cli::printError("Failed to rebuild forge");
    }
    return 0;
}

// =============================================================================
// build <container>
// =============================================================================
int CmdBuild::execute(const Args& args, Workspace& ws) {
    std::string name = args.pos(0);
    if (name.empty()) { std::cerr << "Error: Container name required.\n"; return 1; }

    bool updateAssets = args.has("--update-assets");
    bool srcIncluded  = args.has("--src-included");
    bool force        = args.has("--force") || args.has("-f");
    bool recache      = args.has("--recache");

    fs::path contPath = ws.containerPath(name);
    json contJson = ws.loadContainerJson(name);
    if (contJson.empty() || !contJson.contains("container_id")) {
        cli::printError("Container '" + name + "' not found");
        return 1;
    }

    if (!contJson.contains("preload") || !contJson["preload"].is_array())
        contJson["preload"] = json::array();
    bool hasNexus = false;
    for (auto& p : contJson["preload"])
        if (p.get<std::string>() == "NexusXCore") { hasNexus = true; break; }
    if (!hasNexus) {
        contJson["preload"].push_back("NexusXCore");
        ws.saveContainerJson(name, contJson);
    }

    auto collectMods = [&](const std::string& key) -> std::vector<std::string> {
        if (!contJson.contains(key) || !contJson[key].is_array()) return {};
        return contJson[key].get<std::vector<std::string>>();
    };

    std::vector<std::string> allMods;
    auto addUnique = [&](const std::string& m) {
        if (std::find(allMods.begin(), allMods.end(), m) == allMods.end())
            allMods.push_back(m);
    };
    for (auto& m : collectMods("preload")) addUnique(m);
    for (auto& m : collectMods("load_order")) addUnique(m);

    if (contJson.contains("stages")) {
        auto& stages = contJson["stages"];
        auto collectStageMods = [&](const std::string& key) -> std::vector<std::string> {
            if (!stages.contains(key) || !stages[key].is_array()) return {};
            return stages[key].get<std::vector<std::string>>();
        };
        for (auto& m : collectStageMods("stage0_boot")) addUnique(m);
        for (auto& m : collectStageMods("stage1_hosts")) addUnique(m);
        for (auto& m : collectStageMods("stage3_finalize")) addUnique(m);
        if (stages.contains("stage2_submodules") && stages["stage2_submodules"].is_object())
            for (auto& [hostId, subs] : stages["stage2_submodules"].items())
                for (auto& sub : subs)
                    addUnique(sub.get<std::string>());
    }

    fs::create_directories(contPath / "bin");
    fs::create_directories(contPath / "assets");
    fs::create_directories(contPath / "configs");
    if (srcIncluded) fs::create_directories(contPath / "src");

    auto processModule = [&](const std::string& modId, bool copyAssets) -> bool {
        fs::path modPath = ws.resolveModule(modId);
        if (modPath.empty()) {
            std::cerr << "Error: ModuleInstance '" << modId << "' not found.\n";
            return false;
        }
        ModuleInfo mi = ws.loadModule(modPath);
        if (mi.id.empty()) return false;

        fs::path buildPath = modPath / "build";
        bool hasCMake = fs::exists(modPath / "CMakeLists.txt");
        bool hasCSharp = !findCSharpProj(modPath, mi.id).empty();
        if (!hasCMake && hasCSharp) {
            // pure C# module — skip native build
        } else if (recache || !fs::exists(buildPath) || !fs::exists(buildPath / "CMakeCache.txt")) {
            if (recache && fs::exists(buildPath)) fs::remove_all(buildPath);
            if (recache) {
                fs::path cacheDir = modPath / ".build_cache";
                fs::path hashFile = cacheDir / ".forge_src_hash";
                if (fs::exists(hashFile)) fs::remove(hashFile);
            }
            fs::create_directories(buildPath);
            if (!Process::system("cmake -S " + modPath.string() + " -B " + buildPath.string() + " -DCMAKE_BUILD_TYPE=Release"))
                return false;
        }

        bool needsBuild = true;
        fs::path cacheDir = modPath / ".build_cache";
        fs::path hashFile = cacheDir / ".forge_src_hash";
        std::string currentHash = computeModuleSrcHash(modPath);

        // Check hash cache: skip build only if source is unchanged AND .so actually exists
        if (!force && !currentHash.empty() && fs::exists(hashFile)) {
            std::ifstream hf(hashFile);
            std::string savedHash;
            std::getline(hf, savedHash);
            if (savedHash == currentHash) {
                std::string entryPoint = mi.entry_point;
                fs::path builtLib;
                if (fs::exists(buildPath / entryPoint)) builtLib = buildPath / entryPoint;
                else if (fs::exists(buildPath / ("lib" + entryPoint))) builtLib = buildPath / ("lib" + entryPoint);
                else {
                    for (const auto& e : fs::directory_iterator(buildPath))
                        if (e.path().extension() == ".so") { builtLib = e.path(); break; }
                }
                if (!builtLib.empty())
                    needsBuild = false;
            }
        }

        if (hasCMake && needsBuild) {
            int jobs2 = 2; try { int n = (int)std::thread::hardware_concurrency(); if (n > 0) jobs2 = std::min(n > 2 ? n - 2 : 2, 2); } catch(...) {}
            Process::system("nice -n 10 cmake --build " + buildPath.string() + " --parallel " + std::to_string(jobs2));
            if (!currentHash.empty()) {
                fs::create_directories(cacheDir);
                std::ofstream of(hashFile);
                of << currentHash;
            }
        }

        if (hasCMake) {
            std::string entryPoint = mi.entry_point;
            fs::path builtLib;
            if (fs::exists(buildPath / entryPoint)) builtLib = buildPath / entryPoint;
            else if (fs::exists(buildPath / ("lib" + entryPoint))) builtLib = buildPath / ("lib" + entryPoint);
            else {
                if (fs::exists(buildPath))
                    for (const auto& e : fs::directory_iterator(buildPath))
                        if (e.path().extension() == ".so") { builtLib = e.path(); break; }
            }
            if (!builtLib.empty()) {
                fs::copy_file(builtLib, contPath / "bin" / entryPoint, fs::copy_options::overwrite_existing);
            } else {
                fs::path csProjCheck = findCSharpProj(modPath, mi.id);
                if (csProjCheck.empty()) {
                    std::cerr << "Error: Could not find any .so file in " << buildPath << std::endl;
                    return false;
                }
            }
        }

        // C# part: publish ALL .csproj found in module (heavy compose: HelloWorld + Shared)
        {
            // collect all csproj (first via findCSharpProj + recursive scan)
            std::vector<fs::path> allCsProjs;
            {
                fs::path first = findCSharpProj(modPath, mi.id);
                if (!first.empty()) allCsProjs.push_back(first);
                for (auto& e : fs::recursive_directory_iterator(modPath)) {
                    if (!e.is_regular_file()) continue;
                    if (e.path().extension() != ".csproj") continue;
                    if (std::find(allCsProjs.begin(), allCsProjs.end(), e.path()) == allCsProjs.end())
                        allCsProjs.push_back(e.path());
                }
            }
            bool anyCsProj = !allCsProjs.empty();
            if (anyCsProj) {
                // check clrDeps override in container.json: "clrDeps": "auto"|"manual"
                std::string clrDepsMode = contJson.value("clrDeps", "auto");
                bool copyDeps = (clrDepsMode == "auto");
                for (auto& csProj : allCsProjs) {
                fs::path csHashFile = cacheDir / ".forge_cs_hash";
                std::string csHash = computeModuleCSharpHash(modPath);
                bool needCsBuild = true;
                if (!force && !csHash.empty() && fs::exists(csHashFile)) {
                    std::ifstream hf(csHashFile);
                    std::string saved; std::getline(hf, saved);
                    if (saved == csHash) {
                        // check if dll already in container
                        std::string dllName = csProj.stem().string() + ".dll";
                        if (fs::exists(contPath / "bin" / dllName)) needCsBuild = false;
                    }
                }
                if (needCsBuild) {
                    std::cout << "    " << cli::gray("  + csharp ") << cli::bold(csProj.filename().string()) << "..." << "\n";
                    std::string outDir = (contPath / "bin").string();
                    // Publish to temp then copy to avoid second publish cleaning shared outDir (multi-csproj per module)
                    std::string tmpOut = (contPath / "bin" / (".tmp-publish-" + csProj.stem().string())).string();
                    std::string pub = "dotnet publish " + csProj.string() + " -c Release -o " + tmpOut;
                    // ensure DOTNET_ROOT for dev-box
                    const char* home = std::getenv("HOME");
                    std::string envPrefix;
                    if (home) {
                        std::string dr = std::string(home) + "/.dotnet";
                        if (fs::exists(dr)) envPrefix = "DOTNET_ROOT=" + dr + " ";
                    }
                    if (!Process::system(envPrefix + pub)) {
                        std::cerr << "Warning: dotnet publish failed for " << csProj << "\n";
                    } else {
                        try {
                            for (auto& e : fs::directory_iterator(tmpOut)) {
                                if (e.is_regular_file()) {
                                    fs::copy_file(e.path(), fs::path(outDir) / e.path().filename(), fs::copy_options::overwrite_existing);
                                }
                            }
                            fs::remove_all(tmpOut);
                        } catch(...) {}
                        if (!csHash.empty()) {
                            fs::create_directories(cacheDir);
                            std::ofstream of(csHashFile); of << csHash;
                        }
                    }
                } else {
                    std::cout << "    " << cli::gray("  + csharp ") << cli::gray("[up to date]") << "\n";
                }
            }
        }
        }

        if (copyAssets && updateAssets) {
            std::string assetsRoot = mi.dependencies.empty() ? "assets/" : "assets/"; // original used mJson.value("assets_root", "assets/")
            fs::path modJsonPath = modPath / "module.json";
            if (fs::exists(modJsonPath)) {
                std::ifstream mf(modJsonPath); json mj; mf >> mj;
                assetsRoot = mj.value("assets_root", "assets/");
                fs::copy(modPath / assetsRoot, contPath / "assets" / modId,
                         fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                std::string configRoot = mj.value("config_root", "configs/");
                if (fs::exists(modPath / configRoot)) {
                    fs::create_directories(contPath / "configs" / modId);
                    fs::copy(modPath / configRoot, contPath / "configs" / modId,
                             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                }
            }
        }

        if (srcIncluded) {
            fs::path destSrc = contPath / "src" / modId;
            fs::copy(modPath, destSrc, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            if (fs::exists(destSrc / "build")) fs::remove_all(destSrc / "build");
        }
        return needsBuild;
    };

    std::cout << "  " << cli::iconSDK() << " " << cli::bold("Syncing SDK headers...") << "\n";
    CmdSyncSdk syncCmd;
    syncCmd.execute(Args::parse({}), ws);

    std::cout << "  " << cli::iconBuild() << " " << cli::bold("Building modules...") << "\n";
    if (force) std::cout << "  " << cli::gray("(--force: skipping cache, rebuilding all)") << "\n";

    int builtCount = 0, cachedCount = 0;
    auto totalStart = std::chrono::steady_clock::now();

    for (size_t mi = 0; mi < allMods.size(); ++mi) {
        const auto& modId = allMods[mi];
        std::cout << "    " << cli::gray("[" + std::to_string(mi + 1) + "/" + std::to_string(allMods.size()) + "]")
                  << " " << cli::iconBullet() << " " << cli::bold(modId) << "...\n";
        auto modStart = std::chrono::steady_clock::now();
        bool didBuild = processModule(modId, true);
        auto modEnd = std::chrono::steady_clock::now();
        auto modMs = std::chrono::duration_cast<std::chrono::milliseconds>(modEnd - modStart).count();

        if (didBuild) {
            builtCount++;
            std::cout << "    " << cli::gray("  \xe2\x94\x94\xe2\x94\x80 ") << cli::iconOk()
                      << " " << cli::cyan("(" + std::to_string(modMs) + "ms)") << "\n";
        } else {
            cachedCount++;
            std::cout << "    " << cli::gray("  \xe2\x94\x94\xe2\x94\x80 ") << cli::iconOk()
                      << " " << cli::gray("[up to date]") << "\n";
        }
    }

    auto totalEnd = std::chrono::steady_clock::now();
    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(totalEnd - totalStart).count();
    std::cout << "\n";
    cli::printSuccess("Container '" + name + "' built successfully!");
    std::cout << "    " << cli::gray("Modules: ")
              << cli::bold(std::to_string(builtCount) + " built") << ", "
              << cli::bold(std::to_string(cachedCount) + " up-to-date") << ", "
              << cli::gray("total " + std::to_string(totalMs) + "ms") << "\n";
    return 0;
}

// =============================================================================
// run
// =============================================================================
int CmdRun::execute(const Args& args, Workspace& ws) {
    std::string name = args.pos(0);

    if (name.empty()) {
        fs::path activeFile = ws.projectRoot() / "configs" / "active_container.json";
        if (fs::exists(activeFile)) {
            try {
                std::ifstream f(activeFile);
                json active; f >> active;
                name = active.value("active_container", "");
            } catch (...) {}
        }
        if (name.empty()) {
            std::cerr << "Error: No active container set. Use 'run <container>' to set and launch.\n";
            return 1;
        }
    } else {
        fs::path activeFile = ws.projectRoot() / "configs" / "active_container.json";
        fs::create_directories(activeFile.parent_path());
        json active = {{"active_container", name}};
        std::ofstream f(activeFile);
        f << active.dump(4);
        cli::printSuccess("Active container set to '" + name + "'");
    }

    std::cout << "Launching engine with container: " << name << "...\n";
    fs::path appPath = ws.engineBinary();
    if (fs::exists(appPath)) {
        Process::system(appPath.string());
    } else {
        std::cerr << "Error: Engine executable not found at "
                  << appPath << ". Build it first with 'engine-build'.\n";
    }
    return 0;
}

// =============================================================================
// watch
// =============================================================================
static std::atomic<bool> watchRunning{true};
static void watchSignalHandler(int) { watchRunning = false; }

int CmdWatch::execute(const Args& args, Workspace& ws) {
    std::string contName = args.pos(0);
    if (contName.empty()) { cli::printError("Container name required"); return 1; }

    json contJson = ws.loadContainerJson(contName);
    if (contJson.empty()) { cli::printError("Container '" + contName + "' not found"); return 1; }

    std::vector<std::string> allMods;
    auto addFrom = [&](const std::string& key) {
        if (contJson.contains(key) && contJson[key].is_array())
            for (auto& m : contJson[key]) allMods.push_back(m.get<std::string>());
    };
    addFrom("preload");
    addFrom("load_order");
    if (contJson.contains("stages")) {
        addFrom("stage0_boot");
        addFrom("stage1_hosts");
        addFrom("stage3_finalize");
    }
    if (allMods.empty()) { cli::printError("No modules in container '" + contName + "'"); return 1; }

    struct WatchEntry { std::string modName; fs::path srcDir; int wd; };
    std::vector<WatchEntry> watches;
    std::map<int, size_t> wdToIdx;

    cli::printSection(cli::iconBox(), "Watching: " + contName);
    int inotifyFd = inotify_init();
    if (inotifyFd < 0) { cli::printError("Failed to initialize inotify"); return 1; }

    for (const auto& modId : allMods) {
        fs::path modPath = ws.resolveModule(modId);
        if (modPath.empty()) { continue; }
        fs::path srcDir = modPath / "src";
        if (!fs::exists(srcDir)) continue;

        int wd = inotify_add_watch(inotifyFd, srcDir.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
        if (wd >= 0) {
            size_t idx = watches.size();
            watches.push_back({modId, srcDir, wd});
            wdToIdx[wd] = idx;
            int srcCount = 0;
            for (auto& e : fs::recursive_directory_iterator(srcDir))
                if (e.is_regular_file()) srcCount++;
            std::cout << "    " << cli::iconBullet() << " " << cli::bold(modId)
                      << cli::gray(" (" + std::to_string(srcCount) + " files)") << "\n";
        }
    }

    if (watches.empty()) { cli::printError("Nothing to watch"); close(inotifyFd); return 1; }
    cli::printRule();
    std::cout << "  " << cli::gray("Watching " + std::to_string(watches.size())
                                   + " module(s). Press Ctrl+C to stop.") << "\n";

    signal(SIGINT, watchSignalHandler);
    struct pollfd pfd = {inotifyFd, POLLIN, 0};
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

    while (watchRunning) {
        int ret = poll(&pfd, 1, 500);
        if (ret < 0) break;
        if (ret == 0) continue;

        ssize_t len = read(inotifyFd, buf, sizeof(buf));
        if (len <= 0) break;

        const char* ptr = buf;
        while (ptr < buf + len) {
            auto* event = (struct inotify_event*)ptr;
            ptr += sizeof(struct inotify_event) + event->len;

            auto it = wdToIdx.find(event->wd);
            if (it == wdToIdx.end()) continue;

            auto& we = watches[it->second];
            std::string filename = event->len > 0 ? event->name : "";
            if (!filename.empty() && filename[0] == '.') continue;

            std::string ext = filename.size() > 4 ? filename.substr(filename.size() - 4) : "";
            bool isCpp = (ext == ".cpp" || ext == ".hpp" || (filename.size() > 2 && filename.substr(filename.size() - 2) == ".c") || (filename.size() > 2 && filename.substr(filename.size() - 2) == ".h"));
            bool isCs = (filename.size() > 3 && filename.substr(filename.size() - 3) == ".cs") || (filename.size() > 7 && filename.substr(filename.size() - 7) == ".csproj");
            if (!isCpp && !isCs) continue;

            std::cout << "\n  " << cli::yellow("\xF0\x9F\x93�") << " " << cli::bold(we.modName)
                      << ": " << filename << " " << cli::gray("changed") << "\n";
            std::cout << "  " << cli::iconBuild() << " " << cli::bold("Rebuilding "
                      + we.modName) << "...\n";

            fs::path modPath = ws.resolveModule(we.modName);
            if (!modPath.empty()) {
                bool isCsChange = (filename.size() > 3 && filename.substr(filename.size() - 3) == ".cs") || (filename.size() > 7 && filename.substr(filename.size() - 7) == ".csproj");
                if (isCsChange) {
                    fs::path csProj = findCSharpProj(modPath, we.modName);
                    if (!csProj.empty()) {
                        std::cout << "  " << cli::iconBuild() << " dotnet publish " << csProj.filename().string() << "...\n";
                        std::string outDir = (ws.containerPath(contName) / "bin").string();
                        std::string pub = "dotnet publish " + csProj.string() + " -c Release -o " + outDir;
                        const char* home = std::getenv("HOME");
                        std::string envPrefix;
                        if (home) { std::string dr = std::string(home) + "/.dotnet"; if (fs::exists(dr)) envPrefix = "DOTNET_ROOT=" + dr + " "; }
                        Process::system(envPrefix + pub);
                        cli::printSuccess(we.modName + " (csharp) rebuilt and deployed");
                        std::cout << "  " << cli::gray("Watching...") << "\n";
                        continue;
                    }
                }
                fs::path bPath = modPath / "build";
                fs::create_directories(bPath);
                Process::system("cmake -S " + modPath.string() + " -B " + bPath.string()
                                + " -DCMAKE_BUILD_TYPE=Release");
                { int jobs3 = 2; try { int n = (int)std::thread::hardware_concurrency(); if (n > 0) jobs3 = std::min(n > 2 ? n - 2 : 2, 2); } catch(...) {} Process::system("nice -n 10 cmake --build " + bPath.string() + " --parallel " + std::to_string(jobs3)); }

                std::string entryPoint = we.modName + ".so";
                fs::path builtLib;
                if (fs::exists(bPath / entryPoint)) builtLib = bPath / entryPoint;
                else if (fs::exists(bPath / ("lib" + entryPoint))) builtLib = bPath / ("lib" + entryPoint);
                else {
                    for (const auto& e : fs::directory_iterator(bPath))
                        if (e.path().extension() == ".so") { builtLib = e.path(); break; }
                }
                if (!builtLib.empty()) {
                    fs::path contBin = ws.containerPath(contName) / "bin";
                    fs::create_directories(contBin);
                    fs::copy_file(builtLib, contBin / entryPoint, fs::copy_options::overwrite_existing);
                    cli::printSuccess(we.modName + " rebuilt and deployed");
                } else {
                    cli::printError("Build failed for " + we.modName);
                }
            }
            std::cout << "  " << cli::gray("Watching...") << "\n";
        }
    }

    for (auto& we : watches) inotify_rm_watch(inotifyFd, we.wd);
    close(inotifyFd);
    std::cout << "\n  " << cli::gray("\xF0\x9F\x91\x8B Watch stopped") << "\n";
    return 0;
}

// =============================================================================
// clean
// =============================================================================
int CmdClean::execute(const Args& args, Workspace& ws) {
    bool all = args.has("--all");
    int removed = 0;

    cli::printSection(cli::iconBuild(), "Cleaning Build Artifacts");

    ws.eachModuleDir([&](const fs::path& p, const std::string& name) {
        auto cleanDir = [&](const fs::path& dir) {
            if (fs::exists(dir)) {
                fs::remove_all(dir);
                std::cout << "    " << cli::iconBullet() << " " << name << "/"
                          << dir.filename().string() << " " << cli::gray("removed") << "\n";
                removed++;
            }
        };
        cleanDir(p / "build");
        cleanDir(p / "build-windows-clang");
    });

    fs::path engineBuild = ws.engineBuildDir();
    if (fs::exists(engineBuild)) {
        fs::remove_all(engineBuild);
        std::cout << "    " << cli::iconBullet() << " engine/build " << cli::gray("removed") << "\n";
        removed++;
    }

    fs::path contDir = ws.containersRoot();
    if (fs::exists(contDir)) {
        for (const auto& entry : fs::directory_iterator(contDir)) {
            if (entry.is_directory()) {
                fs::path binPath = entry.path() / "bin";
                if (fs::exists(binPath)) {
                    fs::remove_all(binPath);
                    std::cout << "    " << cli::iconBullet() << " container/"
                              << entry.path().filename().string() << "/bin "
                              << cli::gray("removed") << "\n";
                    removed++;
                }
            }
        }
    }

    // --all: also clean vendored SDK headers
    if (all) {
        ws.eachModuleDir([&](const fs::path& p, const std::string& name) {
            fs::path sdkDir = p / "include" / "SDK";
            if (fs::exists(sdkDir)) {
                fs::remove_all(sdkDir);
                std::cout << "    " << cli::iconBullet() << " " << name
                          << "/include/SDK " << cli::gray("removed") << "\n";
                removed++;
            }
        });
    }

    cli::printRule();
    cli::printSuccess(std::to_string(removed) + " artifact(s) removed");
    return 0;
}
