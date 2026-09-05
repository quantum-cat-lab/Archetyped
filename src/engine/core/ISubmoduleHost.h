#pragma once
#include "core/IKernel.h"
#include "components/ModuleLoader/ModuleInstance.h"
#include <functional>

struct ModuleConfig;

class ISubmoduleHost {
public:
    virtual ~ISubmoduleHost() = default;

    virtual bool loadSubmodule(const char* moduleId, ModuleConfig config) = 0;
    virtual bool unloadSubmodule(const char* moduleId) = 0;
    virtual ModuleInstance* getSubmodule(const char* moduleId) = 0;
    virtual void forEachSubmodule(std::function<void(ModuleInstance&)>) = 0;
    virtual const char* getModuleId() const = 0;
    virtual void finalizeInit() = 0;
};
