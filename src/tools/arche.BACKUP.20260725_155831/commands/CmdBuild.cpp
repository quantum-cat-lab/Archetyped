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

// =============================================================================
// engine-build
// =============================================================================
void CmdEngineBuild::execute(const Args& args, Workspace& ws) {
    std::cout << "  " << cli::iconBuild() << " " << cli::bold("Building Archetyped Engine Core...") << "\n";
    fs::path buildPath = ws.engineBuildDir();
    bool recache = args.has("--recache");
    try {
        if (recache && fs::exists(buildPath)) fs::remove_all(buildPath);
        if (!fs::exists(buildPath)) fs::create_directories(buildPath);
        std::string cmakeConfig = "cmake -S " + ws.projectRoot().string() + " -B " + buildPath.string() + " -DCMAKE_BUILD_TYPE=Release";
        if (!Process::system(cmakeConfig)) return;
        std::string cmakeBuild = "cmake --build " + buildPath.string() + " --parallel $(nproc)";
        if (!Process::system(cmakeBuild)) return;

        // Sync SDK headers into template
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
}

// =============================================================================
// arche-build
// =============================================================================
void CmdArcheBuild::execute(const Args& args, Workspace& ws) {
    std::cout << "Rebuilding Archetyped Toolchain (arche)..." << std::endl;
    fs::path buildPath = ws.engineBuildDir();
    bool recache = args.has("--recache");

    if (recache && fs::exists(buildPath)) fs::remove_all(buildPath);
    if (!fs::exists(buildPath)) fs::create_directories(buildPath);

    std::string cmakeConfig = "cmake -S " + ws.projectRoot().string() + " -B " + buildPath.string() + " -DCMAKE_BUILD_TYPE=Release";
    if (!Process::system(cmakeConfig)) return;

    if (Process::system("cmake --build " + buildPath.string() + " --target arche")) {
        cli::printSuccess("Arche rebuilt successfully");
    } else {
        cli::printError("Failed to rebuild arche");
    }
}

// =============================================================================
// build <container>
// =============================================================================
void CmdBuild::execute(const Args& args, Workspace& ws) {
    std::string name = args.pos(0);
    if (name.empty()) { std::cerr << "Error: Container name required.\n"; return; }

    bool updateAssets = args.has("--update-assets");
    bool srcIncluded  = args.has("--src-included");
    bool force        = args.has("--force") || args.has("-f");
    bool recache      = args.has("--recache");

    fs::path contPath = ws.containerPath(name);
    json contJson = ws.loadContainerJson(name);
    if (contJson.empty() || !contJson.contains("container_id")) {
        cli::printError("Container '" + name + "' not found");
        return;
    }

    // Ensure NexusXCore in preload
    if (!contJson.contains("preload") || !contJson["preload"].is_array())
        contJson["preload"] = json::array();
    bool hasNexus = false;
    for (auto& p : contJson["preload"])
        if (p.get<std::string>() == "NexusXCore") { hasNexus = true; break; }
    if (!hasNexus) {
        contJson["preload"].push_back("NexusXCore");
        ws.saveContainerJson(name, contJson);
    }

    // Collect modules
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
        for (auto& m : collectMods("stage0_boot")) addUnique(m);
        for (auto& m : collectMods("stage1_hosts")) addUnique(m);
        for (auto& m : collectMods("stage3_finalize")) addUnique(m);
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
        if (recache || !fs::exists(buildPath) || !fs::exists(buildPath / "CMakeCache.txt")) {
            if (recache && fs::exists(buildPath)) fs::remove_all(buildPath);
            fs::create_directories(buildPath);
            if (!Process::system("cmake -S " + modPath.string() + " -B " + buildPath.string() + " -DCMAKE_BUILD_TYPE=Release"))
                return false;
        }

        bool needsBuild = true;
        fs::path cacheDir = modPath / ".build_cache";
        fs::path hashFile = cacheDir / ".arche_src_hash";
        std::string currentHash = computeModuleSrcHash(modPath);

        if (!force && !currentHash.empty() && fs::exists(hashFile)) {
            std::ifstream hf(hashFile);
            std::string savedHash;
            std::getline(hf, savedHash);
            if (savedHash == currentHash) needsBuild = false;
        }

        if (needsBuild) {
            Process::system("cmake --build " + buildPath.string() + " --parallel $(nproc)");
            if (!currentHash.empty()) {
                fs::create_directories(cacheDir);
                std::ofstream of(hashFile);
                of << currentHash;
            }
        }

        // Find .so
        std::string entryPoint = mi.entry_point;
        fs::path builtLib;
        if (fs::exists(buildPath / entryPoint)) builtLib = buildPath / entryPoint;
        else if (fs::exists(buildPath / ("lib" + entryPoint))) builtLib = buildPath / ("lib" + entryPoint);
        else {
            for (const auto& e : fs::directory_iterator(buildPath))
                if (e.path().extension() == ".so") { builtLib = e.path(); break; }
        }

        if (!builtLib.empty()) {
            fs::copy_file(builtLib, contPath / "bin" / entryPoint, fs::copy_options::overwrite_existing);
        } else {
            std::cerr << "Error: Could not find any .so file in " << buildPath << std::endl;
            return false;
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

    // Sync SDK then build all modules
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
}

// =============================================================================
// run
// =============================================================================
void CmdRun::execute(const Args& args, Workspace& ws) {
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
            return;
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
}

// =============================================================================
// watch
// =============================================================================
static std::atomic<bool> watchRunning{true};
static void watchSignalHandler(int) { watchRunning = false; }

void CmdWatch::execute(const Args& args, Workspace& ws) {
    std::string contName = args.pos(0);
    if (contName.empty()) { cli::printError("Container name required"); return; }

    json contJson = ws.loadContainerJson(contName);
    if (contJson.empty()) { cli::printError("Container '" + contName + "' not found"); return; }

    // Collect modules
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
    if (allMods.empty()) { cli::printError("No modules in container '" + contName + "'"); return; }

    struct WatchEntry { std::string modName; fs::path srcDir; int wd; };
    std::vector<WatchEntry> watches;
    std::map<int, size_t> wdToIdx;

    cli::printSection(cli::iconBox(), "Watching: " + contName);
    int inotifyFd = inotify_init();
    if (inotifyFd < 0) { cli::printError("Failed to initialize inotify"); return; }

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

    if (watches.empty()) { cli::printError("Nothing to watch"); close(inotifyFd); return; }
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

            // Only watch .c/.cpp/.h/.hpp
            std::string ext = filename.size() > 4 ? filename.substr(filename.size() - 4) : "";
            if (ext != ".cpp" && ext != ".hpp" && !(filename.size() > 2 && filename.substr(filename.size() - 2) == ".c")
                && !(filename.size() > 2 && filename.substr(filename.size() - 2) == ".h"))
                continue;

            std::cout << "\n  " << cli::yellow("\xF0\x9F\x93�") << " " << cli::bold(we.modName)
                      << ": " << filename << " " << cli::gray("changed") << "\n";
            std::cout << "  " << cli::iconBuild() << " " << cli::bold("Rebuilding "
                      + we.modName) << "...\n";

            fs::path modPath = ws.resolveModule(we.modName);
            if (!modPath.empty()) {
                fs::path bPath = modPath / "build";
                fs::create_directories(bPath);
                Process::system("cmake -S " + modPath.string() + " -B " + bPath.string()
                                + " -DCMAKE_BUILD_TYPE=Release");
                Process::system("cmake --build " + bPath.string() + " --parallel $(nproc)");

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
}

// =============================================================================
// clean
// =============================================================================
void CmdClean::execute(const Args& args, Workspace& ws) {
    bool all = args.has("--all");
    int removed = 0;

    cli::printSection(cli::iconBuild(), "Cleaning Build Artifacts");

    // ModuleInstance build dirs
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

    // Engine build dir
    fs::path engineBuild = ws.engineBuildDir();
    if (fs::exists(engineBuild)) {
        fs::remove_all(engineBuild);
        std::cout << "    " << cli::iconBullet() << " engine/build " << cli::gray("removed") << "\n";
        removed++;
    }

    // Container bin dirs
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
}
