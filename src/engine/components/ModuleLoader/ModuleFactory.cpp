#include "ModuleFactory.h"
#include "components/ModuleLoader/ModuleInstance.h"
#include <iostream>
#include <cstring>

ankerl::unordered_dense::map<uint32_t, std::unique_ptr<ModuleInstance>, IdentityHash> ModuleFactory::modules;
std::mutex ModuleFactory::modulesMutex;

ModuleFactory::ModuleFactory() {
    FractalKernel::instance().registerCMDMethod(loadModuleHash, &loadModule);
    FractalKernel::instance().registerCMDMethod(isLoadedHash, &isLoadedCMD);
    FractalKernel::instance().registerCMDMethod(setModuleStateHash, &setModuleStateCMD);
    FractalKernel::instance().registerCMDMethod(getModuleStateHash, &getModuleStateCMD);
}

void ModuleFactory::loadModule(FURCMDPacket& packet) {
    auto context = static_cast<loadModuleContext*>(packet.payload);
    if (!context || context->path[0] == '\0') {
        fprintf(stderr, "[ModuleFactory::loadModule] INVALID packet (null or empty path)\n");
        return;
    }
    
    std::string pathCopy(context->path);
    uint32_t moduleHash = fnv1aHash(pathCopy.c_str());
    fprintf(stderr, "[ModuleFactory::loadModule] path='%s' hash=0x%08x\n", pathCopy.c_str(), moduleHash);
    
    ModuleInstance* modPtr = nullptr;
    {
        std::lock_guard<std::mutex> lock(modulesMutex);
        if (modules.contains(moduleHash)) {
            std::cout << "[ModuleFactory] ModuleInstance already loaded: " << pathCopy << std::endl;
            return;
        }

        auto module = std::make_unique<ModuleInstance>();
        if (!module->load(pathCopy.c_str())) {
            std::cerr << "[ModuleFactory] dlopen error for: " << pathCopy << std::endl;
            return;
        }

        modPtr = module.get();
        modules[moduleHash] = std::move(module);
    }

    // Call ModuleMain WITHOUT holding modulesMutex — FURCMD worker needs it
    // for setModuleStateCMD / getModuleStateCMD (e.g. kernel->setModuleState
    // inside ModuleMain must not deadlock).
    modPtr->callMain(&FractalKernel::instance(), context->config);

    {
        std::lock_guard<std::mutex> lock(modulesMutex);
        fprintf(stderr, "[ModuleFactory::loadModule] post-callMain state check for hash=0x%08x\n", moduleHash);
        // Legacy fallback: promote state if module didn't self-report
        if (modules[moduleHash]->getState() == ModuleInstance::Loading) {
            fprintf(stderr, "[ModuleFactory::loadModule] Legacy fallback: promoting to SDK_Ready\n");
            modules[moduleHash]->setState(ModuleInstance::SDK_Ready);
        }

        std::cout << "[ModuleFactory] Successfully loaded: " << pathCopy << std::endl;
    }
}

ModuleInstance* ModuleFactory::findModule(const char* path) {
    if (!path || path[0] == '\0') return nullptr;
    uint32_t hash = fnv1aHash(path);
    std::lock_guard<std::mutex> lock(modulesMutex);
    auto it = modules.find(hash);
    if (it == modules.end()) {
        fprintf(stderr, "[ModuleFactory::findModule] MODULE NOT FOUND: '%s' hash=0x%08x\n", path, hash);
        return nullptr;
    }
    return it->second.get();
}

ISubmoduleHost* ModuleFactory::probeHost(const char* path) {
    auto* mod = findModule(path);
    if (!mod) return nullptr;
    auto* getHost = (ISubmoduleHost*(*)())mod->getSymbol("GetHostInstance");
    return getHost ? getHost() : nullptr;
}

void ModuleFactory::isLoadedCMD(FURCMDPacket& packet) {
    auto context = static_cast<isLoadedCMDContext*>(packet.payload);
    if (!context || !packet.outputBuffer) return;
    
    uint32_t moduleHash = fnv1aHash(context->path);
    
    std::lock_guard<std::mutex> lock(modulesMutex);
    bool loaded = modules.contains(moduleHash);
    
    std::memcpy(packet.outputBuffer, &loaded, sizeof(bool));
}

void ModuleFactory::setModuleStateCMD(FURCMDPacket& packet) {
    auto context = static_cast<setModuleStateContext*>(packet.payload);
    if (!context || context->path[0] == '\0') return;
    
    uint32_t moduleHash = fnv1aHash(context->path);
    std::lock_guard<std::mutex> lock(modulesMutex);
    auto it = modules.find(moduleHash);
    if (it != modules.end()) {
        it->second->setState(context->state);
    }
}

void ModuleFactory::getModuleStateCMD(FURCMDPacket& packet) {
    auto context = static_cast<getModuleStateContext*>(packet.payload);
    if (!context || context->path[0] == '\0') return;
    
    uint32_t moduleHash = fnv1aHash(context->path);
    
    std::lock_guard<std::mutex> lock(modulesMutex);
    auto it = modules.find(moduleHash);
    if (it != modules.end()) {
        if (packet.outputBuffer) {
            int state = it->second->getState();
            std::memcpy(packet.outputBuffer, &state, sizeof(int));
        }
    } else {
        if (packet.outputBuffer) {
            int notFound = -1;
            std::memcpy(packet.outputBuffer, &notFound, sizeof(int));
        }
    }
}