#pragma once
#ifndef MODULE_H
#define MODULE_H

#ifdef _WIN32
    #include <windows.h>
    using LibHandle = HMODULE;
#else
    #include <dlfcn.h>
    #include <iostream>
    using LibHandle = void*;
#endif

#include "core/IKernel.h"
#include <atomic>

typedef void (*ModuleEntry)(IKernel* kernel, ModuleConfig config);

class ModuleInstance {
public:
    enum State : int {
        Loading   = 0,
        SDK_Ready = 1,
        Running   = 2,
        Error     = 3
    };

    ModuleInstance() = default;
    ~ModuleInstance() { unload(); }

    bool load(const char* path) {
        if (!path || path[0] == '\0') return false;
#ifdef _WIN32
        handle = LoadLibraryA(path);
#else
        handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
#endif
        return handle != nullptr;
    }

    bool callMain(IKernel* kernel, ModuleConfig config) const {
        auto* mainFunc = (ModuleEntry)getSymbol("ModuleMain");
        if (!mainFunc) return false;
        mainFunc(kernel, config);
        return true;
    }

    bool isLoaded() const { return handle != nullptr; }

    void setState(int s) { m_state.store(s); }
    int getState() const { return m_state.load(); }

    void* getSymbol(const char* name) const {
        if (!handle) return nullptr;
#ifdef _WIN32
        return (void*)GetProcAddress(handle, name);
#else
        return dlsym(handle, name);
#endif
    }

private:
    LibHandle handle = nullptr;
    std::atomic<int> m_state{Loading};

    void unload() {
        if (handle) {
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            handle = nullptr;
        }
    }
};

#endif
