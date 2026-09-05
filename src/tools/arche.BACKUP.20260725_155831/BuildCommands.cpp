#include "Arche.h"
#include "SHA256.h"
#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>
#include <csignal>
#include <atomic>
#include <chrono>

static std::atomic<bool> watchRunning{true};
static void watchSignalHandler(int) { watchRunning = false; }

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

// --- Build container ---
void Arche::doBuild(const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << "Error: Container name required.\n"; return; }
    std::string name = args[0];
    bool updateAssets = false;
    bool srcIncluded = false;
    bool force = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--update-assets") updateAssets = true;
        if (args[i] == "--src-included") srcIncluded = true;
        if (args[i] == "--force" || args[i] == "-f") force = true;
    }
    fs::path contPath = projectRoot_ / "container" / name;
    fs::path contJsonPath = contPath / "container.json";
    if (!fs::exists(contJsonPath)) return;
    try {
        std::ifstream inFile(contJsonPath);
        json contJson; inFile >> contJson; inFile.close();

        if (!contJson.contains("preload") || !contJson["preload"].is_array()) {
            contJson["preload"] = json::array();
        }
        bool hasNexus = false;
        for (auto& p : contJson["preload"]) {
            if (p.get<std::string>() == "NexusXCore") { hasNexus = true; break; }
        }
        if (!hasNexus) {
            contJson["preload"].push_back("NexusXCore");
            std::ofstream outFile(contJsonPath);
            outFile << contJson.dump(4);
            outFile.close();
        }

        // Collect all modules to process (supports legacy and staged format)
        std::vector<std::string> allMods;
        if (contJson.contains("stages")) {
            auto& stages = contJson["stages"];
            auto collectArray = [&](const std::string& key) {
                if (stages.contains(key) && stages[key].is_array())
                    for (auto& m : stages[key]) {
                        std::string mod = m.get<std::string>();
                        if (std::find(allMods.begin(), allMods.end(), mod) == allMods.end())
                            allMods.push_back(mod);
                    }
            };
            collectArray("stage0_boot");
            collectArray("stage1_hosts");
            collectArray("stage3_finalize");
            if (stages.contains("stage2_submodules") && stages["stage2_submodules"].is_object())
                for (auto& [hostId, subs] : stages["stage2_submodules"].items())
                    for (auto& sub : subs) {
                        std::string mod = sub.get<std::string>();
                        if (std::find(allMods.begin(), allMods.end(), mod) == allMods.end())
                            allMods.push_back(mod);
                    }
        } else {
            // Legacy: preload + load_order
            if (contJson.contains("preload") && contJson["preload"].is_array())
                for (auto& p : contJson["preload"]) allMods.push_back(p.get<std::string>());
            if (contJson.contains("load_order") && contJson["load_order"].is_array())
                for (auto& m : contJson["load_order"]) allMods.push_back(m.get<std::string>());
        }

        fs::create_directories(contPath / "bin");
        fs::create_directories(contPath / "assets");
        fs::create_directories(contPath / "configs");
        if (srcIncluded) fs::create_directories(contPath / "src");

        auto processModule = [&](const std::string& modId, bool copyAssets) -> bool {
            fs::path modPath = resolveModuleDir(modId);
            if (modPath.empty()) {
                std::cerr << "Error: ModuleInstance '" << modId << "' not found.\n";
                return false;
            }
            fs::path modJsonPath = modPath / "module.json";
            if (!fs::exists(modJsonPath)) return false;
            std::ifstream mFile(modJsonPath);
            json mJson; mFile >> mJson; mFile.close();
            fs::path buildPath = modPath / "build";
            bool recache = std::find(args.begin(), args.end(), "--recache") != args.end();
            if (recache || !fs::exists(buildPath) || !fs::exists(buildPath / "CMakeCache.txt")) {
                if (recache && fs::exists(buildPath)) fs::remove_all(buildPath);
                fs::create_directories(buildPath);
                if (!runCommand("cmake -S " + modPath.string() + " -B " + buildPath.string() + " -DCMAKE_BUILD_TYPE=Release")) return false;
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
                runCommand("cmake --build " + buildPath.string() + " --parallel $(nproc)");
                if (!currentHash.empty()) {
                    fs::create_directories(cacheDir);
                    std::ofstream of(hashFile);
                    of << currentHash;
                }
            }

            std::string entryPoint = mJson.value("entry_point", modId + ".so");

            fs::path builtLib;
            if (fs::exists(buildPath / entryPoint)) builtLib = buildPath / entryPoint;
            else if (fs::exists(buildPath / ("lib" + entryPoint))) builtLib = buildPath / ("lib" + entryPoint);
            else {
                for (const auto& entry : fs::directory_iterator(buildPath)) {
                    if (entry.path().extension() == ".so") {
                        builtLib = entry.path();
                        break;
                    }
                }
            }

            if (!builtLib.empty()) {
                fs::copy_file(builtLib, contPath / "bin" / entryPoint, fs::copy_options::overwrite_existing);
            } else {
                std::cerr << "Error: Could not find any .so file in " << buildPath << std::endl;
                return false;
            }
            if (copyAssets && updateAssets) {
                std::string assetsRoot = mJson.value("assets_root", "assets/");
                fs::copy(modPath / assetsRoot, contPath / "assets" / modId, fs::copy_options::recursive | fs::copy_options::overwrite_existing);

                std::string configRoot = mJson.value("config_root", "configs/");
                if (fs::exists(modPath / configRoot)) {
                    fs::create_directories(contPath / "configs" / modId);
                    fs::copy(modPath / configRoot, contPath / "configs" / modId,
                             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                    std::cout << "  copied configs/" << modId << std::endl;
                }
            }
            if (srcIncluded) {
                fs::path destSrc = contPath / "src" / modId;
                fs::copy(modPath, destSrc, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                if (fs::exists(destSrc / "build")) fs::remove_all(destSrc / "build");
            }
            return needsBuild;
        };

        // Build modules
        std::cout << "  " << cli::iconSDK() << " " << cli::bold("Syncing SDK headers...") << "\n";
        doSyncSdk({});
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
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << std::endl; }
}

// --- Run container ---
void Arche::doRun(const std::vector<std::string>& args) {
    std::string name;
    if (args.empty() || args[0].empty()) {
        fs::path activeContFile = projectRoot_ / "configs" / "active_container.json";
        if (fs::exists(activeContFile)) {
            try {
                std::ifstream file(activeContFile);
                json active; file >> active;
                name = active.value("active_container", "");
            } catch (...) {}
        }
        if (name.empty()) {
            std::cerr << "Error: No active container set. Use 'run <container>' to set and launch.\n";
            return;
        }
    } else {
        name = args[0];
        fs::path activeContFile = projectRoot_ / "configs" / "active_container.json";
        fs::create_directories(activeContFile.parent_path());
        json active = {{"active_container", name}};
        std::ofstream file(activeContFile);
        file << active.dump(4);
        cli::printSuccess("Active container set to '" + name + "'");
    }

    std::cout << "Launching engine with container: " << name << "...\n";
    fs::path appPath = projectRoot_ / "build" / "archetyped";
    if (fs::exists(appPath)) {
        runCommand(appPath.string());
    } else {
        std::cerr << "Error: Engine executable not found at " << appPath << ". Build it first with 'engine-build'.\n";
    }
}

// --- Watch: hot-reload loop ---
void Arche::doWatch(const std::vector<std::string>& args) {
    if (args.empty()) { cli::printError("Container name required"); return; }
    std::string contName = args[0];
    fs::path contJsonPath = projectRoot_ / "container" / contName / "container.json";
    if (!fs::exists(contJsonPath)) { cli::printError("Container '" + contName + "' not found"); return; }

    json contJson;
    { std::ifstream f(contJsonPath); f >> contJson; }

    // Collect modules
    std::vector<std::string> allMods;
    auto addMods = [&](const std::string& key) {
        if (contJson.contains(key) && contJson[key].is_array())
            for (auto& m : contJson[key]) allMods.push_back(m.get<std::string>());
    };
    addMods("preload");
    addMods("load_order");
    if (contJson.contains("stages")) {
        addMods("stage0_boot");
        addMods("stage1_hosts");
        addMods("stage3_finalize");
    }

    if (allMods.empty()) { cli::printError("No modules in container '" + contName + "'"); return; }

    // Resolve each module's source directory
    struct WatchEntry { std::string modName; fs::path srcDir; int wd; };
    std::vector<WatchEntry> watches;
    std::map<int, size_t> wdToIdx;

    cli::printSection(cli::iconBox(), "Watching: " + contName);
    int inotifyFd = inotify_init();
    if (inotifyFd < 0) { cli::printError("Failed to initialize inotify"); return; }

    for (const auto& modId : allMods) {
        fs::path modPath = resolveModuleDir(modId);
        if (modPath.empty()) { std::cout << "    " << cli::iconWarn() << " ModuleInstance '" << modId << "' not found, skipping\n"; continue; }
        fs::path srcDir = modPath / "src";
        if (!fs::exists(srcDir)) { std::cout << "    " << cli::iconWarn() << " " << modId << "/src not found, skipping\n"; continue; }

        int wd = inotify_add_watch(inotifyFd, srcDir.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
        if (wd >= 0) {
            size_t idx = watches.size();
            watches.push_back({modId, srcDir, wd});
            wdToIdx[wd] = idx;
            // Count source files
            int srcCount = 0;
            for (auto& e : fs::recursive_directory_iterator(srcDir))
                if (e.is_regular_file()) srcCount++;
            std::cout << "    " << cli::iconBullet() << " " << cli::bold(modId) << cli::gray(" (" + std::to_string(srcCount) + " files)") << "\n";
        }
    }

    if (watches.empty()) { cli::printError("Nothing to watch"); close(inotifyFd); return; }

    cli::printRule();
    std::cout << "  " << cli::gray("Watching " + std::to_string(watches.size()) + " module(s). Press Ctrl+C to stop.") << "\n";

    signal(SIGINT, watchSignalHandler);

    // Event loop
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
            std::string ext = filename.size() > 2 ? filename.substr(filename.size() - 2) : "";
            if (ext != ".h" && ext != ".c" && !(filename.size() > 4 && filename.substr(filename.size() - 4) == ".cpp") && !(filename.size() > 2 && filename.substr(filename.size() - 2) == ".c"))
                continue;

            std::cout << "\n  " << cli::yellow("📝") << " " << cli::bold(we.modName) << ": " << filename << " " << cli::gray("changed") << "\n";
            std::cout << "  " << cli::iconBuild() << " " << cli::bold("Rebuilding " + we.modName) << "...\n";

            // Rebuild just this module
            fs::path modPath = resolveModuleDir(we.modName);
            if (!modPath.empty()) {
                fs::path buildPath = modPath / "build";
                fs::create_directories(buildPath);
                runCommand("cmake -S " + modPath.string() + " -B " + buildPath.string() + " -DCMAKE_BUILD_TYPE=Release");
                runCommand("cmake --build " + buildPath.string() + " --parallel $(nproc)");

                // Copy .so to container
                std::string entryPoint = we.modName + ".so";
                fs::path builtLib;
                if (fs::exists(buildPath / entryPoint)) builtLib = buildPath / entryPoint;
                else if (fs::exists(buildPath / ("lib" + entryPoint))) builtLib = buildPath / ("lib" + entryPoint);
                else {
                    for (const auto& e : fs::directory_iterator(buildPath))
                        if (e.path().extension() == ".so") { builtLib = e.path(); break; }
                }
                if (!builtLib.empty()) {
                    fs::path contBin = projectRoot_ / "container" / contName / "bin";
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

    // Cleanup
    for (auto& we : watches) inotify_rm_watch(inotifyFd, we.wd);
    close(inotifyFd);
    std::cout << "\n  " << cli::gray("👋 Watch stopped") << "\n";
}
