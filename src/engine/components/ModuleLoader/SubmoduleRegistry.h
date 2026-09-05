#pragma once
#include <SDK/archetyped/hash/hash.h>
#include "core/IKernel.h"
#include "components/ModuleLoader/ModuleInstance.h"
#include "ankerl/unordered_dense.h"
#include <string>
#include <memory>
#include <mutex>
#include <chrono>
#include <thread>

struct ModuleConfig;

class SubmoduleRegistry {
public:
    SubmoduleRegistry(IKernel* kernel, ModuleConfig baseConfig);

    bool load(const char* id, const char* path);
    bool load(const char* id, const char* path, ModuleConfig config);
    bool unload(const char* id);
    void unloadAll();
    ModuleInstance* get(const char* id);
    int getState(const char* id);
    bool waitForState(const char* id, int minState, int timeoutMs = 10000);

private:
    struct SubmoduleEntry {
        std::string id;
        std::string path;
        uint32_t hash;
    };

    ankerl::unordered_dense::map<uint32_t, std::unique_ptr<SubmoduleEntry>> m_modules;
    IKernel* m_kernel;
    ModuleConfig m_baseConfig;
    std::mutex m_mutex;
};
