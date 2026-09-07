#pragma once
#include <cstdint>

// Forward declarations — the SDK does not have access to engine-internal
// headers like FURCMD/FURCMD.h or KernelCommands.h. The full IKernel vtable
// (including kernel_call) is part of the stable SDK ABI so modules can be
// compiled against it without knowing the engine internals. vtable layout
// MUST stay in sync with engine core IKernel.h.
struct FURCMDPacket;
struct KernelCMDPacket;
typedef void (*FURMethod)(FURCMDPacket& packet);

// Module configuration passed to ModuleMain by the engine (canonical — single
// source of truth; vendored into every consumer as part of `fractalsdk`).
struct ModuleConfig {
    char containerRoot[1024];
    char userRoot[1024];
    char loadedModulePath[512];
};

class IKernel {
public:
    virtual ~IKernel() = default;

    virtual void kernel_call(KernelCMDPacket& packet) = 0;

    virtual void sendCMDPacket(FURCMDPacket& packet) = 0;
    virtual void registerCMDMethod(uint32_t hashId, FURMethod method) = 0;
    virtual void setModuleState(const char* modulePath, int state) = 0;

    // Load a module directly on the calling thread, bypassing the FURCMD queue.
    // This prevents deadlocks when ModuleMain sends synchronous FURCMD packets.
    virtual void loadModuleDirect(const char* path, const ModuleConfig& config) = 0;

    // Query module state directly on the calling thread, bypassing the FURCMD queue.
    // Returns the module state integer (0=loading, 1=SDK_Ready, 2=Running, 3=Failed).
    virtual int getModuleStateDirect(const char* path) = 0;
};
