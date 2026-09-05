#include "SubmoduleRegistry.h"
#include "components/ModuleLoader/ModuleFactory.h"
#include "core/FractalKernel.h"
#include <cstring>
#include <iostream>

SubmoduleRegistry::SubmoduleRegistry(IKernel* kernel, ModuleConfig baseConfig)
    : m_kernel(kernel)
    , m_baseConfig(baseConfig)
{
}

bool SubmoduleRegistry::load(const char* id, const char* path) {
    return load(id, path, m_baseConfig);
}

bool SubmoduleRegistry::load(const char* id, const char* path, ModuleConfig config) {
    if (!id || !path) return false;

    uint32_t hash = fnv1aHash(path);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_modules.contains(hash)) {
            std::cerr << "[SubmoduleRegistry] Submodule already loaded: " << id << std::endl;
            return false;
        }
    }

    loadModuleContext loadCtx;
    std::strncpy(loadCtx.path, path, sizeof(loadCtx.path) - 1);
    loadCtx.path[sizeof(loadCtx.path) - 1] = '\0';
    loadCtx.config = config;
    std::strncpy(loadCtx.config.loadedModulePath, path, sizeof(loadCtx.config.loadedModulePath) - 1);

    FURCMDPacket loadPacket;
    loadPacket.methodHash = loadModuleHash;
    loadPacket.payload = &loadCtx;
    loadPacket.payloadSize = sizeof(loadModuleContext);
    m_kernel->sendCMDPacket(loadPacket);

    auto entry = std::make_unique<SubmoduleEntry>();
    entry->id = id;
    entry->path = path;
    entry->hash = hash;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_modules[hash] = std::move(entry);
    }

    if (!waitForState(id, ModuleInstance::SDK_Ready)) {
        std::cerr << "[SubmoduleRegistry] Submodule " << id << " failed to reach SDK_Ready" << std::endl;
        return false;
    }

    std::cout << "[SubmoduleRegistry] Submodule " << id << " loaded and ready." << std::endl;
    return true;
}

bool SubmoduleRegistry::unload(const char* id) {
    if (!id) return false;
    uint32_t hash = fnv1aHash(id);

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_modules.find(hash);
    if (it == m_modules.end()) return false;

    // TODO: send unload to ModuleFactory when supported
    m_modules.erase(it);
    return true;
}

void SubmoduleRegistry::unloadAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    // TODO: send unloadAll to ModuleFactory when supported
    m_modules.clear();
}

ModuleInstance* SubmoduleRegistry::get(const char* id) {
    if (!id) return nullptr;
    uint32_t hash = fnv1aHash(id);

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_modules.find(hash);
    if (it == m_modules.end()) return nullptr;

    // ModuleInstance is owned by ModuleFactory, SubmoduleRegistry tracks by hash only
    // For direct access, query ModuleFactory's map
    // For now return nullptr — getSubmodule is used for iteration/inspection
    return nullptr;
}

int SubmoduleRegistry::getState(const char* id) {
    if (!id) return -1;

    auto* entry = [&]() -> SubmoduleEntry* {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_modules.find(fnv1aHash(id));
        return it != m_modules.end() ? it->second.get() : nullptr;
    }();

    if (!entry) return -1;

    getModuleStateContext stateCtx;
    std::strncpy(stateCtx.path, entry->path.c_str(), sizeof(stateCtx.path) - 1);
    stateCtx.path[sizeof(stateCtx.path) - 1] = '\0';

    FURCMDPacket statePacket;
    statePacket.methodHash = getModuleStateHash;
    statePacket.payload = &stateCtx;
    statePacket.payloadSize = sizeof(getModuleStateContext);

    char outputBuf[sizeof(int)];
    statePacket.outputBuffer = outputBuf;

    uint64_t stateFence = 0;
    statePacket.fence = &stateFence;

    m_kernel->sendCMDPacket(statePacket);

    while (stateFence != 1) std::this_thread::yield();

    int state = 0;
    std::memcpy(&state, outputBuf, sizeof(int));
    return state;
}

bool SubmoduleRegistry::waitForState(const char* id, int minState, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        int state = getState(id);
        if (state >= minState) return true;
        if (state == ModuleInstance::Error) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cerr << "[SubmoduleRegistry] Timeout waiting for " << id << " to reach state " << minState << std::endl;
    return false;
}
