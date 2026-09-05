#pragma once
#ifndef FRACTAL_KERNEL
#define FRACTAL_KERNEL

#include <cstdint>
#include <memory>
#include "FURCMD/FURCMD.h"
#include "IKernel.h"
#include <stdint.h>
#include "KernelCommands.h"
class FractalKernel : public IKernel {
public:
    static FractalKernel& instance();

    FractalKernel* self();

    void kernel_call(KernelCMDPacket &packet);
    void kernel_register(KernelCMD cmd, uint8_t cmdId);
    void sendCMDPacket(FURCMDPacket& packet) override;
    void registerCMDMethod(uint32_t hashId, FURMethod method) override;
    void setModuleState(const char* modulePath, int state) override;
    void loadModuleDirect(const char* path, const ModuleConfig& config) override;
    int getModuleStateDirect(const char* path) override;

    void init();

    void shutdown();

private:
    std::unique_ptr<FURCommandManager> FURCMDManager;
    std::unique_ptr<KernelCMDManager> KernelCMDMgr;
};

#endif // !FRACTAL_KERNEL
