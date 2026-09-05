#pragma once
#include "FURCMD/FURCMD.h"
#include "ModuleInstance.h"
#include <SDK/archetyped/hash/hash.h>
#include "Engine/Engine.hpp"
#include "core/FractalKernel.h"
#include "ankerl/unordered_dense.h"
#include <memory>
#include <mutex>
#include <thread>

#include "core/IKernel.h"
#include "core/ISubmoduleHost.h"

struct loadModuleContext {
    char path[512];
    ModuleConfig config;
};
struct isLoadedCMDContext {
    char path[512];
};
struct setModuleStateContext {
    char path[512];
    int state;
};
struct getModuleStateContext {
    char path[512];
};

/**
 * ModuleFactory (ModuleInstance Loader) - Core Component.
 * Responsible for loading, unloading, and managing the state of engine modules.
 */
constexpr uint32_t loadModuleHash = fnv1aHashConst("archetyped:module_loader:loadModule");
constexpr uint32_t isLoadedHash = fnv1aHashConst("archetyped:module_loader:isLoaded");
constexpr uint32_t setModuleStateHash = fnv1aHashConst("archetyped:module_loader:setState");
constexpr uint32_t getModuleStateHash = fnv1aHashConst("archetyped:module_loader:getState");

class ModuleFactory {
public:
    ModuleFactory();
    ~ModuleFactory() = default;

    /**
     * Loads a module from the specified path.
     * 
     * @param packet Command packet containing the module path and configuration.
     * @return void
     */
    static void loadModule(FURCMDPacket& packet);

    /**
     * Checks if a module is currently loaded.
     * 
     * @param packet Command packet containing the module path.
     * @return void
     */
    static void isLoadedCMD(FURCMDPacket& packet);

    /**
     * Sets the state of a specific module.
     * 
     * @param packet Command packet containing the module path and the target state.
     * @return void
     */
    static void setModuleStateCMD(FURCMDPacket& packet);

    /**
     * Retrieves the current state of a specific module.
     * 
     * @param packet Command packet containing the module path.
     * @return void
     */
    static void getModuleStateCMD(FURCMDPacket& packet);
    static ModuleInstance* findModule(const char* path);
    static ISubmoduleHost* probeHost(const char* path);

private:
    static ankerl::unordered_dense::map<uint32_t, std::unique_ptr<ModuleInstance>, IdentityHash> modules;
    static std::mutex modulesMutex;
};
