#include "FractalKernel.h"
#include "KernelCommands.h"
#include "components/ModuleLoader/ModuleFactory.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>

FractalKernel& FractalKernel::instance() {
    static FractalKernel instance;
    return instance;
}

FractalKernel* FractalKernel::self() {
    return &instance();
}

void FractalKernel::kernel_call(KernelCMDPacket &packet) {
    KernelCMDMgr->kernel_call(packet);
}

void FractalKernel::kernel_register(KernelCMD cmd, uint8_t cmdId) {
    KernelCMDMgr->kernel_register(cmd, cmdId);
}

void FractalKernel::sendCMDPacket(FURCMDPacket& packet) {
    if (FURCMDManager) {
        FURCMDManager->invoke(packet);
    } else {
        throw std::runtime_error("FURCMDManager not initialized");
    }
}

void FractalKernel::registerCMDMethod(uint32_t hashId, FURMethod method) {
    if (FURCMDManager) {
        FURCMDManager->registerFURMethod(hashId, method);
    } else {
        throw std::runtime_error("FURCMDManager not initialized");
    }
}

void FractalKernel::init() {
    if (!FURCMDManager) {
        FURCMDManager = std::make_unique<FURCommandManager>();
    }
}

void FractalKernel::shutdown() {
    if (FURCMDManager) {
        FURCMDManager->stop();
    }
}

void FractalKernel::setModuleState(const char* modulePath, int state) {
    if (!modulePath || modulePath[0] == '\0') {
        return;
    }

    setModuleStateContext ctx;
    std::strncpy(ctx.path, modulePath, sizeof(ctx.path) - 1);
    ctx.path[sizeof(ctx.path) - 1] = '\0';
    ctx.state = state;

    FURCMDPacket packet;
    packet.methodHash = setModuleStateHash;
    packet.payload = &ctx;
    packet.payloadSize = sizeof(setModuleStateContext);
    sendCMDPacket(packet);
}

void FractalKernel::loadModuleDirect(const char* path, const ModuleConfig& config) {
    if (!path || path[0] == '\0') return;

    loadModuleContext ctx;
    std::strncpy(ctx.path, path, sizeof(ctx.path) - 1);
    ctx.path[sizeof(ctx.path) - 1] = '\0';
    ctx.config = config;

    FURCMDPacket packet;
    packet.methodHash = loadModuleHash;
    packet.payload = &ctx;
    packet.payloadSize = sizeof(loadModuleContext);

    ModuleFactory::loadModule(packet);
}

int FractalKernel::getModuleStateDirect(const char* path) {
    if (!path || path[0] == '\0') return 0;

    ModuleInstance* mod = ModuleFactory::findModule(path);
    if (!mod) return 0;

    switch (mod->getState()) {
        case ModuleInstance::Loading:  return 0;
        case ModuleInstance::SDK_Ready: return 1;
        case ModuleInstance::Running:  return 2;
        case ModuleInstance::Error:    return 3;
        default: return 0;
    }
}
