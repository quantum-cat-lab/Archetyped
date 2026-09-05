#include "ContainerLoader.h"
#include "core/FractalKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include "components/vfs/VFSDomains.h"
#include "components/ModuleLoader/ModuleInstance.h"
#include "components/ModuleLoader/ModuleFactory.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <thread>
#include "json.hpp"

constexpr uint32_t reloadHash = fnv1aHashConst("archetyped:container_loader:reload");

ContainerLoader::ContainerLoader() {
    FractalKernel::instance().registerCMDMethod(reloadHash, &reloadCMD);
    loadActiveContainer();
}

std::string ContainerLoader::getActiveContainerName() {
    std::ifstream file("configs/active_container.json");
    if (!file.is_open()) return "";
    json j;
    file >> j;
    return j.value("active_container", "");
}

static void loadSingleModule(const std::string& modId, const std::string& modPath,
                              const ModuleConfig& config) {
    loadModuleContext loadCtx;
    std::strncpy(loadCtx.path, modPath.c_str(), sizeof(loadCtx.path) - 1);
    loadCtx.config = config;
    std::strncpy(loadCtx.config.loadedModulePath, modPath.c_str(),
                 sizeof(loadCtx.config.loadedModulePath) - 1);

    FURCMDPacket loadPacket;
    loadPacket.methodHash = loadModuleHash;
    loadPacket.payload = &loadCtx;
    loadPacket.payloadSize = sizeof(loadModuleContext);

    std::cout << "[ContainerLoader] Requesting load: " << modPath << "\n";

    // Call loadModule directly on the calling thread instead of going through
    // the async FURCMD queue.  This prevents a deadlock: ModuleMain (called
    // inside loadModule) often sends synchronous FURCMD packets (e.g. ECS
    // registerCMDomain with fence spin-wait).  When ModuleMain runs on the
    // FURCMD worker thread, the worker cannot drain its own queue while
    // spin-waiting, causing a deadlock.  By running ModuleMain on the main
    // thread, its FURCMD calls are queued to the worker which is free to
    // process them.
    ModuleFactory::loadModule(loadPacket);

    // After the direct call, state should already be SDK_Ready (auto-promoted
    // by ModuleFactory).  Verify with a quick poll in case a module set it
    // asynchronously.
    getModuleStateContext stateCtx;
    std::strncpy(stateCtx.path, modPath.c_str(), sizeof(stateCtx.path) - 1);
    stateCtx.path[sizeof(stateCtx.path) - 1] = '\0';

    char stateBuf[sizeof(int)];
    uint64_t stateFence = 0;

    FURCMDPacket statePacket;
    statePacket.methodHash = getModuleStateHash;
    statePacket.payload = &stateCtx;
    statePacket.payloadSize = sizeof(getModuleStateContext);
    statePacket.outputBuffer = stateBuf;
    statePacket.fence = &stateFence;

    for (int attempt = 0; attempt < 100; ++attempt) {
        stateFence = 0;
        FractalKernel::instance().sendCMDPacket(statePacket);

        while (stateFence != 1) {
            std::this_thread::yield();
        }

        int currentState = 0;
        std::memcpy(&currentState, stateBuf, sizeof(int));

        if (currentState >= ModuleInstance::SDK_Ready) {
            std::cout << "[ContainerLoader] ModuleInstance " << modId
                      << " is READY (state=" << currentState << ")." << std::endl;
            return;
        } else if (currentState == ModuleInstance::Error) {
            std::cerr << "[ContainerLoader] ModuleInstance " << modId
                      << " encountered an ERROR during init." << std::endl;
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cerr << "[ContainerLoader] ModuleInstance " << modId << " timed out waiting for READY state." << std::endl;
}

static void loadModulesFromArray(const json& arr, const std::string& contName,
                                  const ModuleConfig& config) {
    if (!arr.is_array()) return;
    for (const auto& modId : arr) {
        std::string mid = modId.get<std::string>();
        std::string modPath = "container/" + contName + "/bin/" + mid + ".so";
        loadSingleModule(mid, modPath, config);
    }
}

ISubmoduleHost* ContainerLoader::findHost(const std::string& hostId) {
    uint32_t hash = fnv1aHash(hostId.c_str());
    auto it = m_hosts.find(hash);
    if (it != m_hosts.end()) return it->second->host;

    std::string hostPath = "container/" + getActiveContainerName() + "/bin/" + hostId + ".so";
    auto* host = ModuleFactory::probeHost(hostPath.c_str());
    if (host) {
        auto entry = std::make_unique<HostEntry>();
        entry->id = hostId;
        entry->host = host;
        m_hosts[hash] = std::move(entry);
    }
    return host;
}

static ModuleConfig makeConfig(const std::string& contName) {
    std::string rootDir = "container/" + contName + "/";
    ModuleConfig config;
    std::strncpy(config.containerRoot, rootDir.c_str(), sizeof(config.containerRoot) - 1);
    std::strncpy(config.userRoot, "user_data/", sizeof(config.userRoot) - 1);
    return config;
}

void ContainerLoader::loadStaged(const json& j, const std::string& contName,
                                  const ModuleConfig& config) {
    const auto& stages = j["stages"];

    if (stages.contains("stage0_boot")) {
        std::cout << "[ContainerLoader] Stage 0 (boot) — loading system modules..." << std::endl;
        loadModulesFromArray(stages["stage0_boot"], contName, config);
    }

    if (stages.contains("stage1_hosts")) {
        std::cout << "[ContainerLoader] Stage 1 (hosts) — loading host modules..." << std::endl;
        loadModulesFromArray(stages["stage1_hosts"], contName, config);

        for (const auto& hostId : stages["stage1_hosts"]) {
            std::string hostName = hostId.get<std::string>();
            findHost(hostName);
        }
    }

    if (stages.contains("stage2_submodules")) {
        std::cout << "[ContainerLoader] Stage 2 (submodules) — delegating to hosts..." << std::endl;
        for (auto& [hostId, submodules] : stages["stage2_submodules"].items()) {
            auto* host = findHost(hostId);
            if (!host) {
                std::cerr << "[ContainerLoader] Host not found: " << hostId << std::endl;
                continue;
            }
            for (const auto& subId : submodules) {
                std::string subName = subId.get<std::string>();
                std::string subPath = "container/" + contName + "/bin/" + subName + ".so";

                ModuleConfig subConfig = makeConfig(contName);
                std::strncpy(subConfig.loadedModulePath, subPath.c_str(),
                             sizeof(subConfig.loadedModulePath) - 1);

                std::cout << "[ContainerLoader] Delegating submodule " << subName
                          << " to host " << hostId << std::endl;
                host->loadSubmodule(subName.c_str(), subConfig);
            }
        }
    }

    if (stages.contains("stage3_finalize")) {
        std::cout << "[ContainerLoader] Stage 3 (finalize) — finalizing hosts..." << std::endl;
        for (const auto& hostId : stages["stage3_finalize"]) {
            std::string hostName = hostId.get<std::string>();
            auto* host = findHost(hostName);
            if (!host) {
                std::cerr << "[ContainerLoader] Host not found for finalize: " << hostName << std::endl;
                continue;
            }
            std::cout << "[ContainerLoader] Finalizing host: " << hostName << std::endl;
            host->finalizeInit();
        }
    }

    std::cout << "[ContainerLoader] Container '" << contName << "' fully loaded." << std::endl;
}

void ContainerLoader::loadLegacyFormat(const json& j, const std::string& contName,
                                        const ModuleConfig& config) {
    if (j.contains("preload")) {
        loadModulesFromArray(j["preload"], contName, config);
    }
    if (j.contains("load_order")) {
        loadModulesFromArray(j["load_order"], contName, config);
    }
}

void ContainerLoader::loadActiveContainer() {
    std::string contName = getActiveContainerName();
    if (contName.empty()) {
        std::cout << "[ContainerLoader] No active container found in active_container.json\n";
        return;
    }

    std::string path = "container/" + contName + "/container.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[ContainerLoader] Failed to open: " << path << "\n";
        return;
    }

    json j;
    file >> j;

    std::cout << "[ContainerLoader] Loading container: " << contName << " from " << path << "\n";

    ModuleConfig config = makeConfig(contName);

    if (j.contains("stages")) {
        loadStaged(j, contName, config);
    } else {
        loadLegacyFormat(j, contName, config);
    }
}

void ContainerLoader::reloadCMD(FURCMDPacket& packet) {
    std::cout << "[ContainerLoader] Reloading active container...\n";
}
